#pragma once

#include "catalog.hpp"
#include "common.hpp"
#include "page.hpp"

#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <memory>

namespace buffer
{
struct FrameTag
{
};
using FrameId = StrongId<FrameTag, U32>;

void Init();
void Destroy();
void Flush(catalog::FileId file_id);

void* Request(catalog::FileId file_id, page::Id page_id, bool append, FrameId& frame_out);
void  Release(FrameId frame, bool dirty);

template <typename Page> class Pin
{
public:
    Pin() = default;

    Pin(catalog::FileId file_id, page::Id page_id, bool append = false)
        : file_id_{file_id}, page_id_{page_id},
          page_{reinterpret_cast<Page*>(Request(file_id, page_id, append, frame_))}
    {
    }

    Pin(const Pin&)            = delete;
    Pin& operator=(const Pin&) = delete;

    template <typename OtherPage>
    Pin(Pin<OtherPage>&& other) // NOLINT(google-explicit-constructor)
        : file_id_{other.file_id_}, frame_{other.frame_}, page_id_{other.page_id_},
          page_{reinterpret_cast<Page*>(other.page_)}
    {
        other.page_ = nullptr;
    }

    template <typename OtherPage> Pin& operator=(Pin<OtherPage>&& other) noexcept
    {
        if (this != &other)
        {
            Release();
            file_id_    = other.file_id_;
            frame_      = other.frame_;
            page_id_    = other.page_id_;
            page_       = reinterpret_cast<Page*>(other.page_);
            other.page_ = nullptr;
        }
        return *this;
    }

    template <typename OtherPage> [[nodiscard]] OtherPage* Cast() const
    {
        return reinterpret_cast<OtherPage*>(page_);
    }

    ~Pin()
    {
        Release();
    }

    // create new pin using the same file
    // template <typename OtherPage = Page>
    // Pin<OtherPage> Shift(page::Id page_id, bool append = false) const
    //{
    //    return {file_id_, page_id, append};
    //}

    [[nodiscard]] catalog::FileId GetFileId() const
    {
        return file_id_;
    }
    [[nodiscard]] page::Id GetPageId() const
    {
        return page_id_;
    }
    [[nodiscard]] Page* GetPage() const
    {
        return page_;
    }
    [[nodiscard]] Page* operator->() const
    {
        return page_;
    }

private:
    void Release()
    {
        if (page_)
        {
            buffer::Release(frame_, !std::is_const_v<Page>);
            page_ = nullptr;
        }
    }

    catalog::FileId file_id_{};
    FrameId         frame_{};
    page::Id        page_id_{};
    Page*           page_ = nullptr;

    template <typename> friend class Pin;
};

template <typename Page = void> class Buffer
{
public:
    explicit Buffer(FrameId frame_count = FrameId{1})
        : frame_count_{frame_count},
          buffer_{std::aligned_alloc(page::kSize,
                                     static_cast<std::size_t>(frame_count.Get()) * page::kSize),
                  &std::free}
    {
        ASSERT(buffer_);
        memset(buffer_.get(), 0,
               static_cast<std::size_t>(frame_count.Get()) * page::kSize); // TODO: remove
    }

    void* GetFrame(FrameId frame)
    {
        ASSERT(frame < frame_count_);
        return reinterpret_cast<char*>(buffer_.get()) +
               static_cast<std::size_t>(frame.Get() * page::kSize);
    }

    [[nodiscard]] Page* Get() const
    {
        return reinterpret_cast<Page*>(buffer_.get());
    }
    Page* operator->() const
    {
        return reinterpret_cast<Page*>(buffer_.get());
    }

private:
    FrameId                                frame_count_;
    std::unique_ptr<void, void (*)(void*)> buffer_;
};

} // namespace buffer
