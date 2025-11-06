#include <chrono>

#include "execute.hpp"
#include "parse.hpp"
#include "compile.hpp"
#include "catalog.hpp"
#include "row.hpp"
#include "fst.hpp"
#include "aggregate.hpp"

std::vector<Value> execute_internal_statement(const std::string &source)
{
	try {
		Lexer lexer { source };
		AstStatement ast = parse_statement(lexer);
		lexer.expect(Token::End);
		const Statement statement = compile_statement(ast);
		std::vector<Value> values;
		if (const Query * const query = std::get_if<Query>(&statement)) {
			query->iter->open();
			for (;;) {
				std::optional<Value> value = query->iter->next();
				if (!value) {
					break;
				}
				values.push_back(std::move(*value));
			}
			query->iter->close();
		}
		else {
			execute_statement(statement);
		}
		return values;
	}
	catch (const ClientError &error) {
		std::string message = "[ " + source + " ] -> " + error.what();
		throw ServerError { "internal error: " + std::move(message) };
	}
	catch (const ServerError &error) {
		std::string message = "[ " + source + " ] -> " + error.what();
		throw ServerError { "internal error: " + std::move(message) };
	}
}

static void execute_create_table(const CreateTable &statement)
{
	catalog::create_table(statement.table_name, statement.columns);
}

static void execute_insert_value(const InsertValue &statement)
{
	const row::Prefix prefix = row::calculate_layout(statement.value);
	const page::Offset align = statement.type.get_align();
	const page::Offset size_padded = prefix.size + align - 1;

	const auto [file_fst, file_dat] = catalog::get_table_files(statement.table_id);
	const auto [page_id, append] = fst::find_or_append(file_fst, size_padded);
	const buffer::Pin<page::Slotted<>> page { file_dat, page_id, append };
	if (append) {
		page->init({});
	}

	page::Offset free_size;
	u8 * const row = page->insert(align, prefix.size, {}, &free_size);
	ASSERT(row);
	row::write(prefix, statement.value, row);

	fst::update(file_fst, page_id, free_size);
}

static std::string pad(const std::string &string, std::size_t width, bool left)
{
	ASSERT(string.size() <= width);
	const std::string space(width - string.size(), ' ');
	return left ? string + space : space + string;
}

static void execute_query(const Query &query)
{
	const auto time_start = std::chrono::high_resolution_clock::now();
	query.iter->open();

	std::vector<Value> values;
	unsigned int count = 0;
	while (!query.limit || count < *query.limit) {
		const std::optional<Value> value = query.iter->next();
		if (value) {
			values.push_back(std::move(*value));
			count++;
		}
		else {
			break;
		}
	}

	query.iter->close();
	const auto time_end = std::chrono::high_resolution_clock::now();
	const std::chrono::duration<double, std::milli> time_delta = time_end - time_start;

	std::vector<std::vector<std::string>> rows;
	std::vector<std::size_t> max_sizes;

	for (const auto &[name, type] : query.columns) {
		max_sizes.push_back(name.size());
	}

	for (const Value &value : values) {
		ASSERT(value.size() == query.columns.size());
		std::vector<std::string> strings;
		for (std::size_t i = 0; i < query.columns.size(); i++) {
			std::string string = column_value_to_string(value[i], false);
			max_sizes[i] = std::max(max_sizes[i], string.size());
			strings.push_back(std::move(string));
		}
		rows.push_back(std::move(strings));
	}

	for (std::size_t i = 0; i < query.columns.size(); i++) {
		const std::string string(max_sizes[i] + 2, '-');
		std::printf("+%s", string.c_str());
	}
	std::printf("+\n");

	for (std::size_t i = 0; i < query.columns.size(); i++) {
		const std::string string = pad(query.columns[i].first, max_sizes[i], true);
		std::printf("| %s ", string.c_str());
	}
	std::printf("|\n");

	for (std::size_t i = 0; i < query.columns.size(); i++) {
		const std::string string(max_sizes[i] + 2, '-');
		std::printf("+%s", string.c_str());
	}
	std::printf("+\n");

	for (const std::vector<std::string> &row : rows) {
		for (std::size_t i = 0; i < query.columns.size(); i++) {
			const std::string string = pad(row[i], max_sizes[i], !column_type_is_arithmetic(query.columns[i].second));
			std::printf("| %s ", string.c_str());
		}
		std::printf("|\n");
	}

	for (std::size_t i = 0; i < query.columns.size(); i++) {
		const std::string string(max_sizes[i] + 2, '-');
		std::printf("+%s", string.c_str());
	}
	std::printf("+\n");

	std::printf("(%u rows in %.1lf ms)\n", count, time_delta.count());
}

void execute_statement(const Statement &statement)
{
	return std::visit(Overload {
		[](const CreateTable &statement) {
			execute_create_table(statement);
		},
		[](const InsertValue &statement) {
			execute_insert_value(statement);
		},
		[](const Query &statement) {
			execute_query(statement);
		},
	}, statement);
}
