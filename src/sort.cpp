#include "sort.hpp"
#include "buffer.hpp"
#include "common.hpp"
#include "iter.hpp"
#include "os.hpp"
#include "page.hpp"
#include "row.hpp"
#include "type.hpp"
#include "value.hpp"

#include <algorithm>
#include <array>
#include <cstring>
#include <iterator>
#include <memory>
#include <optional>
#include <utility>

// TODO: should temp files use buffer?
// TODO: don't create temp files for small sorts (like column lookup)

// K-way merge sort

static constexpr unsigned int kKWay = 2; // TODO

static bool CompareRows(const Type& type, const OrderBy& order_by, const U8* row_l, const U8* row_r)
{
    if (row_r == nullptr)
    {
        return true;
    }
    if (row_l == nullptr)
    {
        return false;
    }
    for (const OrderBy::Column& column : order_by.columns)
    {
        const int result = row::Compare(type, column.column_id, row_l, row_r);
        if (result < 0)
        {
            return column.asc;
        }
        if (result > 0)
        {
            return !column.asc;
        }
    }
    return true;
}

static void SortPage(const Type& type, const OrderBy& order_by, page::Slotted<>* page)
{
    // TODO: pipeline does not emit clang-tidy warning for ranges modernize
    std::sort(page->Begin(), page->End(),
              [&type, &order_by, page](const page::Slotted<>::Slot& slot_l,
                                       const page::Slotted<>::Slot& slot_r)
              {
                  const U8* entry_l = page->GetEntry(slot_l);
                  const U8* entry_r = page->GetEntry(slot_r);
                  return CompareRows(type, order_by, entry_l, entry_r);
              });
}

static std::optional<unsigned int> NextInput(const Type& type, const OrderBy& order_by,
                                             const std::array<const U8*, kKWay>& rows)
{
    const auto compare = [&type, &order_by](const U8* row_l, const U8* row_r)
    { return CompareRows(type, order_by, row_l, row_r); };
    const auto* iter = std::ranges::min_element(rows, compare);
    if (*iter == nullptr)
    {
        return std::nullopt;
    }
    return static_cast<unsigned int>(std::distance(rows.begin(), iter));
}

class Input
{
public:
    void Init(const os::TempFile& file, page::Id page_begin, page::Id page_end)
    {
        this->file_       = &file;
        this->page_begin_ = page_begin;
        this->page_end_   = page_end;

        page_id_  = page_begin;
        entry_id_ = page::EntryId{};
    }

    const U8* Next(page::Offset& size)
    {
        for (;;)
        {
            if (entry_id_ == 0)
            {
                if (page_id_ == page_end_)
                {
                    return nullptr;
                }
                file_->Read(page_id_, page_.Get());
            }
            if (entry_id_ == page_->GetEntryCount())
            {
                page_id_++;
                entry_id_ = page::EntryId{};
                continue;
            }
            const U8* const entry = page_->GetEntry(entry_id_++, size);
            if (entry == nullptr)
            {
                continue;
            }
            return entry;
        }
    }

private:
    buffer::Buffer<page::Slotted<>> page_;

    const os::TempFile* file_;
    page::Id            page_begin_, page_end_;

    page::Id      page_id_;
    page::EntryId entry_id_;
};

class Output
{
public:
    Output(const os::TempFile& file) : file_{file}, page_id_{}, page_id_begin_{}
    {
        page_->Init({});
    }

    void Append(const U8* row, page::Offset align, page::Offset size)
    {
        for (;;)
        {
            U8* const entry = page_->Insert(align, size, {});
            if (entry == nullptr)
            {
                Write();
                continue;
            }
            std::memcpy(entry, row, size);
            break;
        }
    }

    std::pair<page::Id, page::Id> EndSection()
    {
        Write();
        const page::Id begin = page_id_begin_;
        const page::Id end   = page_id_;
        page_id_begin_       = page_id_;
        return {begin, end};
    }

private:
    void Write()
    {
        if (page_->GetEntryCount() > 0)
        {
            file_.Write(page_id_++, page_.Get());
        }
        page_->Init({});
    }

    const os::TempFile&             file_;
    page::Id                        page_id_, page_id_begin_;
    buffer::Buffer<page::Slotted<>> page_;
};

// stores sections of data file, which will be merged together
// needed because variable-length rows
class SectionQueue
{
public:
    struct Section
    {
        page::Id begin, end;
    };

    [[nodiscard]] page::Id GetSize() const
    {
        return size_;
    }

    SectionQueue() : page_begin_{0}, page_r_{0}, page_w_{0}, entry_r_{0}, entry_w_{0}, size_{0}
    {
    }

    void Push(Section section)
    {
        size_++;
        ASSERT(entry_w_ < kSectionsPerPage);
        buffer_w_.Get()[(entry_w_++).Get()] = section;
        if (entry_w_ == kSectionsPerPage)
        {
            file_.Write(page::Id{page_w_++}, buffer_w_.Get());
            entry_w_ = page::EntryId{};
        }
    }

    [[nodiscard]] Section Pop()
    {
        size_--;
        if (entry_r_ == 0 || entry_r_ == kSectionsPerPage)
        {
            file_.Read(page_r_++, buffer_r_.Get());
            entry_r_ = page::EntryId{};
        }
        return buffer_r_.Get()[(entry_r_++).Get()];
    }

    void Flush()
    {
        if (entry_w_ > 0)
        {
            file_.Write(page_w_++, buffer_w_.Get());
            entry_w_ = page::EntryId{};
        }
        page_r_     = page_begin_;
        entry_r_    = page::EntryId{};
        page_begin_ = page_w_;
    }

private:
    static constexpr unsigned int kSectionsPerPage = page::kSize / sizeof(Section);

    const os::TempFile file_;

    page::Id      page_begin_;
    page::Id      page_r_, page_w_;
    page::EntryId entry_r_, entry_w_;

    page::Id size_;

    buffer::Buffer<Section> buffer_r_, buffer_w_;
};

static os::TempFile MergeSortedPages(const Type& type, const OrderBy& order_by, os::TempFile file,
                                     page::Id& page_count_out)
{
    os::TempFile file_src = std::move(file);
    os::TempFile file_dst;

    SectionQueue queue;
    for (page::Id page_id{}; page_id < page_count_out; page_id++)
    {
        queue.Push({.begin = page_id, .end = page_id + 1});
    }
    queue.Flush();

    std::array<Input, kKWay>        inputs;
    std::array<const U8*, kKWay>    rows;
    std::array<page::Offset, kKWay> sizes;

    const page::Offset align = type.GetAlign();

    while (queue.GetSize() > 1)
    {

        Output output{file_dst};

        const page::Id merges    = queue.GetSize() / kKWay;
        const page::Id remainder = queue.GetSize() % kKWay;

        for (page::Id i{}; i < merges; i++)
        {

            for (page::Id k{}; k < kKWay; k++)
            {
                const auto [begin, end] = queue.Pop();
                inputs[k.Get()].Init(file_src, begin, end);
                rows[k.Get()] = inputs[k.Get()].Next(sizes[k.Get()]);
            }

            std::optional<unsigned int> input_k = NextInput(type, order_by, rows);
            while (input_k)
            {
                const unsigned int k = *input_k;
                output.Append(rows[k], align, sizes[k]);
                rows[k] = inputs[k].Next(sizes[k]);
                input_k = NextInput(type, order_by, rows);
            }

            const auto [begin, end] = output.EndSection();
            queue.Push({.begin = begin, .end = end});
        }

        if (remainder > 0)
        {

            for (page::Id k{}; k < remainder; k++)
            {
                const auto [begin, end] = queue.Pop();
                inputs[k.Get()].Init(file_src, begin, end);
                rows[k.Get()] = inputs[k.Get()].Next(sizes[k.Get()]);
            }

            std::optional<unsigned int> input_k = NextInput(type, order_by, rows);
            while (input_k)
            {
                const unsigned int k = *input_k;
                ASSERT(k < remainder.Get()); // TODO: id / int
                output.Append(rows[k], align, sizes[k]);
                rows[k] = inputs[k].Next(sizes[k]);
                input_k = NextInput(type, order_by, rows);
            }

            const auto [begin, end] = output.EndSection();
            queue.Push({.begin = begin, .end = end});
        }

        queue.Flush();

        std::swap(file_src, file_dst);
    }

    ASSERT(queue.GetSize() == 1);
    const auto [begin, end] = queue.Pop();
    ASSERT(begin == 0);

    page_count_out = end;
    return file_src;
}

static os::TempFile MergeSort(Iter iter, const OrderBy& order_by, page::Id& page_count_out)
{
    os::TempFile file;

    // TODO: if parent is materialized, simply copy and sort pages

    page::Id                        page_id = page::Id{};
    buffer::Buffer<page::Slotted<>> page;
    page->Init({});

    const Type&        type  = iter->type;
    const page::Offset align = type.GetAlign();

    iter->Open();
    for (;;)
    {
        std::optional<Value> value = iter->Next();
        if (!value)
        {
            break;
        }
        const row::Prefix prefix = row::CalculateLayout(*value);

        for (;;)
        {
            U8* const entry = page->Insert(align, prefix.size, {});
            if (entry == nullptr)
            {
                SortPage(type, order_by, page.Get());
                file.Write(page_id++, page.Get());
                page->Init({});
                continue;
            }
            row::Write(prefix, *value, entry);
            break;
        }
    }
    iter->Close();

    if (page->GetEntryCount() > 0)
    {
        SortPage(type, order_by, page.Get());
        file.Write(page_id++, page.Get());
    }

    page_count_out = page_id;
    return MergeSortedPages(type, order_by, std::move(file), page_count_out);
}

void IterSort::Open()
{
    // puts("SORTING");
    ASSERT(!sorted_iter_);
    page::Id     page_count;
    os::TempFile file       = MergeSort(std::move(parent_), columns_, page_count);
    Type         type_clone = type; // TODO
    sorted_iter_ =
        std::make_unique<IterScanTemp>(std::move(file), page_count, std::move(type_clone));
    sorted_iter_->Open();
}

void IterSort::Restart()
{
    ASSERT(sorted_iter_);
    sorted_iter_->Restart();
}

void IterSort::Close()
{
    sorted_iter_->Close();
}

std::optional<Value> IterSort::Next()
{
    return sorted_iter_->Next();
}
