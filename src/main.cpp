#include <fstream>
#include <streambuf>

#include "buffer.hpp"
#include "execute.hpp"
#include "error.hpp"

/*void test_buffer()
{
	const unsigned int seed = os::random();
	srand(seed);

	printf("testing buffer\n");

	const catalog::File file = catalog::get_table_files("SYS_TABLES").file_dat;
	std::vector<char> file_data;

	buffer::init();
	catalog::init();

	{
		std::unordered_map<page::Id, unsigned int> pins_count;
		std::vector<buffer::Pin<char>> pins;

		for (unsigned int page_count = 0; page_count < 5'000; page_count++) {

			{
				const unsigned int page_id_append = page_count;
				buffer::Pin<char> page_append { file, page_id_append, true };
				const char c_append = 'A' + rand() % ('Z' - 'A' + 1);
				memset(page_append.get_page(), c_append, page::SIZE);
				file_data.push_back(c_append);
			}

			for (unsigned int j = 0; j < 10; j++) {

				const unsigned int page_id_set = rand() % (page_count + 1);
				buffer::Pin<char> page_set { file, page_id_set };
				const char c_set = 'A' + rand() % ('Z' - 'A' + 1);
				memset(page_set.get_page(), c_set, page::SIZE);
				file_data.at(page_id_set) = c_set;

				if (rand() % (j + 1) == 0) {
					while (pins_count.size() + 1 >= buffer::FRAME_COUNT) {
						const unsigned index = rand() % pins.size();
						const unsigned page_id = pins[index].get_page_id();
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

	const os::Fd fd = os::file_open(catalog::get_file_path(file));
	for (unsigned int i = 0; i < file_data.size(); i++) {
		char frame[PAGE_SIZE];
		os::file_read(fd, i, frame);
		ASSERT_TODO(frame[0] == file_data[i]);
	}
	os::file_close(fd);

	printf("testing buffer done\n");
}*/

/*#include <algorithm>
#include "fst.hpp"

void test_fst()
{
	unsigned int seed = {};
	ASSERT_TODO(getrandom(&seed, sizeof(seed), 0) == sizeof(seed));
	srand(seed);

	printf("testing fst\n");

	buffer::init();

	const file::Id file_fst = file::create_table("test").fd_fst;

	std::vector<u32> test;
	for (u32 i = 0; i < 1'000; i++) {
		const u32 value_append = rand() % 100'000U + 1U;
		fst_append(file_fst, value_append);
		test.push_back(value_append);
		for (u32 j = 0; j < 10; j++) {
			const u32 index_set = rand() % test.size();
			const u32 value_set = rand() % 100'000U + 1U;
			fst_set(file_fst, index_set, value_set);
			test.at(index_set) = value_set;
			const u32 value_find = rand() % 100'000U + 1U;
			const std::optional<u32> page_id = fst_find(file_fst, value_find);
			const u32 test_max = *std::max_element(test.cbegin(), test.cend());
			ASSERT_TODO((!page_id && test_max < value_find) || (page_id && test.at(*page_id) >= value_find));
		}
	}

	file::remove_table("test");

	buffer::destroy();

	ASSERT_TODO(close(file_fst) == 0);

	printf("testing fst done\n");
}*/

int main(int argc, const char **argv)
{
	const unsigned int seed = os::random();
	srand(seed);

	buffer::init();
	catalog::init();

	catalog::TableDef users_def;
	users_def.push_back({ "NAME", ColumnType::VARCHAR });
	users_def.push_back({ "AGE", ColumnType::INTEGER });
	users_def.push_back({ "GRADE", ColumnType::INTEGER });
	execute_create_table("USERS", users_def);

	catalog::TableDef dogs_def;
	dogs_def.push_back({ "NAME", ColumnType::VARCHAR });
	dogs_def.push_back({ "AGE", ColumnType::INTEGER });
	execute_create_table("DOGS", dogs_def);

	unsigned int count = 10;

	for (unsigned int i = 0; i < count; i++) {
		const Value &value = {
			ColumnValueVarchar { "user_" + std::to_string(i) },
			ColumnValueInteger { rand() % count },
			ColumnValueInteger { rand() % 5 + 1 },
		};
		execute_insert("USERS", value);
	}

	for (unsigned int i = 0; i < count; i++) {
		const Value &value = {
			ColumnValueVarchar { "dog_" + std::to_string(i) },
			ColumnValueInteger { rand() % count },
		};
		execute_insert("DOGS", value);
	}

	{
		std::string source;
		try {
			if (argc >= 2) {
				if (argc != 2) {
					ASSERT_TODO(false && "too many arguments");
				}
				source = argv[1];
			}
			else {
				std::ifstream stream { "run.sql" };
				source = {
					std::istreambuf_iterator<char>(stream),
					std::istreambuf_iterator<char>()
				};
			}
			execute_query(source);
		}
		catch (const ClientError &error) {
			error.report(source);
		}
		catch (const ServerError &error) {
			error.report();
		}
	}

	buffer::destroy();
}