#include "buffer.hpp"
#include "catalog.hpp"
#include "common.hpp"
#include "os.hpp"
#include "page.hpp"

#include <array>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <list>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>

namespace buffer
{
constexpr FrameId kFrameCount{1 << 5}; // TODO

struct Id
{
    catalog::FileId file_id;
    page::Id        page_id;
    bool            operator==(const Id& other) const
    {
        return file_id == other.file_id && page_id == other.page_id;
    }
};

struct IdHash
{
    std::size_t operator()(const Id& id) const
    {
        static constexpr std::size_t kBitOffset = 32;
        return (static_cast<std::size_t>(id.file_id.Get()) << kBitOffset) | id.page_id.Get();
    }
};

struct FrameInfo
{
    unsigned int      pins;
    bool              dirty;
    std::optional<Id> id;
};

static std::unique_ptr<std::array<FrameInfo, kFrameCount.Get()>> frame_infos;
static Buffer                                                    frames{kFrameCount};

static std::unordered_map<Id, FrameId, IdHash> ids_used;

static std::list<FrameId>                                        free_list;
static std::unordered_map<FrameId, std::list<FrameId>::iterator> free_list_iters;

static std::unordered_map<catalog::FileId, std::string> file_name_cache; // TODO: limit cache
static const std::string& GetFileName(catalog::FileId file_id, bool assert_cached)
{
    const auto iter = file_name_cache.find(file_id);
    if (iter != file_name_cache.end())
    {
        return iter->second;
    }
    ASSERT(!assert_cached);
    auto tmp                        = catalog::GetFileName(file_id);
    return file_name_cache[file_id] = std::move(tmp);
}

static void InputFrame(FrameId frame, Id id, bool append)
{
    ASSERT(frame < kFrameCount);
    FrameInfo& frame_info = (*frame_infos)[frame.Get()];
    ASSERT(frame_info.pins == 0);
    ASSERT(frame_info.dirty == false);
    ASSERT(!frame_info.id);

    const os::File file{GetFileName(id.file_id, false)};

    if (append)
    {
        frame_info.dirty = true;
    }
    else
    {
        void* const src = frames.GetFrame(frame);
        file.Read(id.page_id, src);
    }
    frame_info.id = id;
    ASSERT(!ids_used.contains(id));
    ids_used[id] = frame;
}

static void OuputFrame(FrameId frame)
{
    ASSERT(frame < kFrameCount);
    FrameInfo& frame_info = (*frame_infos)[frame.Get()];
    ASSERT(frame_info.pins == 0);
    if (frame_info.id)
    {
        const Id id = *frame_info.id;
        if (frame_info.dirty)
        {
            const os::File    file{GetFileName(id.file_id, true)};
            const void* const dst = frames.GetFrame(frame);
            file.Write(id.page_id, dst);
            frame_info.dirty = false;
        }
        frame_info.id = std::nullopt;
        ASSERT(ids_used.contains(id));
        ids_used.erase(id);
    }
}

static void PinFrame(FrameId frame)
{
    ASSERT(frame < kFrameCount);
    FrameInfo& frame_info = (*frame_infos)[frame.Get()];
    ASSERT(frame_info.id);
    if (frame_info.pins == 0)
    {
        // TODO: slow asserts
        // ASSERT(std::find(free_list.cbegin(), free_list.cend(), frame) != free_list.cend());
        // ASSERT_T(free_list_iters.contains(frame)); // TODO
        const auto iter = free_list_iters.at(frame);
        free_list.erase(iter);
        free_list_iters.erase(frame); // TODO: optimize: cache access by key 'frame'
    }
    frame_info.pins++;
}

static void UnpinFrame(FrameId frame, bool dirty)
{
    ASSERT(frame < kFrameCount);
    FrameInfo& frame_info = (*frame_infos)[frame.Get()];
    ASSERT(frame_info.pins > 0);
    ASSERT(frame_info.id);
    frame_info.pins--;
    frame_info.dirty = frame_info.dirty || dirty;
    if (frame_info.pins == 0)
    {
        // TODO: slow asserts
        // ASSERT(std::find(free_list.cbegin(), free_list.cend(), frame) == free_list.cend());
        // ASSERT_T(!free_list_iters.contains(frame));
        free_list.push_back(frame);
        free_list_iters[frame] = --free_list.end();
    }
}

void Init()
{
    ids_used.clear();
    free_list.clear();
    free_list_iters.clear();
    file_name_cache.clear(); // TODO
    frame_infos = std::make_unique<std::array<FrameInfo, kFrameCount.Get()>>();
    for (FrameId frame{}; frame < kFrameCount; frame++)
    {
        free_list.push_back(frame);
        free_list_iters[frame] = --free_list.end();
    }
}

void Destroy()
{
    for (FrameId frame{}; frame < kFrameCount; frame++)
    {
        OuputFrame(frame);
    }
}

void Flush(catalog::FileId file_id)
{
    for (FrameId frame{}; frame < kFrameCount; frame++)
    {
        FrameInfo& info = (*frame_infos)[frame.Get()];
        if (info.id && info.id->file_id == file_id)
        {
            OuputFrame(frame);
        }
    }
    file_name_cache.erase(file_id);
}

void* Request(catalog::FileId file_id, page::Id page_id, bool append, FrameId& frame_out)
{
    const Id   id   = {.file_id = file_id, .page_id = page_id};
    const auto iter = ids_used.find(id);
    if (iter == ids_used.end())
    {
        ASSERT(!free_list.empty());
        frame_out = free_list.front();
        OuputFrame(frame_out);
        InputFrame(frame_out, id, append);
        // TODO: optimize: in/out both access ids_used, do once
    }
    else
    {
        frame_out = iter->second;
    }
    PinFrame(frame_out);
    return frames.GetFrame(frame_out);
}

void Release(FrameId frame, bool dirty)
{
    ASSERT(frame < kFrameCount);
    const FrameInfo& frame_info = (*frame_infos)[frame.Get()];
    ASSERT(frame_info.id);
    UnpinFrame(frame, dirty);
}

// TODO: move to tests folder
/*void test()
{
    const unsigned int seed = os::random();
    srand(seed);

    std::printf("testing buffer\n");

    init();
    catalog::init();

    const catalog::FileId file_id = catalog::get_table_file_ids(catalog::TableId{}).dat;
    std::vector<char>     file_data;

    {
        std::unordered_map<page::Id, unsigned int> pins_count;
        std::vector<Pin<const char>>                     pins;

        static constexpr unsigned int page_count = 5'000;
        for (unsigned int page_count = 0; page_count < page_count; page_count++)
        {

            {
                const page::Id page_id_append{page_count};
                Pin<const char>      page_append{file_id, page_id_append, true};
                const auto     c_append = static_cast<char>('A' + (rand() % ('Z' - 'A' + 1)));
                memset(page_append.get_page(), c_append, page::kSize);
                file_data.push_back(c_append);
            }

            static constexpr unsigned int iteration_count = 10;
            for (unsigned int j = 0; j < iteration_count; j++)
            {

                const page::Id page_id_set(rand() % (page_count + 1));
                Pin<const char>      page_set{file_id, page_id_set};
                const auto     c_set = static_cast<char>('A' + (rand() % ('Z' - 'A' + 1)));
                memset(page_set.get_page(), c_set, page::kSize);
                file_data.at(page_id_set.get()) = c_set;

                if (rand() % (j + 1) == 0)
                {
                    while (pins_count.size() + 1 >= kFrameCount.get())
                    {
                        const unsigned index   = rand() % pins.size();
                        const page::Id page_id = pins[index].get_page_id();
                        pins.erase(pins.begin() + index);
                        if (--pins_count[page_id] == 0)
                        {
                            pins_count.erase(page_id);
                        }
                    }
                    pins.push_back(std::move(page_set));
                    pins_count[page_id_set]++;
                }
            }
        }
    }

    destroy();

    const os::File file{GetFileName(file_id, false)};
    for (page::Id i{}; i < file_data.size(); i++)
    {
        std::array<char, page::kSize> frame;
        file.read(i, frame.data());
        ASSERT(frame[0] == file_data[i.get()]);
    }

    std::printf("testing buffer done\n");
}*/

} // namespace buffer
