#pragma once

#include "catalog.hpp"

namespace buffer
{
	struct FrameTag {};
	using FrameId = StrongId<FrameTag, u32>;

	inline constexpr FrameId FRAME_COUNT { 1 << 6 }; // TODO

	void init();
	void destroy();
	void flush(catalog::FileId file);

	void *request(catalog::FileId file, PageId page_id, bool append, FrameId &frame_out);
	void release(FrameId frame, bool dirty);

	template <typename PageT>
	class Pin
	{
	public:

		Pin() : frame {}, page_id {}, page {} {}

		Pin(catalog::FileId file, PageId page_id, bool append = false)
			: page_id { page_id }
			, page { reinterpret_cast<PageT *>(request(file, page_id, append, frame)) }
		{
		}

		Pin(const Pin &) = delete;
		Pin &operator=(const Pin &) = delete;

		Pin(Pin &&other)
		{
			frame = other.frame;
			page_id = other.page_id;
			page = other.page;
			other.frame = {};
			other.page_id = {};
			other.page = {};
		}

		Pin &operator=(Pin &&other)
		{
			release();
			frame = other.frame;
			page_id = other.page_id;
			page = other.page;
			other.frame = {};
			other.page_id = {};
			other.page = {};
			return *this;
		}

		~Pin()
		{
			release();
		}

		inline PageId get_page_id() const { return page_id; }
		inline PageT *get_page() const { return page; }
		inline PageT *operator->() const { return page; }

	private:

		void release()
		{
			if (page) {
				buffer::release(frame, !std::is_const_v<PageT>);
			}
		}

		FrameId frame;
		PageId page_id;
		PageT *page;
	};

	template <typename PageT = void>
	class Buffer
	{
	public:

		Buffer(FrameId frame_count = FrameId { 1 })
			: frame_count { frame_count }
			, buffer { std::aligned_alloc(PAGE_SIZE.get(), frame_count.get() * PAGE_SIZE.get()) }
		{
			ASSERT(buffer);
			memset(buffer, 0, frame_count.get() * PAGE_SIZE.get()); // TODO: remove
		}

		~Buffer()
		{
			std::free(buffer);
		}

		inline void *get_frame(FrameId frame)
		{
			ASSERT(frame < frame_count);
			return reinterpret_cast<char *>(buffer) + frame.get() * PAGE_SIZE.get();
		}

		inline PageT *get() const { return reinterpret_cast<PageT *>(buffer); }
		inline PageT *operator->() const { return reinterpret_cast<PageT *>(buffer); }

	private:

		const FrameId frame_count;
		void * const buffer;
	};

	void test();

}
