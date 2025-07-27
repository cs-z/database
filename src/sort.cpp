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

static void sort_page(const Type &type, const OrderBy &order_by, row::Page *page)
{
	const auto compare = [&type, &order_by, page](row::Page::SlotInfo slot_l, row::Page::SlotInfo slot_r) {
		const u8 *row_l = page->get_row(slot_l);
		const u8 *row_r = page->get_row(slot_r);
		return compare_rows(type, order_by, row_l, row_r);
	};
	page->sort(compare);
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

	void init(const os::TempFile &file, PageId page_begin, PageId page_end)
	{
		this->file = &file;
		this->page_begin = page_begin;
		this->page_end = page_end;

		page_id = page_begin;
		slot = row::Page::Slot {};
	}

	const u8 *next(u32 &size)
	{
		for (;;) {
			if (slot == 0) {
				if (page_id == page_end) {
					return nullptr;
				}
				os::file_read(file->get(), page_id, page.get());
			}
			if (slot == page->get_row_count()) {
				page_id++;
				slot = row::Page::Slot {};
				continue;
			}
			const u8 * const row = page->get_row(slot++, size);
			if (!row) {
				continue;
			}
			return row;
		}
	}

private:

	buffer::Buffer<row::Page> page;

	const os::TempFile *file;
	PageId page_begin, page_end;

	PageId page_id;
	row::Page::Slot slot;
};

class Output
{
public:

	Output(const os::TempFile &file)
		: file { file }
		, page_id {}
		, page_id_begin {}
	{
		page->init();
	}

	void append(const u8 *row, u32 align, u32 size)
	{
		while (!page->append(row, align, size)) {
			write();
		}
		//ASSERT(page->append(row, align, size));
		// TODO: ??
	}

	std::pair<PageId, PageId> end_section()
	{
		write();
		const PageId begin = page_id_begin, end = page_id;
		page_id_begin = page_id;
		return { begin, end };
	}

private:

	void write()
	{
		if (page->get_row_count() > 0) {
			os::file_write(file.get(), page_id++, page.get());
		}
		page->init();
	}

	const os::TempFile &file;
	PageId page_id, page_id_begin;
	buffer::Buffer<row::Page> page;
};

// stores sections of data file, which will be merged together
// needed because variable-length rows
class SectionQueue
{
public:

	struct Section
	{
		PageId begin, end;
	};

	PageId get_size() const { return size; }

	SectionQueue()
		: page_begin { 0 }
		, page_r { 0 }
		, page_w { 0 }
		, slot_r { 0 }
		, slot_w { 0 }
		, size { 0 }
	{
	}

	void push(Section section)
	{
		size++;
		ASSERT(slot_w < SECTION_PER_PAGE);
		buffer_w.get()[(slot_w++).get()] = section;
		if (slot_w == SECTION_PER_PAGE) {
			os::file_write(file.get(), PageId { page_w++ }, buffer_w.get());
			slot_w = row::Page::Slot {};
		}
	}

	Section pop()
	{
		size--;
		if (slot_r == 0 || slot_r == SECTION_PER_PAGE) {
			os::file_read(file.get(), page_r++, buffer_r.get());
			slot_r = row::Page::Slot {};
		}
		return buffer_r.get()[(slot_r++).get()];
	}

	void flush()
	{
		if (slot_w > 0) {
			os::file_write(file.get(), page_w++, buffer_w.get());
			slot_w = row::Page::Slot {};
		}
		page_r = page_begin;
		slot_r = row::Page::Slot {};
		page_begin = page_w;
	}

private:

	static constexpr unsigned int SECTION_PER_PAGE = PAGE_SIZE.get() / sizeof(Section);

	const os::TempFile file;

	PageId page_begin;
	PageId page_r, page_w;
	row::Page::Slot slot_r, slot_w;

	PageId size;

	buffer::Buffer<Section> buffer_r, buffer_w;
};

static os::TempFile merge_sorted_pages(const Type &type, const OrderBy &order_by, os::TempFile file, PageId &page_count_out)
{
	os::TempFile file_src = std::move(file);
	os::TempFile file_dst;

	SectionQueue queue;
	for (PageId page_id {}; page_id < page_count_out; page_id++) {
		queue.push({ page_id, page_id + 1 });
	}
	queue.flush();

	Input inputs[K];
	const u8 *rows[K];
	u32 sizes[K];

	const u32 align = row::calculate_align(type);

	while (queue.get_size() > 1) {

		Output output { file_dst };

		const PageId merges = queue.get_size() / K;
		const PageId remainder = queue.get_size() % K;

		for (PageId i {}; i < merges; i++) {

			for (PageId k {}; k < K; k++) {
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

			for (PageId k {}; k < remainder; k++) {
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

static os::TempFile merge_sort(IterPtr iter, const OrderBy &order_by, PageId &page_count_out)
{
	os::TempFile file;

	// TODO: if parent is materialized, simply copy and sort pages

	PageId page_id = PageId {};
	buffer::Buffer<row::Page> page;
	page->init();

	iter->open();
	for (;;) {
		std::optional<Value> value = iter->next();
		if (!value) {
			break;
		}
		u32 align, size;
		const row::Prefix prefix = row::calculate_layout(*value, align, size);
		const u32 size_padded = size + align - 1;
		if (page->get_free_size() < size_padded) {
			sort_page(iter->type, order_by, page.get());
			os::file_write(file.get(), page_id++, page.get());
			page->init();
		}
		ASSERT(page->get_free_size() >= size_padded);

		ASSERT(size <= page->get_free_size());
		u8 * const row = page->insert(align, size);
		row::write(prefix, *value, row);
	}
	iter->close();

	if (page->get_row_count() > 0) {
		sort_page(iter->type, order_by, page.get());
		os::file_write(file.get(), page_id++, page.get());
	}

	page_count_out = page_id;
	return merge_sorted_pages(iter->type, order_by, std::move(file), page_count_out);
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
		PageId page_count;
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
