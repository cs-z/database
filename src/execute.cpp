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
	catalog::create_table(statement.table_name, statement.table_def);
}

static void execute_insert_value(const InsertValue &statement)
{
	page::Offset align, size;
	const row::Prefix prefix = row::calculate_layout(statement.value, align, size);
	const page::Offset size_padded = static_cast<page::Offset>(size + align - 1);

	const auto [file_fst, file_dat] = catalog::get_table_files(statement.table_id);
	const auto [page_id, append] = fst::find_or_append(file_fst, size_padded);
	const buffer::Pin<page::PageSlotted> page { file_dat, page_id, append };
	if (append) {
		page->init();
	}
	ASSERT(page->get_free_size() >= size_padded);

	u8 * const row = page->insert(align, size);
	row::write(prefix, statement.value, row);

	const page::Offset free_size = page->get_free_size();
	fst::update(file_fst, page_id, free_size);
}

static std::string pad(const std::string &string, size_t width, bool left)
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
	std::vector<size_t> max_sizes;

	for (const auto &[name, type] : query.table_def) {
		max_sizes.push_back(name.size());
	}

	for (const Value &value : values) {
		ASSERT(value.size() == query.table_def.size());
		std::vector<std::string> strings;
		for (size_t i = 0; i < query.table_def.size(); i++) {
			std::string string = column_value_to_string(value[i], false);
			max_sizes[i] = std::max(max_sizes[i], string.size());
			strings.push_back(std::move(string));
		}
		rows.push_back(std::move(strings));
	}

	for (size_t i = 0; i < query.table_def.size(); i++) {
		const std::string string(max_sizes[i] + 2, '-');
		std::printf("+%s", string.c_str());
	}
	std::printf("+\n");

	for (size_t i = 0; i < query.table_def.size(); i++) {
		const std::string string = pad(query.table_def[i].name, max_sizes[i], true);
		std::printf("| %s ", string.c_str());
	}
	std::printf("|\n");

	for (size_t i = 0; i < query.table_def.size(); i++) {
		const std::string string(max_sizes[i] + 2, '-');
		std::printf("+%s", string.c_str());
	}
	std::printf("+\n");

	for (const std::vector<std::string> &row : rows) {
		for (size_t i = 0; i < query.table_def.size(); i++) {
			const std::string string = pad(row[i], max_sizes[i], !column_type_is_arithmetic(query.table_def[i].type));
			std::printf("| %s ", string.c_str());
		}
		std::printf("|\n");
	}

	for (size_t i = 0; i < query.table_def.size(); i++) {
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

/*void execute_query(const std::string &source)
{

	std::printf("rows: %u\n", count);
	std::printf("time: %.1lf ms\n", time_delta.count());
	std::printf("\n");
}*/