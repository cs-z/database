#include "row.hpp"
#include "page.hpp"

namespace row
{
	Prefix calculate_layout(const Value &value)
	{
		Prefix prefix;
		prefix.size = value.size() * sizeof(ColumnPrefix);
		for (const ColumnValue &column_value : value) {
			const auto [column_align, column_size] = std::visit(Overload{
				[](const ColumnValueNull &) {
					const page::Offset column_align = 1;
					const page::Offset column_size = 0;
					return std::make_pair(column_align, column_size);
				},
				[](const ColumnValueBoolean &) {
					const page::Offset column_align = alignof(ColumnValueBoolean);
					const page::Offset column_size = sizeof(ColumnValueBoolean);
					return std::make_pair(column_align, column_size);
				},
				[](const ColumnValueInteger &) {
					const page::Offset column_align = alignof(ColumnValueInteger);
					const page::Offset column_size = sizeof(ColumnValueInteger);
					return std::make_pair(column_align, column_size);
				},
				[](const ColumnValueReal &) {
					const page::Offset column_align = alignof(ColumnValueReal);
					const page::Offset column_size = sizeof(ColumnValueReal);
					return std::make_pair(column_align, column_size);
				},
				[](const ColumnValueVarchar &column_value) {
					const page::Offset column_align = alignof(char);
					const page::Offset column_size = column_value.size();
					return std::make_pair(column_align, column_size);
				},
			}, column_value);
			prefix.size = align_up(prefix.size, column_align);
			prefix.columns.push_back({ column_size != 0 ? prefix.size : page::Offset {}, column_size });
			prefix.size += column_size;
		}
		return prefix;
	}

	template <typename T>
	T *get_column(u8 *row, ColumnId column)
	{
		const page::Offset offset = reinterpret_cast<ColumnPrefix *>(row)[column.get()].offset;
		return reinterpret_cast<T *>(reinterpret_cast<u8 *>(row) + offset);
	}

	void write(const Prefix &prefix, const Value &value, u8 *row)
	{
		ASSERT(prefix.columns.size() == value.size());
		memcpy(row, prefix.columns.data(), prefix.columns.size() * sizeof(ColumnPrefix));
		for (ColumnId column_id {}; column_id < value.size(); column_id++) {
			std::visit(Overload{
				[](const ColumnValueNull &) {
				},
				[row, column_id](const ColumnValueBoolean &value) {
					*get_column<ColumnValueBoolean>(row, column_id) = value;
				},
				[row, column_id](const ColumnValueInteger &value) {
					*get_column<ColumnValueInteger>(row, column_id) = value;
				},
				[row, column_id](const ColumnValueReal &value) {
					*get_column<ColumnValueReal>(row, column_id) = value;
				},
				[row, column_id](const ColumnValueVarchar &value) {
					memcpy(get_column<char>(row, column_id), value.data(), value.size()); // NOLINT(bugprone-not-null-terminated-result)
				},
			}, value[column_id.get()]);
		}
	}

	ColumnPrefix get_prefix(const u8 *row, ColumnId column)
	{
		return reinterpret_cast<const ColumnPrefix *>(row)[column.get()];
	}

	template <typename T>
	const T *get_column(const u8 *row, ColumnPrefix prefix)
	{
		return reinterpret_cast<const T *>(reinterpret_cast<const u8 *>(row) + prefix.offset);
	}

	Value read(const Type &type, const u8 *row)
	{
		Value value;
		for (ColumnId column_id {}; column_id < type.size(); column_id++) {
			const ColumnPrefix prefix = get_prefix(row, column_id);
			if (prefix.offset == 0) {
				value.push_back(ColumnValueNull {});
				continue;
			}
			switch (type.at(column_id.get())) {
				case ColumnType::BOOLEAN: {
					value.push_back(*get_column<ColumnValueBoolean>(row, prefix));
					break;
				}
				case ColumnType::INTEGER: {
					value.push_back(*get_column<ColumnValueInteger>(row, prefix));
					break;
				}
				case ColumnType::REAL: {
					value.push_back(*get_column<ColumnValueReal>(row, prefix));
					break;
				}
				case ColumnType::VARCHAR: {
					const char * const begin = get_column<char>(row, prefix);
					value.push_back(ColumnValueVarchar { begin, prefix.size });
					break;
				}
			}
		}
		return value;
	}

	int compare(const Type &type, ColumnId column, const u8 *row_l, const u8 *row_r)
	{
		const ColumnPrefix prefix_l = get_prefix(row_l, column);
		const ColumnPrefix prefix_r = get_prefix(row_r, column);
		if (prefix_r.offset == 0) {
			return -1;
		}
		if (prefix_l.offset == 0) {
			return +1;
		}
		switch (type.at(column.get())) {
			case ColumnType::BOOLEAN: {
				UNREACHABLE();
			}
			case ColumnType::INTEGER: {
				const ColumnValueInteger column_value_l = *get_column<ColumnValueInteger>(row_l, prefix_l);
				const ColumnValueInteger column_value_r = *get_column<ColumnValueInteger>(row_r, prefix_r);
				if (column_value_l < column_value_r) {
					return -1;
				}
				if (column_value_l > column_value_r) {
					return +1;
				}
				return 0;
			}
			case ColumnType::REAL: {
				const ColumnValueReal column_value_l = *get_column<ColumnValueReal>(row_l, prefix_l);
				const ColumnValueReal column_value_r = *get_column<ColumnValueReal>(row_r, prefix_r);
				if (column_value_l < column_value_r) {
					return -1;
				}
				if (column_value_l > column_value_r) {
					return +1;
				}
				return 0;
			}
			case ColumnType::VARCHAR: {
				const char *column_value_l = get_column<char>(row_l, prefix_l);
				const char *column_value_r = get_column<char>(row_r, prefix_r);
				return compare_strings({ column_value_l, prefix_l.size }, { column_value_r, prefix_r.size });
			}
		}
		UNREACHABLE();
	}

	int compare(const Type &type, ColumnId column, const u8 *row_l, const Value &row_r)
	{
		const ColumnPrefix prefix_l = get_prefix(row_l, column);
		const ColumnValue &value_r = row_r.at(column.get());
		if (std::holds_alternative<ColumnValueNull>(value_r)) {
			return -1;
		}
		if (prefix_l.offset == 0) {
			return +1;
		}
		switch (type.at(column.get())) {
			case ColumnType::BOOLEAN: {
				UNREACHABLE();
			}
			case ColumnType::INTEGER: {
				const ColumnValueInteger column_value_l = *get_column<ColumnValueInteger>(row_l, prefix_l);
				const ColumnValueInteger column_value_r = std::get<ColumnValueInteger>(value_r);
				if (column_value_l < column_value_r) {
					return -1;
				}
				if (column_value_l > column_value_r) {
					return +1;
				}
				return 0;
			}
			case ColumnType::REAL: {
				const ColumnValueReal column_value_l = *get_column<ColumnValueReal>(row_l, prefix_l);
				const ColumnValueReal column_value_r = std::get<ColumnValueReal>(value_r);
				if (column_value_l < column_value_r) {
					return -1;
				}
				if (column_value_l > column_value_r) {
					return +1;
				}
				return 0;
			}
			case ColumnType::VARCHAR: {
				const char *column_value_l = get_column<char>(row_l, prefix_l);
				return compare_strings({ column_value_l, prefix_l.size }, std::get<ColumnValueVarchar>(value_r));
			}
		}
		UNREACHABLE();
	}
}