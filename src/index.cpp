#include "common.hpp"
#include "os.hpp"
#include "value.hpp"
#include "page.hpp"
#include "buffer.hpp"
#include "iter.hpp"

#include <algorithm>
#include <unordered_set>

// record id
using RID = unsigned int;

struct DataLeaf
{
	page::Id prev;
	page::Id next;
};
using ValueLeaf = RID; // record id

using DataInner = page::Id; // children page id
using ValueInner = page::Id; // leftmost child page id

template <bool IsLeaf, typename Data, typename Value>
class PageStringBase;

using PageStringLeaf = PageStringBase<true, DataLeaf, ValueLeaf>;
using PageStringInner = PageStringBase<false, DataInner, ValueInner>;

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

struct PageHeader
{
	bool is_leaf;
};

// TODO: create abstract class for all kinds of slotted pages

// abstract class for slotted tree nodes
template <bool IsLeaf, typename Data, typename Value>
class PageStringBase
{
public:

	static constexpr bool IsLeafPage = IsLeaf && std::is_same_v<Data, DataLeaf> && std::is_same_v<Value, ValueLeaf>;
	static constexpr bool IsInnerPage = !IsLeaf && std::is_same_v<Data, DataInner> && std::is_same_v<Value, ValueInner>;

	inline const Data &get_data() const
	{
		return data;
	}

	inline Data &get_data()
	{
		return data;
	}

	inline unsigned int get_count() const
	{
		return slots_count;
	}

	inline std::string_view get_key(unsigned int index) const
	{
		ASSERT(index < slots_count);
		return get_key(slots[index]);
	}

	inline Value get_value(unsigned int index) const
	{
		ASSERT(index < slots_count);
		return slots[index].value;
	}

	void init(Data data)
	{
		this->header.is_leaf = IsLeaf;
		this->free_begin = offsetof(PageStringBase, slots);
		this->free_end = page::SIZE;
		this->slots_count = 0;
		this->data = std::move(data);
	}

	static std::pair<page::Id, std::string> insert(const buffer::Pin<PageStringBase> &page, std::string_view key, Value value, unsigned int index)
	{
		ASSERT(key.size() <= page->free_begin);
		ASSERT(index <= page->slots_count);

		const page::Offset new_free_begin = page->free_begin + sizeof(Slot);
		const page::Offset new_free_end = page->free_end - key.size();

		if (new_free_begin <= new_free_end) {
			memcpy(page->get_pointer(new_free_end), key.data(), key.size());

			memmove(page->slots + index + 1, page->slots + index, (page->slots_count - index) * sizeof(Slot));
			page->slots[index] = Slot { new_free_end, static_cast<page::Offset>(key.size()), value };

			page->free_begin = new_free_begin;
			page->free_end = new_free_end;
			page->slots_count++;

			return {};
		}
		else {
			auto [new_page, new_key] = page->split(page, key, value, index);
			return std::make_pair(new_page, std::string { new_key });
		}
	}

	inline unsigned int find_insert_position(std::string_view key) const
	{
		return find_upper_slot(key);
	}

	auto find_lower(std::string_view key) const
	{
		if constexpr (IsLeafPage) {
			const unsigned int index = find_lower_slot(key);
			if (index == slots_count || compare_strings(get_key(index), key) != 0) {
				return std::optional<unsigned int>();
			}
			return std::optional<unsigned int>(index);
		}
		if constexpr (IsInnerPage) {
			unsigned int index = find_lower_slot(key);
			if (index < slots_count && compare_strings(get_key(index), key) == 0) {
				index++;
			}
			return index > 0 ? slots[index - 1].value : data;
		}
	}

	auto find_upper(std::string_view key) const
	{
		if constexpr (IsLeafPage) {
			const unsigned int index = find_upper_slot(key);
			if (index == 0 || compare_strings(get_key(index - 1), key) != 0) {
				return std::optional<unsigned int>();
			}
			return std::optional<unsigned int>(index - 1);
		}
		if constexpr (IsInnerPage) {
			const unsigned int index = find_upper_slot(key);
			return index > 0 ? slots[index - 1].value : data;
		}
	}

private:

	struct Slot
	{
		page::Offset key_offset;
		page::Offset key_size;
		Value value;
	};

	PageHeader header;

	page::Offset free_begin, free_end;
	unsigned int slots_count;

	Data data;

	Slot slots[FLEXIBLE_ARRAY];

private:

	inline char *get_pointer(page::Offset offset)
	{
		return reinterpret_cast<char *>(this) + offset;
	}

	inline const char *get_pointer(page::Offset offset) const
	{
		return reinterpret_cast<const char *>(this) + offset;
	}

	inline std::string_view get_key(const Slot &slot) const
	{
		return { get_pointer(slot.key_offset), slot.key_size };
	}

	unsigned int find_lower_slot(std::string_view key) const
	{
		const auto comp = [this](const Slot &slot, const std::string_view &key) {
			return compare_strings(get_key(slot), key) < 0;
		};
		const Slot *slot = std::lower_bound(slots, slots + slots_count, key, comp);
		return static_cast<unsigned int>(slot - slots);
	}

	unsigned int find_upper_slot(std::string_view key) const
	{
		const auto comp = [this](const std::string_view &key, const Slot &slot) {
			return compare_strings(key, get_key(slot)) < 0;
		};
		const Slot *slot = std::upper_bound(slots, slots + slots_count, key, comp);
		return static_cast<unsigned int>(slot - slots);
	}

	// validate state
	static void debug(const buffer::Pin<const PageStringBase> &page)
	{
		if constexpr (IsLeafPage) {
			ASSERT(page->header.is_leaf);
			ASSERT(page->free_begin == offsetof(PageStringBase, slots) + page->slots_count * sizeof(Slot));
			ASSERT(page->free_end >= page->free_begin);
			ASSERT(page->free_end <= page::SIZE);
			std::vector<bool> allocated ( page::SIZE - page->free_end, false );
			for (unsigned int i = 0; i < page->slots_count; i++) {
				const Slot &slot = page->slots[i];
				if (slot.key_offset == 0) {
					ASSERT(slot.key_size == 0);
					continue;
				}
				ASSERT(slot.key_size > 0);
				ASSERT(slot.key_offset >= page->free_end);
				ASSERT(slot.key_offset + slot.key_size <= page::SIZE);
				for (unsigned int j = 0; j < slot.key_size; j++) {
					auto byte = allocated.at(slot.key_offset + j - page->free_end);
					ASSERT(!byte);
					byte = true;
				}
			}
			bool switched = false;
			for (auto c : allocated) {
				if (c) {
					switched = true;
				}
				else {
					ASSERT(!switched);
				}
			}
			if (page->data.prev != 0) {
				buffer::Pin<const PageStringBase> prev { page.get_file(), page->data.prev };
				if (page->slots_count > 0 && prev->slots_count > 0) {
					ASSERT(prev->data.next == page.get_page_id());
					ASSERT(compare_strings(page->get_key(0), prev->get_key(prev->get_count() - 1)) >= 0);
				}
			}
			if (page->data.next != 0) {
				buffer::Pin<const PageStringBase> next { page.get_file(), page->data.next };
				if (page->slots_count > 0 && next->slots_count > 0) {
					ASSERT(next->data.prev == page.get_page_id());
					ASSERT(compare_strings(page->get_key(page->slots_count - 1), next->get_key(0)) <= 0);
				}
			}
		}
		if constexpr (IsInnerPage) {
			ASSERT(!page->header.is_leaf);
			ASSERT(page->free_begin == offsetof(PageStringBase, slots) + page->slots_count * sizeof(Slot));
			ASSERT(page->free_end >= page->free_begin);
			ASSERT(page->free_end <= page::SIZE);
			ASSERT(page->data != 0);
			std::vector<bool> allocated ( page::SIZE - page->free_end, false );
			std::unordered_set<page::Id> children;
			children.insert(page->data);
			for (unsigned int i = 0; i < page->slots_count; i++) {
				const Slot &slot = page->slots[i];
				ASSERT(slot.value != 0);
				ASSERT(!children.contains(slot.value));
				children.insert(slot.value);
				if (slot.key_offset == 0) {
					ASSERT(slot.key_size == 0);
					continue;
				}
				ASSERT(slot.key_size > 0);
				ASSERT(slot.key_offset >= page->free_end);
				ASSERT(slot.key_offset + slot.key_size <= page::SIZE);
				for (unsigned int j = 0; j < slot.key_size; j++) {
					auto byte = allocated.at(slot.key_offset + j - page->free_end);
					ASSERT(!byte);
					byte = true;
				}
			}
			bool switched = false;
			for (auto c : allocated) {
				if (c) {
					switched = true;
				}
				else {
					ASSERT(!switched);
				}
			}
		}
	}

	// compact key at the end of the page
	void shift_keys()
	{
		std::vector<std::tuple<page::Offset, page::Offset, unsigned int>> keys;
		for (unsigned int i = 0; i < slots_count; i++) {
			keys.push_back({ slots[i].key_offset, slots[i].key_size, i });
		}
		std::sort(keys.begin(), keys.end());
		page::Offset offset = page::SIZE;
		for (auto it = keys.rbegin(); it != keys.rend(); it++) {
			auto [key_offset, key_size, index] = *it;
			ASSERT(offset > key_size);
			offset -= key_size;
			if (offset != key_offset) {
				ASSERT(offset > key_offset);
				char *src = get_pointer(key_offset);
				char *dst = get_pointer(offset);
				memmove(dst, src, key_size);
				slots[index].key_offset = offset;
			}
		}
		ASSERT(free_begin == offsetof(PageStringBase, slots) + slots_count * sizeof(Slot));
		free_end = offset;
	}

	static std::pair<page::Id, std::string> split(const buffer::Pin<PageStringBase> &page, std::string_view key, Value value, unsigned int index)
	{
		buffer::Pin<FileHeader> header { page.get_file(), page::Id {} };
		if constexpr (IsLeafPage) {
			ASSERT(page->slots_count >= 2);

			const unsigned int slots_count_sum = page->slots_count;
			const unsigned int slots_count_r = slots_count_sum / 2;
			const unsigned int slots_count_l = slots_count_sum - slots_count_r;

			ASSERT(slots_count_l > 0);
			ASSERT(slots_count_r > 0);
			ASSERT(slots_count_l + slots_count_r == slots_count_sum);

			buffer::Pin<PageStringLeaf> new_page { page.get_file(), header->alloc(), true };
			new_page->init({ page.get_page_id(), page->get_data().next });

			if (page->get_data().next != 0) {
				buffer::Pin<PageStringLeaf> next_page { page.get_file(), page->get_data().next };
				next_page->get_data().prev = new_page.get_page_id();
			}
			page->get_data().next = new_page.get_page_id();

			for (unsigned int i = 0; i < slots_count_r; i++) {
				const unsigned int index = slots_count_l + i;
				const auto key = page->get_key(index);

				const page::Offset new_free_begin = new_page->free_begin + sizeof(Slot);
				const page::Offset new_free_end = new_page->free_end - key.size();
				ASSERT(new_free_begin <= new_free_end);

				memcpy(new_page->get_pointer(new_free_end), key.data(), key.size());
				new_page->slots[i] = Slot { new_free_end, static_cast<page::Offset>(key.size()), page->slots[index].value };

				new_page->free_begin = new_free_begin;
				new_page->free_end = new_free_end;
			}
			new_page->slots_count = slots_count_r;

			page->slots_count = slots_count_l;
			page->free_begin = offsetof(PageStringBase, slots) + page->slots_count * sizeof(Slot);
			page->shift_keys();

			if (index < slots_count_l) {
				const auto result = insert(page, key, value, index);
				ASSERT(result.first == 0);
			}
			else {
				const unsigned int new_index = index - slots_count_l;
				const auto result = insert(new_page, key, value, new_index);
				ASSERT(result.first == 0);
			}

			return std::make_pair(new_page.get_page_id(), std::string { new_page->get_key(0) });
		}
		if constexpr (IsInnerPage) {
			ASSERT(page->slots_count >= 3);

			const auto middle = page->slots_count / 2;

			const auto slots_count_l = middle;
			const auto slots_count_r = page->slots_count - middle - 1;

			ASSERT(slots_count_l > 0);
			ASSERT(slots_count_r > 0);
			ASSERT(slots_count_l + slots_count_r + 1 == page->slots_count);

			buffer::Pin<PageStringInner> new_page { page.get_file(), header->alloc(), true };
			new_page->init(page->slots[middle].value);

			const auto new_key = std::string { page->get_key(middle) };

			for (unsigned int i = 0; i < slots_count_r; i++) {
				const unsigned int index = middle + 1 + i;
				const auto key = page->get_key(index);

				const page::Offset new_free_begin = new_page->free_begin + sizeof(Slot);
				const page::Offset new_free_end = new_page->free_end - key.size();
				ASSERT(new_free_begin <= new_free_end);

				memcpy(new_page->get_pointer(new_free_end), key.data(), key.size());
				new_page->slots[i] = Slot { new_free_end, static_cast<page::Offset>(key.size()), page->slots[index].value };

				new_page->free_begin = new_free_begin;
				new_page->free_end = new_free_end;
			}
			new_page->slots_count = slots_count_r;

			page->slots_count = slots_count_l;
			page->free_begin = offsetof(PageStringBase, slots) + page->slots_count * sizeof(Slot);
			page->shift_keys();

			if (index <= slots_count_l) {
				const auto result = insert(page, key, value, index);
				ASSERT(result.first == 0);
			}
			else {
				const unsigned int new_index = index - slots_count_l - 1;
				const auto result = insert(new_page, key, value, new_index);
				ASSERT(result.first == 0);
			}

			return std::make_pair(new_page.get_page_id(), new_key);
		}
	}

};

/*static void print_recursive_string(buffer::Pin<const PageHeader> page, int tabs)
{
	if (page->is_leaf) {
		buffer::Pin<const PageStringLeaf> leaf = std::move(page);
		for (int i = 0; i < tabs; i++) printf("  ");
		printf("[");
		for (unsigned int i = 0; i < leaf->get_count(); i++) {
			const auto key = leaf->get_key(i);
			const auto value = leaf->get_value(i);
			printf("\'%.*s\': %u", (int) key.size(), key.data(), value);
			if (i + 1 < leaf->get_count()) {
				printf(", ");
			}
		}
		printf("]\n");
	}
	else {
		buffer::Pin<const PageStringInner> inner = std::move(page);
		print_recursive_string({ inner.get_file(), inner->get_data() }, tabs + 1);
		for (unsigned int i = 0; i < inner->get_count(); i++) {
			for (int i = 0; i < tabs; i++) printf("  ");
			if (i == 0) printf("(");
			const auto key = inner->get_key(i);
			printf("%.*s", (int) key.size(), key.data());
			if (i + 1 == inner->get_count()) printf(")");
			printf("\n");
			print_recursive_string({ inner.get_file(), inner->get_value(i) }, tabs + 1);
		}
	}
}

static void print_string(catalog::FileId file)
{
	buffer::Pin<const FileHeader> header { file, page::Id {} };
	print_recursive_string({ file, header->get_root() }, 0);
}*/

void init_string(catalog::FileId file)
{
	buffer::Pin<FileHeader> header { file, page::Id {}, true };
	header->init();

	buffer::Pin<PageStringLeaf> root { file, header->alloc(), true };
	root->init({ page::Id {}, page::Id {} });
	header->set_root(root.get_page_id());
}

static std::pair<page::Id, std::string> insert_recursive_string(catalog::FileId file, page::Id page_id, std::string_view key, RID value)
{
	buffer::Pin<PageHeader> page { file, page_id };
	if (page->is_leaf) {
		buffer::Pin<PageStringLeaf> leaf = std::move(page);
		const unsigned int index = leaf->find_insert_position(key);
		return PageStringLeaf::insert(leaf, key, value, index);
	}
	else {
		buffer::Pin<PageStringInner> inner = std::move(page);
		const auto index = inner->find_insert_position(key);
		const auto child = index > 0 ? inner->get_value(index - 1) : inner->get_data();
		auto overflow = insert_recursive_string(file, child, key, value);
		if (overflow.first != 0) {
			return PageStringInner::insert(inner, overflow.second, overflow.first, index);
		}
		return {};
	}
}

void insert_string(catalog::FileId file, std::string_view key, RID rid)
{
	buffer::Pin<FileHeader> header { file, page::Id {} };
	const auto overflow = insert_recursive_string(file, header->get_root(), key, rid);
	if (overflow.first != 0) {
		buffer::Pin<PageStringInner> inner { file, header->alloc(), true };
		inner->init(header->get_root());
		const auto result = PageStringInner::insert(inner, overflow.second, overflow.first, 0);
		ASSERT(result.first == 0);
		header->set_root(inner.get_page_id());
	}
}

static bool find_next_value(buffer::Pin<const PageStringLeaf> &page, unsigned int &index, bool reverse, std::optional<std::string_view> key)
{
	std::optional<page::Id> new_page_id;
	buffer::Pin<const PageStringLeaf> new_page;

	unsigned int new_index;

	if (!reverse) {
		if (index + 1 == page->get_count()) {
			new_page_id = page->get_data().next;
		}
		else {
			new_index = index + 1;
		}
	}
	else {
		if (index == 0) {
			new_page_id = page->get_data().prev;
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
			new_index = 0;
		}
		else {
			new_index = new_page->get_count() - 1;
		}
	}

	if (key) {
		auto &page_to_check = new_page_id ? new_page : page;
		if (compare_strings(page_to_check->get_key(new_index), *key) != 0) {
			return false;
		}
	}

	if (new_page_id) {
		page = std::move(new_page);
	}
	index = new_index;

	return true;
}

static std::pair<buffer::Pin<const PageStringLeaf>, unsigned int> find_string_recursive(buffer::Pin<const PageHeader> page, std::string_view key, bool reverse)
{
	if (page->is_leaf) {
		buffer::Pin<const PageStringLeaf> leaf = std::move(page);
		const std::optional<unsigned int> index = reverse ? leaf->find_upper(key) : leaf->find_lower(key);
		if (index) {
			return std::make_pair(std::move(leaf), *index);
		}
		else {
			return {};
		}
	}
	else {
		buffer::Pin<const PageStringInner> inner = std::move(page);
		const page::Id child = reverse ? inner->find_upper(key) : inner->find_lower(key);
		return find_string_recursive(inner.shift(child), key, reverse);
	}
}

// TODO: implement iterator
std::pair<buffer::Pin<const PageStringLeaf>, unsigned int> find_string(catalog::FileId file, std::string_view key, bool reverse)
{
	buffer::Pin<const FileHeader> header { file, page::Id {} };
	auto iter = find_string_recursive(header.shift(header->get_root()), key, reverse);
	if (iter.first != 0) {
		while (find_next_value(iter.first, iter.second, !reverse, key)) {}
	}
	return iter;
}
