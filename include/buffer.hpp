#pragma once

#include "page.hpp"
#include "catalog.hpp"

namespace buffer
{
	struct FrameTag {};
	using FrameId = StrongId<FrameTag, u32>;

	void init();
	void destroy();
	void flush(catalog::FileId file);

	void *request(catalog::FileId file, page::Id page_id, bool append, FrameId &frame_out);
	void release(FrameId frame, bool dirty);

	template <typename PageT>
	class Pin
	{
	public:

		Pin() : file {}, frame {}, page_id {}, page {} {}

		Pin(catalog::FileId file, page::Id page_id, bool append = false)
			: file { file }
			, page_id { page_id }
			, page { reinterpret_cast<PageT *>(request(file, page_id, append, frame)) }
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
			file = other.file;
			frame = other.frame;
			page_id = other.page_id;
			page = reinterpret_cast<PageT *>(other.page);
			other.file = {};
			other.frame = {};
			other.page_id = {};
			other.page = {};
		}

		template <typename PageOtherT = PageT>
		Pin &operator=(Pin<PageOtherT> &&other)
		{
			release();
			file = other.file;
			frame = other.frame;
			page_id = other.page_id;
			page = reinterpret_cast<PageT *>(other.page);
			other.file = {};
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
		inline Pin<PageResultT> shift(page::Id page_id, bool append = false) const
		{
			return { file, page_id, append };
		}

		inline catalog::FileId get_file() const { return file; }
		inline page::Id get_page_id() const { return page_id; }
		inline PageT *get_page() const { return page; }
		inline PageT *operator->() const { return page; }

	private:

		void release()
		{
			if (page) {
				buffer::release(frame, !std::is_const_v<PageT>);
			}
		}

		catalog::FileId file;
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
			, buffer { std::aligned_alloc(page::SIZE, frame_count.get() * page::SIZE) }
		{
			ASSERT(buffer);
			memset(buffer, 0, frame_count.get() * page::SIZE); // TODO: remove
		}

		~Buffer()
		{
			std::free(buffer);
		}

		inline void *get_frame(FrameId frame)
		{
			ASSERT(frame < frame_count);
			return reinterpret_cast<char *>(buffer) + frame.get() * page::SIZE;
		}

		inline PageT *get() const { return reinterpret_cast<PageT *>(buffer); }
		inline PageT *operator->() const { return reinterpret_cast<PageT *>(buffer); }

	private:

		const FrameId frame_count;
		void * const buffer;
	};

	void test();

}
