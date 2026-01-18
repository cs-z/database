#pragma once

#include <cstddef>

#include "page.hpp"
#include "catalog.hpp"

namespace buffer
{
	struct FrameTag {};
	using FrameId = StrongId<FrameTag, u32>;

	void init();
	void destroy();
	void flush(catalog::FileId file_id);

	void *request(catalog::FileId file_id, page::Id page_id, bool append, FrameId &frame_out);
	void release(FrameId frame, bool dirty);

	template <typename PageT>
	class Pin
	{
	public:

		Pin() : file_id {}, frame {}, page_id {}, page {} {}

		Pin(catalog::FileId file_id, page::Id page_id, bool append = false)
			: file_id { file_id }
			, page_id { page_id }
			, page { reinterpret_cast<PageT *>(request(file_id, page_id, append, frame)) }
		{
		}

		operator bool() const
		{
			return page != nullptr;
		}

		Pin(const Pin &) = delete;
		Pin &operator=(const Pin &) = delete;

		template <typename PageOtherT = PageT>
		Pin(Pin<PageOtherT> &&other)
		{
			file_id = other.file_id;
			frame = other.frame;
			page_id = other.page_id;
			page = reinterpret_cast<PageT *>(other.page);
			other.file_id = {};
			other.frame = {};
			other.page_id = {};
			other.page = {};
		}

		template <typename PageOtherT = PageT>
		Pin &operator=(Pin<PageOtherT> &&other)
		{
			release();
			file_id = other.file_id;
			frame = other.frame;
			page_id = other.page_id;
			page = reinterpret_cast<PageT *>(other.page);
			other.file_id = {};
			other.frame = {};
			other.page_id = {};
			other.page = {};
			return *this;
		}

		~Pin()
		{
			release();
		}

		// create new pin using the same file
		template <typename PageResultT = PageT>
		Pin<PageResultT> shift(page::Id page_id, bool append = false) const
		{
			return { file_id, page_id, append };
		}

		[[nodiscard]] catalog::FileId get_file_id() const { return file_id; }
		[[nodiscard]] page::Id get_page_id() const { return page_id; }
		[[nodiscard]] PageT *get_page() const { return page; }
		[[nodiscard]] PageT *operator->() const { return page; }

	private:

		void release()
		{
			if (page) {
				buffer::release(frame, !std::is_const_v<PageT>);
			}
		}

		catalog::FileId file_id;
		FrameId frame;
		page::Id page_id;
		PageT *page;

		template <typename>
		friend class Pin;
	};

	template <typename PageT = void>
	class Buffer
	{
	public:

		Buffer(FrameId frame_count = FrameId { 1 })
			: frame_count { frame_count }
			, buffer { std::aligned_alloc(page::SIZE, static_cast<size_t>(frame_count.get()) * page::SIZE) }
		{
			ASSERT(buffer);
			memset(buffer, 0, static_cast<size_t>(frame_count.get()) * page::SIZE); // TODO: remove
		}

		~Buffer()
		{
			std::free(buffer);
		}

		void *get_frame(FrameId frame)
		{
			ASSERT(frame < frame_count);
			return reinterpret_cast<char *>(buffer) + static_cast<size_t>(frame.get() * page::SIZE);
		}

		[[nodiscard]] PageT *get() const { return reinterpret_cast<PageT *>(buffer); }
		PageT *operator->() const { return reinterpret_cast<PageT *>(buffer); }

	private:

		const FrameId frame_count;
		void * const buffer;
	};

	void test();

}
