#include "buffer.hpp"
#include "os.hpp"

#include <list>
#include <algorithm>

namespace buffer
{
	struct Id
	{
		catalog::FileId file;
		PageId page_id;
		bool operator==(const Id &other) const
		{
			return file == other.file && page_id == other.page_id;
		}
	};

	struct IdHash
	{
		size_t operator()(const Id &id) const
		{
			return (static_cast<size_t>(id.file.get()) << 32) | id.page_id.get();
		}
	};

	struct FrameInfo
	{
		unsigned int pins;
		bool dirty;
		std::optional<Id> id;
	};

	static std::unique_ptr<FrameInfo[]> frame_infos;
	static Buffer frames { FrameId { FRAME_COUNT } };

	static std::unordered_map<Id, FrameId,IdHash> ids_used;

	static std::list<FrameId> free_list;
	//static std::unique_ptr<std::list<FrameId>::iterator[]> free_list_iters;

	static std::unordered_map<catalog::FileId, std::string> name_cache;
	static std::unordered_map<catalog::FileId, os::Fd> fd_cache;

	static os::Fd get_fd(catalog::FileId file)
	{
		if (fd_cache.contains(file)) {
			return fd_cache.at(file);
		}
		const std::string name = name_cache.at(file);
		const os::Fd fd = os::file_open(name);
		return fd_cache[file] = fd;
	}

	static void input_frame(FrameId frame, Id id, bool append)
	{
		ASSERT(frame < FRAME_COUNT);
		FrameInfo &frame_info = frame_infos[frame.get()];
		ASSERT(frame_info.pins == 0);
		ASSERT(frame_info.dirty == false);
		ASSERT(!frame_info.id);
		if (append) {
			frame_info.dirty = true;
		}
		else {
			void * const src = frames.get_frame(frame);
			os::file_read(get_fd(id.file), id.page_id, src);
		}
		frame_info.id = id;
		ASSERT(!ids_used.contains(id));
		ids_used[id] = frame;
	}

	static void ouput_frame(FrameId frame)
	{
		ASSERT(frame < FRAME_COUNT);
		FrameInfo &frame_info = frame_infos[frame.get()];
		ASSERT(frame_info.pins == 0);
		if (frame_info.id) {
			const Id id = *frame_info.id;
			if (frame_info.dirty) {
				const void * const dst = frames.get_frame(frame);
				os::file_write(get_fd(id.file), id.page_id, dst);
				frame_info.dirty = false;
			}
			frame_info.id = std::nullopt;
			ASSERT(ids_used.contains(id));
			ids_used.erase(id);
		}
	}

	static void pin(FrameId frame)
	{
		ASSERT(frame < FRAME_COUNT);
		FrameInfo &frame_info = frame_infos[frame.get()];
		ASSERT(frame_info.id);
		if (frame_info.pins == 0) {
			// TODO: slow asserts
			ASSERT(std::find(free_list.cbegin(), free_list.cend(), frame) != free_list.cend());
			//ASSERT_T(free_list_iters.contains(frame)); // TODO
			//const auto iter = free_list_iters[frame];
			// TODO: slow
			//free_list.erase(iter);
			free_list.remove(frame);
			//free_list_iters.erase(frame); // TODO
		}
		frame_info.pins++;
	}

	static void unpin(FrameId frame, bool dirty)
	{
		ASSERT(frame < FRAME_COUNT);
		FrameInfo &frame_info = frame_infos[frame.get()];
		ASSERT(frame_info.pins > 0);
		ASSERT(frame_info.id);
		frame_info.pins--;
		frame_info.dirty = frame_info.dirty || dirty;
		if (frame_info.pins == 0) {
			// TODO: slow asserts
			ASSERT(std::find(free_list.cbegin(), free_list.cend(), frame) == free_list.cend());
			//ASSERT_T(!free_list_iters.contains(frame));
			free_list.push_back(frame);
			//free_list_iters[frame] = --free_list.end();
		}
	}

	void init()
	{
		ids_used.clear();
		free_list.clear();
		frame_infos = std::make_unique<FrameInfo[]>(FRAME_COUNT.get());
		//free_list_iters = std::make_unique<std::list<FrameId>::iterator[]>(FRAME_COUNT);
		for (FrameId frame {}; frame < FRAME_COUNT; frame++) {
			free_list.push_back(frame);
			//free_list_iters[frame] = --free_list.end();
		}
	}

	void destroy()
	{
		for (FrameId frame {}; frame < FRAME_COUNT; frame++) {
			ouput_frame(frame);
		}
		for (auto [file, fd] : fd_cache) {
			os::file_close(fd);
		}
		name_cache.clear();
		fd_cache.clear();
	}

	void flush(catalog::FileId file)
	{
		for (FrameId frame {}; frame < FRAME_COUNT; frame++) {
			FrameInfo &info = frame_infos[frame.get()];
			if (info.id && info.id->file == file) {
				ouput_frame(frame);
			}
		}
		name_cache.erase(file);
		if (fd_cache.contains(file)) {
			os::file_close(fd_cache.at(file));
			fd_cache.erase(file);
		}
	}

	void *request(catalog::FileId file, PageId page_id, bool append, FrameId &frame_out)
	{
		if (!name_cache.contains(file)) {
			name_cache[file] = catalog::get_file_name(file);
		}
		const Id id = { file, page_id };
		const auto iter = ids_used.find(id);
		if (iter == ids_used.end()) {
			ASSERT(!free_list.empty());
			frame_out = FrameId { free_list.front() };
			ouput_frame(frame_out);
			input_frame(frame_out, id, append);
		}
		else {
			frame_out = iter->second;
		}
		pin(frame_out);
		return frames.get_frame(frame_out);
	}

	void release(FrameId frame, bool dirty)
	{
		ASSERT(frame < FRAME_COUNT);
		const FrameInfo &frame_info = frame_infos[frame.get()];
		ASSERT(frame_info.id);
		unpin(frame, dirty);
	}

	void test()
	{
		const unsigned int seed = os::random();
		srand(seed);

		printf("testing buffer\n");

		buffer::init();
		catalog::init();

		const catalog::FileId file = catalog::get_table_files(catalog::TableId {}).second;
		std::vector<char> file_data;

		{
			std::unordered_map<PageId, unsigned int> pins_count;
			std::vector<buffer::Pin<char>> pins;

			for (unsigned int page_count = 0; page_count < 5'000; page_count++) {

				{
					const PageId page_id_append { page_count };
					buffer::Pin<char> page_append { file, page_id_append, true };
					const char c_append = 'A' + rand() % ('Z' - 'A' + 1);
					memset(page_append.get_page(), c_append, PAGE_SIZE.get());
					file_data.push_back(c_append);
				}

				for (unsigned int j = 0; j < 10; j++) {

					const PageId page_id_set(rand() % (page_count + 1));
					buffer::Pin<char> page_set { file, page_id_set };
					const char c_set = 'A' + rand() % ('Z' - 'A' + 1);
					memset(page_set.get_page(), c_set, PAGE_SIZE.get());
					file_data.at(page_id_set.get()) = c_set;

					if (rand() % (j + 1) == 0) {
						while (pins_count.size() + 1 >= buffer::FRAME_COUNT.get()) {
							const unsigned index = rand() % pins.size();
							const PageId page_id = pins[index].get_page_id();
							pins.erase(pins.begin() + index);
							if (--pins_count[page_id] == 0) {
								pins_count.erase(page_id);
							}
						}
						pins.push_back(std::move(page_set));
						pins_count[page_id_set]++;
					}
				}
			}
		}

		buffer::destroy();

		const os::Fd fd = os::file_open(catalog::get_file_name(file));
		for (PageId i {}; i < file_data.size(); i++) {
			char frame[PAGE_SIZE.get()];
			os::file_read(fd, i, frame);
			ASSERT(frame[0] == file_data[i.get()]);
		}
		os::file_close(fd);

		printf("testing buffer done\n");
	}
}
