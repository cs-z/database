#pragma once

#include "catalog.hpp"
#include "common.hpp"
#include "page.hpp"

#include <cstddef>
#include <cstdlib>
#include <cstring>

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

template <typename PageT> class Pin
{
public:
    Pin() : file_id_{}, frame_{}, page_id_{}, page_{}
    {
    }

    Pin(catalog::FileId file_id, page::Id page_id, bool append = false)
        : file_id_{file_id}, page_id_{page_id},
          page_{reinterpret_cast<PageT*>(Request(file_id, page_id, append, frame_))}
    {
    }

    operator bool() const
    {
        return page_ != nullptr;
    }

    Pin(const Pin&)            = delete;
    Pin& operator=(const Pin&) = delete;

    template <typename PageOtherT = PageT> Pin(Pin<PageOtherT>&& other)
    {
        file_id_       = other.file_id_;
        frame_         = other.frame_;
        page_id_       = other.page_id_;
        page_          = reinterpret_cast<PageT*>(other.page_);
        other.file_id_ = {};
        other.frame_   = {};
        other.page_id_ = {};
        other.page_    = {};
    }

    template <typename PageOtherT = PageT> Pin& operator=(Pin<PageOtherT>&& other)
    {
        Release();
        file_id_       = other.file_id_;
        frame_         = other.frame_;
        page_id_       = other.page_id_;
        page_          = reinterpret_cast<PageT*>(other.page_);
        other.file_id_ = {};
        other.frame_   = {};
        other.page_id_ = {};
        other.page_    = {};
        return *this;
    }

    ~Pin()
    {
        Release();
    }

    // create new pin using the same file
    template <typename PageResultT = PageT>
    Pin<PageResultT> Shift(page::Id page_id, bool append = false) const
    {
        return {file_id_, page_id, append};
    }

    [[nodiscard]] catalog::FileId GetFileId() const
    {
        return file_id_;
    }
    [[nodiscard]] page::Id GetPageId() const
    {
        return page_id_;
    }
    [[nodiscard]] PageT* GetPage() const
    {
        return page_;
    }
    [[nodiscard]] PageT* operator->() const
    {
        return page_;
    }

private:
    void Release()
    {
        if (page_)
        {
            buffer::Release(frame_, !std::is_const_v<PageT>);
        }
    }

    catalog::FileId file_id_;
    FrameId         frame_;
    page::Id        page_id_;
    PageT*          page_;

    template <typename> friend class Pin;
};

template <typename PageT = void> class Buffer
{
public:
    Buffer(FrameId frame_count = FrameId{1})
        : frame_count_{frame_count},
          buffer_{std::aligned_alloc(page::kSize,
                                     static_cast<std::size_t>(frame_count.Get()) * page::kSize)}
    {
        ASSERT(buffer_);
        memset(buffer_, 0,
               static_cast<std::size_t>(frame_count.Get()) * page::kSize); // TODO: remove
    }

    ~Buffer()
    {
        std::free(buffer_);
    }

    void* GetFrame(FrameId frame)
    {
        ASSERT(frame < frame_count_);
        return reinterpret_cast<char*>(buffer_) +
               static_cast<std::size_t>(frame.Get() * page::kSize);
    }

    [[nodiscard]] PageT* Get() const
    {
        return reinterpret_cast<PageT*>(buffer_);
    }
    PageT* operator->() const
    {
        return reinterpret_cast<PageT*>(buffer_);
    }

private:
    const FrameId frame_count_;
    void* const   buffer_;
};

} // namespace buffer
