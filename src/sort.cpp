#include <algorithm>

#include "sort.hpp"
#include "buffer.hpp"
#include "row.hpp"

// K-way merge sort

static constexpr unsigned int K = 2; // TODO

static bool compare_rows(const Type &type, const OrderBy &order_by, const u8 *row_l, const u8 *row_r)
{
	if (!row_r) {
		return true;
	}
	if (!row_l) {
		return false;
	}
	for (const OrderBy::Column &column : order_by.columns) {
		const int result = row::compare(type, column.column_id, row_l, row_r);
		if (result < 0) {
			return column.asc;
		}
		if (result > 0) {
			return !column.asc;
		}
	}
	return true;
}

static void sort_page(const Type &type, const OrderBy &order_by, page::Slotted<> *page)
{
	std::sort(page->begin(), page->end(),
		[&type, &order_by, page](const page::Slotted<>::Slot &slot_l, const page::Slotted<>::Slot &slot_r) {
			const u8 *entry_l = page->get_entry(slot_l);
			const u8 *entry_r = page->get_entry(slot_r);
			return compare_rows(type, order_by, entry_l, entry_r);
		}
	);
}

static std::optional<unsigned int> next_input(const Type &type, const OrderBy &order_by, const u8 **rows)
{
	const auto compare = [&type, &order_by](const u8 *row_l, const u8 *row_r) {
		return compare_rows(type, order_by, row_l, row_r);
	};
	const u8 **iter = std::min_element(rows, rows + K, compare);
	if (*iter == nullptr) {
		return std::nullopt;
	}
	return std::distance(rows, iter);
}

class Input
{
public:

	void init(const os::TempFile &file, page::Id page_begin, page::Id page_end)
	{
		this->file = &file;
		this->page_begin = page_begin;
		this->page_end = page_end;

		page_id = page_begin;
		entry_id = page::EntryId {};
	}

	const u8 *next(page::Offset &size)
	{
		for (;;) {
			if (entry_id == 0) {
				if (page_id == page_end) {
					return nullptr;
				}
				os::file_read(file->get(), page_id, page.get());
			}
			if (entry_id == page->get_entry_count()) {
				page_id++;
				entry_id = page::EntryId {};
				continue;
			}
			const u8 * const entry = page->get_entry(entry_id++, size);
			if (!entry) {
				continue;
			}
			return entry;
		}
	}

private:

	buffer::Buffer<page::Slotted<>> page;

	const os::TempFile *file;
	page::Id page_begin, page_end;

	page::Id page_id;
	page::EntryId entry_id;
};

class Output
{
public:

	Output(const os::TempFile &file)
		: file { file }
		, page_id {}
		, page_id_begin {}
	{
		page->init({});
	}

	void append(const u8 *row, page::Offset align, page::Offset size)
	{
		for (;;) {
			u8 * const entry = page->insert(align, size, {});
			if (!entry) {
				write();
				continue;
			}
			memcpy(entry, row, size);
			break;
		}
	}

	std::pair<page::Id, page::Id> end_section()
	{
		write();
		const page::Id begin = page_id_begin, end = page_id;
		page_id_begin = page_id;
		return { begin, end };
	}

private:

	void write()
	{
		if (page->get_entry_count() > 0) {
			os::file_write(file.get(), page_id++, page.get());
		}
		page->init({});
	}

	const os::TempFile &file;
	page::Id page_id, page_id_begin;
	buffer::Buffer<page::Slotted<>> page;
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

	page::Id get_size() const { return size; }

	SectionQueue()
		: page_begin { 0 }
		, page_r { 0 }
		, page_w { 0 }
		, entry_r { 0 }
		, entry_w { 0 }
		, size { 0 }
	{
	}

	void push(Section section)
	{
		size++;
		ASSERT(entry_w < SECTION_PER_PAGE);
		buffer_w.get()[(entry_w++).get()] = section;
		if (entry_w == SECTION_PER_PAGE) {
			os::file_write(file.get(), page::Id { page_w++ }, buffer_w.get());
			entry_w = page::EntryId {};
		}
	}

	Section pop()
	{
		size--;
		if (entry_r == 0 || entry_r == SECTION_PER_PAGE) {
			os::file_read(file.get(), page_r++, buffer_r.get());
			entry_r = page::EntryId {};
		}
		return buffer_r.get()[(entry_r++).get()];
	}

	void flush()
	{
		if (entry_w > 0) {
			os::file_write(file.get(), page_w++, buffer_w.get());
			entry_w = page::EntryId {};
		}
		page_r = page_begin;
		entry_r = page::EntryId {};
		page_begin = page_w;
	}

private:

	static constexpr unsigned int SECTION_PER_PAGE = page::SIZE / sizeof(Section);

	const os::TempFile file;

	page::Id page_begin;
	page::Id page_r, page_w;
	page::EntryId entry_r, entry_w;

	page::Id size;

	buffer::Buffer<Section> buffer_r, buffer_w;
};

static os::TempFile merge_sorted_pages(const Type &type, const OrderBy &order_by, os::TempFile file, page::Id &page_count_out)
{
	os::TempFile file_src = std::move(file);
	os::TempFile file_dst;

	SectionQueue queue;
	for (page::Id page_id {}; page_id < page_count_out; page_id++) {
		queue.push({ page_id, page_id + 1 });
	}
	queue.flush();

	Input inputs[K];
	const u8 *rows[K];
	page::Offset sizes[K];

	const page::Offset align = type.get_align();

	while (queue.get_size() > 1) {

		Output output { file_dst };

		const page::Id merges = queue.get_size() / K;
		const page::Id remainder = queue.get_size() % K;

		for (page::Id i {}; i < merges; i++) {

			for (page::Id k {}; k < K; k++) {
				const auto [begin, end] = queue.pop();
				inputs[k.get()].init(file_src, begin, end);
				rows[k.get()] = inputs[k.get()].next(sizes[k.get()]);
			}

			std::optional<unsigned int> input_k = next_input(type, order_by, rows);
			while (input_k) {
				const unsigned int k = *input_k;
				output.append(rows[k], align, sizes[k]);
				rows[k] = inputs[k].next(sizes[k]);
				input_k = next_input(type, order_by, rows);
			}

			const auto [begin, end] = output.end_section();
			queue.push({ begin, end });
		}

		if (remainder > 0) {

			for (page::Id k {}; k < remainder; k++) {
				const auto [begin, end] = queue.pop();
				inputs[k.get()].init(file_src, begin, end);
				rows[k.get()] = inputs[k.get()].next(sizes[k.get()]);
			}

			std::optional<unsigned int> input_k = next_input(type, order_by, rows);
			while (input_k) {
				const unsigned int k = *input_k;
				ASSERT(k < remainder.get()); // TODO: id / int
				output.append(rows[k], align, sizes[k]);
				rows[k] = inputs[k].next(sizes[k]);
				input_k = next_input(type, order_by, rows);
			}

			const auto [begin, end] = output.end_section();
			queue.push({ begin, end });
		}

		queue.flush();

		std::swap(file_src, file_dst);
	}

	ASSERT(queue.get_size() == 1);
	const auto [begin, end] = queue.pop();
	ASSERT(begin == 0);

	page_count_out = end;
	return file_src;
}

static os::TempFile merge_sort(IterPtr iter, const OrderBy &order_by, page::Id &page_count_out)
{
	os::TempFile file;

	// TODO: if parent is materialized, simply copy and sort pages

	page::Id page_id = page::Id {};
	buffer::Buffer<page::Slotted<>> page;
	page->init({});

	const Type &type = iter->type;
	const page::Offset align = type.get_align();

	iter->open();
	for (;;) {
		std::optional<Value> value = iter->next();
		if (!value) {
			break;
		}
		const row::Prefix prefix = row::calculate_layout(*value);

		for (;;) {
			u8 * const entry = page->insert(align, prefix.size, {});
			if (!entry) {
				sort_page(type, order_by, page.get());
				os::file_write(file.get(), page_id++, page.get());
				page->init({});
				continue;
			}
			row::write(prefix, *value, entry);
			break;
		}
	}
	iter->close();

	if (page->get_entry_count() > 0) {
		sort_page(type, order_by, page.get());
		os::file_write(file.get(), page_id++, page.get());
	}

	page_count_out = page_id;
	return merge_sorted_pages(type, order_by, std::move(file), page_count_out);
}

IterSort::IterSort(IterPtr parent, OrderBy columns)
	: Iter { parent->type }
	, parent { std::move(parent) }
	, columns { std::move(columns) }
{
}

void IterSort::open()
{
	if (!sorted_file) {
		page::Id page_count;
		sorted_file = merge_sort(std::move(parent), columns, page_count);
		sorted_iter = std::make_unique<IterScanTemp>(sorted_file->get(), page_count, type);
	}
	sorted_iter->open();
}

void IterSort::close()
{
	sorted_iter->close();
}

std::optional<Value> IterSort::next()
{
	return sorted_iter->next();
}
