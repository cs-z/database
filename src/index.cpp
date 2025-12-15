#include "common.hpp"
#include "os.hpp"
#include "page.hpp"
#include "buffer.hpp"
#include "iter.hpp"
#include "type.hpp"
#include "row.hpp"

#include <algorithm>
#include <unordered_set>

static int compare_keys(const Type &key_type, auto key_l, auto key_r)
{
	for (ColumnId column_id {}; column_id < key_type.size(); column_id++) {
		const int result = row::compare(key_type, column_id, key_l, key_r);
		if (result != 0) {
			return result;
		}
	}
	return 0;
}

// row id
using RID = unsigned int; // TODO

struct Header
{
	bool is_leaf;
};

struct LeafHeader
{
	Header header;
	page::Id prev;
	page::Id next;
};
using LeafEntryInfo = RID;
using Leaf = page::Slotted<LeafHeader, LeafEntryInfo>;

struct InnerHeader
{
	Header header;
	page::Id leftmost_child;
};
using InnerEntryInfo = page::Id;
using Inner = page::Slotted<InnerHeader, InnerEntryInfo>;

// first page of index heap file
struct FileHeader
{
public:

	inline void init()
	{
		page_count = page::Id { 1 };
		root_id = page::Id { 0 };
	}

	inline page::Id alloc()
	{
		return page_count++;
	}

	inline void set_root(page::Id page_id)
	{
		root_id = page_id;
	}

	inline page::Id get_root() const
	{
		ASSERT(root_id != 0);
		return root_id;
	}

private:

	page::Id page_count;
	page::Id root_id;

};

static std::pair<page::Id, Value> split(const buffer::Pin<Leaf> &page, const Type &key_type, const Value &key, RID rid, page::EntryId index);
static std::pair<page::Id, Value> split(const buffer::Pin<Inner> &page, const Type &key_type, const Value &key, page::Id page_id, page::EntryId index);

static std::pair<page::Id, Value> insert(const auto &page, const Type &key_type, const Value &key, auto value, page::EntryId index)
{
	const row::Prefix key_prefix = row::calculate_layout(key);
	u8 * const entry = page->insert(key_type.get_align(), key_prefix.size, value, index);
	if (entry) {
		row::write(key_prefix, key, entry);
		return {};
	}
	else {
		auto [new_page, new_key] = split(page, key_type, key, std::move(value), index);
		return std::make_pair(new_page, std::move(new_key));
	}
}

static std::pair<page::Id, Value> split(const buffer::Pin<Leaf> &page, const Type &key_type, const Value &key, RID rid, page::EntryId index)
{
	buffer::Pin<FileHeader> header { page.get_file_id(), page::Id {} };

	const page::EntryId entry_count_r = page->get_entry_count() / 2;
	const page::EntryId entry_count_l = page->get_entry_count() - entry_count_r;

	ASSERT(entry_count_l > 0);
	ASSERT(entry_count_r > 0);
	ASSERT(entry_count_l + entry_count_r == page->get_entry_count());

	const page::Offset align = key_type.get_align();

	buffer::Pin<Leaf> new_page { page.get_file_id(), header->alloc(), true };
	new_page->init({ Header { true }, page.get_page_id(), page->get_header().next });

	if (page->get_header().next != 0) {
		buffer::Pin<Leaf> next_page { page.get_file_id(), page->get_header().next };
		next_page->get_header().prev = new_page.get_page_id();
	}
	page->get_header().next = new_page.get_page_id();

	for (page::EntryId i {}; i < entry_count_r; i++) {
		const page::EntryId index = entry_count_l + i;
		page::Offset size;
		const u8 * const src = page->get_entry(index, size);
		u8 * const dst = new_page->insert(align, size, page->get_entry_info(index));
		ASSERT(dst);
		memcpy(dst, src, size);
	}
	page->remove_beyond(entry_count_l);
	page->shift(align);

	if (index < entry_count_l) {
		const auto result = insert(page, key_type, key, rid, index);
		ASSERT(result.first == 0);
	}
	else {
		const auto new_index = index - entry_count_l;
		const auto result = insert(new_page, key_type, key, rid, new_index);
		ASSERT(result.first == 0);
	}

	return std::make_pair(
		new_page.get_page_id(),
		row::read(key_type, new_page->get_entry(page::EntryId { 0 }))
	);
}

static std::pair<page::Id, Value> split(const buffer::Pin<Inner> &page, const Type &key_type, const Value &key, page::Id page_id, page::EntryId index)
{
	buffer::Pin<FileHeader> header { page.get_file_id(), page::Id {} };

	const auto middle = page->get_entry_count() / 2;
	const auto entry_count_l = middle;
	const auto entry_count_r = page->get_entry_count() - middle - 1;

	ASSERT(entry_count_l > 0);
	ASSERT(entry_count_r > 0);
	ASSERT(entry_count_l + entry_count_r + 1 == page->get_entry_count());

	const page::Offset align = key_type.get_align();

	buffer::Pin<Inner> new_page { page.get_file_id(), header->alloc(), true };
	new_page->init({ Header { false }, page->get_entry_info(middle) });

	Value new_key = row::read(key_type, page->get_entry(middle));

	for (page::EntryId i {}; i < entry_count_r; i++) {
		const page::EntryId index = middle + 1 + i;
		page::Offset size;
		const u8 * const src = page->get_entry(index, size);
		u8 * const dst = new_page->insert(align, size, page->get_entry_info(index));
		ASSERT(dst);
		memcpy(dst, src, size);
	}
	page->remove_beyond(entry_count_l);
	page->shift(align);

	if (index <= entry_count_l) {
		const auto result = insert(page, key_type, key, page_id, index);
		ASSERT(result.first == 0);
	}
	else {
		const auto new_index = index - entry_count_l - 1;
		const auto result = insert(new_page, key_type, key, page_id, new_index);
		ASSERT(result.first == 0);
	}

	return std::make_pair(new_page.get_page_id(), std::move(new_key));
}

void init(catalog::FileId file_id)
{
	buffer::Pin<FileHeader> header { file_id, page::Id {}, true };
	header->init();

	buffer::Pin<Leaf> root { file_id, header->alloc(), true };
	root->init({ Header { true }, page::Id {}, page::Id {} });
	header->set_root(root.get_page_id());
}

template <typename PageT>
static auto find_lower_entry(buffer::Pin<const PageT> &page, const Type &key_type, const Value &key)
{
	const auto iter = std::lower_bound(
		page->cbegin(),
		page->cend(),
		key,
		[&page, &key_type](const PageT::Slot &slot, const Value &key) {
			return compare_keys(key_type, page->get_entry(slot), key) < 0; // TODO
		}
	);
	const auto index = static_cast<page::EntryId>(iter - page->cbegin());
	if constexpr (std::is_same_v<PageT, Leaf>) {
		if (index < page->get_entry_count() && compare_keys(key_type, page->get_entry(index), key) == 0) {
			return std::optional<page::EntryId>(index);
		}
		return std::optional<page::EntryId>();
	}
	if constexpr (std::is_same_v<PageT, Inner>) {
		const bool equal = index < page->get_entry_count() && compare_keys(key_type, page->get_entry(index), key) == 0;
		const page::EntryId entry_id { equal ? index + 1 : index };
		return entry_id > 0 ? page->get_entry_info(entry_id - 1) : page->get_header().leftmost_child;
	}
	UNREACHABLE();
}

template <typename PageT>
static auto find_upper_entry(buffer::Pin<const PageT> &page, const Type &key_type, const Value &key)
{
	const auto iter = std::upper_bound(
		page->cbegin(),
		page->cend(),
		key,
		[&page, &key_type](const Value &key, const PageT::Slot &slot) {
			return compare_keys(key_type, page->get_entry(slot), key) > 0;
		}
	);
	const auto index = static_cast<page::EntryId>(iter - page->cbegin());
	if constexpr (std::is_same_v<PageT, Leaf>) {
		if (index > 0 && compare_keys(key_type, page->get_entry(index - 1), key) == 0) {
			return std::optional<page::EntryId>(index - 1);
		}
		return std::optional<page::EntryId>();
	}
	if constexpr (std::is_same_v<PageT, Inner>) {
		return index > 0 ? page->get_entry_info(index - 1) : page->get_header().leftmost_child;
	}
	UNREACHABLE();
}

template <typename PageT>
static page::EntryId find_insert_entry(buffer::Pin<PageT> &page, const Type &key_type, const Value &key)
{
	const auto iter = std::upper_bound(
		page->cbegin(),
		page->cend(),
		key,
		[&page, &key_type](const Value &key, const PageT::Slot &slot) {
			return compare_keys(key_type, page->get_entry(slot), key) > 0;
		}
	);
	return static_cast<page::EntryId>(iter - page->cbegin());
}

static std::pair<page ::Id, Value> insert_recursive(catalog::FileId file_id, page::Id page_id, const Type &key_type, const Value &key, RID rid)
{
	buffer::Pin<Header> page { file_id, page_id };
	if (page->is_leaf) {
		buffer::Pin<Leaf> leaf = std::move(page);
		const page::EntryId index = find_insert_entry(leaf, key_type, key);
		return insert(leaf, key_type, key, rid, index);
	}
	else {
		buffer::Pin<Inner> inner = std::move(page);
		const page::EntryId index = find_insert_entry(inner, key_type, key);
		const auto child = index > 0 ? inner->get_entry_info(index - 1) : inner->get_header().leftmost_child;
		auto overflow = insert_recursive(file_id, child, key_type, key, rid);
		if (overflow.first != 0) {
			return insert(inner, key_type, overflow.second, overflow.first, index);
		}
		return {};
	}
}

void insert(catalog::FileId file_id, const Type &key_type, const Value &key, RID rid)
{
	buffer::Pin<FileHeader> header { file_id, page::Id {} };
	const auto overflow = insert_recursive(file_id, header->get_root(), key_type, key, rid);
	if (overflow.first != 0) {
		buffer::Pin<Inner> inner { file_id, header->alloc(), true };
		inner->init({ Header { false }, header->get_root() });
		const auto result = insert(inner, key_type, overflow.second, overflow.first, page::EntryId {});
		ASSERT(result.first == 0);
		header->set_root(inner.get_page_id());
	}
}
/*
#include <map>

static std::string random_string(std::size_t min_size, std::size_t max_size)
{
	const std::size_t size = rand() % (max_size - min_size + 1) + min_size;
	std::string string;
	for (std::size_t i = 0; i < size; i++) {
		string.push_back(rand() % ('Z' - 'A' + 1) + 'A');
	}
	return string;
}

static void print_recursive(buffer::Pin<const Header> page, const Type &key_type, int tabs)
{
	if (page->is_leaf) {
		buffer::Pin<const Leaf> leaf = std::move(page);
		for (int i = 0; i < tabs; i++) printf("  ");
		printf("[");
		for (page::EntryId i = {}; i < leaf->get_entry_count(); i++) {
			const Value key = row::read(key_type, leaf->get_entry(i));
			const auto value = leaf->get_entry_info(i);
			value_print(key);
			printf(": %u", value);
			if (i + 1 < leaf->get_entry_count()) {
				printf(", ");
			}
		}
		printf("]\n");
	}
	else {
		buffer::Pin<const Inner> inner = std::move(page);
		print_recursive({ inner.get_file_id(), inner->get_header().leftmost_child }, key_type, tabs + 1);
		for (page::EntryId i = {}; i < inner->get_entry_count(); i++) {
			for (int i = 0; i < tabs; i++) printf("  ");
			if (i == 0) printf("{");
			const Value key = row::read(key_type, inner->get_entry(i));
			value_print(key);
			if (i + 1 == inner->get_entry_count()) printf("}");
			printf("\n");
			print_recursive({ inner.get_file_id(), inner->get_entry_info(i) }, key_type, tabs + 1);
		}
	}
}

static void print_string(catalog::FileId file_id, const Type &key_type)
{
	buffer::Pin<const FileHeader> header { file_id, page::Id {} };
	print_recursive({ file_id, header->get_root() }, key_type, 0);
}

static bool find_next_value(buffer::Pin<const Leaf> &page, page::EntryId &index, bool reverse, const Type *key_type_opt, const Value *key_opt)
{
	ASSERT((key_type_opt == nullptr) == (key_opt == nullptr));

	std::optional<page::Id> new_page_id;
	buffer::Pin<const Leaf> new_page;

	page::EntryId new_index;

	if (!reverse) {
		if (index + 1 == page->get_entry_count()) {
			new_page_id = page->get_header().next;
		}
		else {
			new_index = index + 1;
		}
	}
	else {
		if (index == 0) {
			new_page_id = page->get_header().prev;
		}
		else {
			new_index = index - 1;
		}
	}

	if (new_page_id) {
		if (*new_page_id == 0) {
			return false;
		}
		new_page = page.shift(*new_page_id);
		if (!reverse) {
			new_index = page::EntryId { 0 };
		}
		else {
			new_index = new_page->get_entry_count() - 1;
		}
	}

	if (key_opt) {
		auto &page_to_check = new_page_id ? new_page : page;
		if (compare_keys(*key_type_opt, page_to_check->get_entry(new_index), *key_opt) != 0) {
			return false;
		}
	}

	if (new_page_id) {
		page = std::move(new_page);
	}
	index = new_index;

	return true;
}

static std::pair<buffer::Pin<const Leaf>, page::EntryId> find_string_recursive(buffer::Pin<const Header> page, const Type &key_type, const Value &key, bool reverse)
{
	if (page->is_leaf) {
		buffer::Pin<const Leaf> leaf = std::move(page);
		const auto entry_id = reverse ? find_upper_entry(leaf, key_type, key) : find_lower_entry(leaf, key_type, key);
		if (entry_id) {
			return std::make_pair(std::move(leaf), *entry_id);
		}
		else {
			return {};
		}
	}
	else {
		buffer::Pin<const Inner> inner = std::move(page);
		const page::Id child = reverse ? find_upper_entry(inner, key_type, key) : find_lower_entry(inner, key_type, key);
		return find_string_recursive(inner.shift(child), key_type, key, reverse);
	}
}

// TODO: implement iterator
std::pair<buffer::Pin<const Leaf>, page::EntryId> find_string(catalog::FileId file_id, const Type &key_type, const Value &key, bool reverse)
{
	buffer::Pin<const FileHeader> header { file_id, page::Id {} };
	auto iter = find_string_recursive(header.shift(header->get_root()), key_type, key, reverse);
	if (iter.first != 0) {
		while (find_next_value(iter.first, iter.second, !reverse, &key_type, &key)) {}
	}
	return iter;
}

static std::vector<RID> debug_collect_values(catalog::FileId file_id, const Type &key_type, const Value &key)
{
	std::vector<RID> values;
	auto iter = find_string(file_id, key_type, key, false);
	if (iter.first) {
		do {
			const auto current_key = iter.first->get_entry(iter.second);
			const auto current_value = iter.first->get_entry_info(iter.second);
			ASSERT(compare_keys(key_type, current_key, key) == 0);
			values.push_back(current_value);
		} while (find_next_value(iter.first, iter.second, false, &key_type, &key));
	}

	std::vector<RID> values_rev;
	auto iter_rev = find_string(file_id, key_type, key, true);
	if (iter_rev.first) {
		do {
			const auto current_key = iter_rev.first->get_entry(iter_rev.second);
			const auto current_value = iter_rev.first->get_entry_info(iter_rev.second);
			ASSERT(compare_keys(key_type, current_key, key) == 0);
			values_rev.push_back(current_value);
		} while (find_next_value(iter_rev.first, iter_rev.second, true, &key_type, &key));
	}

	ASSERT(values.size() == values_rev.size());
	auto it = values.begin();
	auto it_rev = values_rev.rbegin();
	for (std::size_t i = 0; i < values.size(); i++) {
		ASSERT(*it++ == *it_rev++);
	}

	return values;
}

static std::pair<page::Id, page::EntryId> debug_get_tree_iter(buffer::Pin<const Header> page)
{
	if (page->is_leaf) {
		buffer::Pin<const Leaf> leaf = std::move(page);
		if (leaf->get_entry_count() > 0) {
			return std::make_pair(leaf.get_page_id(), page::EntryId {});
		}
		return {};
	}
	else {
		buffer::Pin<const Inner> inner = std::move(page);
		return debug_get_tree_iter(inner.shift(inner->get_header().leftmost_child));
	}
}

static unsigned int debug_tree_recursive(const Type &key_type, buffer::Pin<const Header> page, std::optional<Value> min_key, std::optional<Value> max_key)
{
	if (page->is_leaf) {
		buffer::Pin<const Leaf> leaf = std::move(page);

		ASSERT(leaf->get_entry_count() > 0);
		for (page::EntryId i { 1 }; i < leaf->get_entry_count(); i++) {
			const auto a = leaf->get_entry(i - 1);
			const auto b = leaf->get_entry(i);
			ASSERT(compare_keys(key_type, a, b) <= 0);
		}
		if (min_key) {
			const auto key = leaf->get_entry(page::EntryId { 0 });
			ASSERT(compare_keys(key_type, key, *min_key) >= 0);
		}
		if (max_key) {
			const auto key = leaf->get_entry(leaf->get_entry_count() - 1);
			ASSERT(compare_keys(key_type, key, *max_key) <= 0);
		}
		return 1;
	}
	else {
		buffer::Pin<const Inner> inner = std::move(page);

		ASSERT(inner->get_entry_count() > 0);

		for (page::EntryId i { 1 }; i < inner->get_entry_count(); i++) {
			const auto a = inner->get_entry(i - 1);
			const auto b = inner->get_entry(i);
			ASSERT(compare_keys(key_type, a, b) <= 0);
		}
		if (min_key) {
			const auto key = inner->get_entry(page::EntryId { 0 });
			ASSERT(compare_keys(key_type, key, *min_key) >= 0);
		}
		if (max_key) {
			const auto key = inner->get_entry(inner->get_entry_count() - 1);
			ASSERT(compare_keys(key_type, key, *max_key) <= 0);
		}

		std::unordered_set<unsigned int> depths;
		depths.insert(debug_tree_recursive(key_type, inner.shift(inner->get_header().leftmost_child), min_key, inner->get_entry_count() > 0 ? row::read(key_type, inner->get_entry(page::EntryId { 0 })) : max_key));
		for (page::EntryId i { 0 }; i < inner->get_entry_count(); i++) {
			depths.insert(debug_tree_recursive(key_type, inner.shift(inner->get_entry_info(i)), i == 0 ? min_key : row::read(key_type, inner->get_entry(i - 1)), i + 1 == inner->get_entry_count() ? max_key : row::read(key_type, inner->get_entry(i + 1))));
		}
		ASSERT(depths.size() == 1);
		return 1 + *depths.begin();
	}
}

static unsigned int debug_tree(catalog::FileId file_id, const Type &key_type, const std::map<Value, std::vector<RID>> &map)
{
	buffer::Pin<const FileHeader> header { file_id, page::Id {} };

	const unsigned int depth = debug_tree_recursive(key_type, header.shift(header->get_root()), std::nullopt, std::nullopt);

	std::map<Value, std::vector<RID>> tree_map;

	const auto iter = debug_get_tree_iter(header.shift(header->get_root()));
	ASSERT(iter.first != 0);
	buffer::Pin<const Leaf> page { file_id, iter.first };
	page::EntryId index = iter.second;

	do {
		const auto key_ptr = page->get_entry(index);
		const auto key = row::read(key_type, key_ptr);
		const auto value = page->get_entry_info(index);
		tree_map[key].push_back(value);
	} while (find_next_value(page, index, false, nullptr, nullptr));

	ASSERT(map.size() == tree_map.size());
	auto vec_iter = map.begin();
	auto vec_tree_iter = tree_map.begin();
	for (std::size_t i = 0; i < map.size(); i++) {
		auto &[key, values] = *vec_iter++;
		auto &[tree_key, tree_values] = *vec_tree_iter++;
		ASSERT(value_equal(key, tree_key));
		ASSERT(values.size() == tree_values.size());

		for (std::size_t j = 0; j < values.size(); j++) {
			ASSERT(values[j] == tree_values[j]);
		}
	}
	ASSERT(vec_iter == map.end());
	ASSERT(vec_tree_iter == tree_map.end());

	return depth;
}

int main(int argc, const char **argv)
{
	Type key_type;
	key_type.push(ColumnType::VARCHAR);
	key_type.push(ColumnType::VARCHAR);

	buffer::init();
	catalog::init();
	const catalog::FileId file_id = catalog::get_table_file_ids(catalog::TableId {}).dat;

	init(file_id);

	ASSERT(argc == 2);
	const unsigned int count = std::stoul(argv[1]);

	const unsigned int seed = os::random();
	printf("%u\n", seed);
	srand(seed);

	std::vector<Value> map_keys;
	std::map<Value, std::vector<RID>> map;

	constexpr unsigned int MIN_LENGTH = 1;
	constexpr unsigned int MAX_LENGTH = 2;

	for (unsigned int i = 0; i < count; i++) {

		const auto key_1 = random_string(MIN_LENGTH, MAX_LENGTH);
		ASSERT(key_1.size() > 0);
		const auto key_2 = random_string(MIN_LENGTH, MAX_LENGTH);
		ASSERT(key_2.size() > 0);
		const Value key { key_1, key_2 };

		const RID value = rand() % 1000;

		// printf("\n%u. insert: ", i);
		// value_print(key);
		// printf(" -> %u\n", value);

		map[key].push_back(value);
		map_keys.push_back(key);

		insert(file_id, key_type, key, value);
		//debug_tree(file_id, key_type, map);
	}

	print_string(file_id, key_type);

	for (unsigned int i = 0; i < count; i++) {

		Value key;

		if (rand() % 2) {
			const auto key_1 = random_string(MIN_LENGTH, MAX_LENGTH);
			ASSERT(key_1.size() > 0);
			const auto key_2 = random_string(MIN_LENGTH, MAX_LENGTH);
			ASSERT(key_2.size() > 0);
			key = { key_1, key_2 };
		}
		else {
			key = map_keys[rand() % map_keys.size()];
		}

		// printf("\n%u. find: ", i);
		// value_print(key);
		// printf("\n");

		auto values = debug_collect_values(file_id, key_type, { key });
		auto map_values = map.contains(key) ? map.at(key) : std::vector<RID>{};

		// for (auto v : values) {
		// 	printf("v: %u\n", v);
		// }
		// printf("\n");
		// for (auto v : map_values) {
		// 	printf("m: %u\n", v);
		// }
		// printf("\n");

		ASSERT(values.size() == map_values.size());
		for (std::size_t j = 0;j < values.size(); j++) {
			ASSERT(values[j] == map_values[j]);
		}
	}

	print_string(file_id, key_type);
	printf("depth: %u\n", debug_tree(file_id, key_type, map));

	buffer::destroy();
}*/
