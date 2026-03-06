#include "execute.hpp"
#include "ast.hpp"
#include "buffer.hpp"
#include "catalog.hpp"
#include "common.hpp"
#include "compile.hpp"
#include "error.hpp"
#include "fst.hpp"
#include "lexer.hpp"
#include "page.hpp"
#include "parse.hpp"
#include "row.hpp"
#include "row_id.hpp"
#include "token.hpp"
#include "type.hpp"
#include "value.hpp"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <optional>
#include <ratio>
#include <string>
#include <utility>
#include <variant>
#include <vector>

[[nodiscard]] std::vector<Value> ExecuteIinternalStatement(const std::string& source)
{
    try
    {
        Lexer        lexer{source};
        AstStatement ast = ParseStatement(lexer);
        lexer.Expect(Token::kEnd);
        const Statement    statement = CompileStatement(ast);
        std::vector<Value> values;
        if (const Query* const query = std::get_if<Query>(&statement))
        {
            query->iter->Open();
            for (;;)
            {
                std::optional<Value> value = query->iter->Next();
                if (!value)
                {
                    break;
                }
                values.push_back(std::move(*value));
            }
            query->iter->Close();
        }
        else
        {
            ExecuteStatement(statement);
        }
        return values;
    }
    catch (const ClientError& error)
    {
        std::string message = "[ " + source + " ] -> " + error.what();
        throw ServerError{"internal error: " + std::move(message)};
    }
    catch (const ServerError& error)
    {
        std::string message = "[ " + source + " ] -> " + error.what();
        throw ServerError{"internal error: " + std::move(message)};
    }
}

static void ExecuteCreateTable(const CreateTable& statement)
{
    catalog::CreateTable(statement.name, statement.columns);
}

static void ExecuteDropTable(const DropTable& statement)
{
    catalog::DropTable(statement.table_id);
}

static void ExecuteInsertValue(const InsertValue& statement)
{
    const row::Prefix  prefix      = row::CalculateLayout(statement.value);
    const page::Offset align       = statement.type.GetAlign();
    const page::Offset size_padded = prefix.size + align - 1;

    const auto [file_fst, file_dat] = catalog::GetTableFileIds(statement.table_id);
    const auto [page_id, append]    = fst::FindOrAppend(file_fst, size_padded);
    const buffer::Pin<page::Slotted<>> page{file_dat, page_id, append};
    if (append)
    {
        page->Init({});
    }

    page::Offset free_size{};
    U8* const    row = page->Insert(align, prefix.size, {}, &free_size);
    ASSERT(row);
    row::Write(prefix, statement.value, row);

    fst::Update(file_fst, page_id, free_size);
}

[[nodiscard]] static std::string Pad(const std::string& string, std::size_t width, bool left)
{
    ASSERT(string.size() <= width);
    const std::string space(width - string.size(), ' ');
    return left ? string + space : space + string;
}

static void ExecuteQuery(const Query& query)
{
    const auto time_start = std::chrono::high_resolution_clock::now();
    query.iter->Open();

    std::vector<Value> values;
    unsigned int       count = 0;
    while (!query.limit || count < *query.limit)
    {
        std::optional<Value> value = query.iter->Next();
        if (value)
        {
            values.push_back(std::move(*value));
            count++;
        }
        else
        {
            break;
        }
    }

    query.iter->Close();
    const auto time_end = std::chrono::high_resolution_clock::now();
    const std::chrono::duration<double, std::milli> time_delta = time_end - time_start;

    std::vector<std::vector<std::string>> rows;
    std::vector<std::size_t>              max_sizes;

    for (const auto& [name, type] : query.columns)
    {
        max_sizes.push_back(name.size());
    }

    for (const Value& value : values)
    {
        ASSERT(value.size() == query.columns.size());
        std::vector<std::string> strings;
        for (std::size_t i = 0; i < query.columns.size(); i++)
        {
            std::string string = ColumnValueToString(value[i], false);
            max_sizes[i]       = std::max(max_sizes[i], string.size());
            strings.push_back(std::move(string));
        }
        rows.push_back(std::move(strings));
    }

    for (std::size_t i = 0; i < query.columns.size(); i++)
    {
        const std::string string(max_sizes[i] + 2, '-');
        std::printf("+%s", string.c_str());
    }
    std::printf("+\n");

    for (std::size_t i = 0; i < query.columns.size(); i++)
    {
        const std::string string = Pad(query.columns[i].first, max_sizes[i], true);
        std::printf("| %s ", string.c_str());
    }
    std::printf("|\n");

    for (std::size_t i = 0; i < query.columns.size(); i++)
    {
        const std::string string(max_sizes[i] + 2, '-');
        std::printf("+%s", string.c_str());
    }
    std::printf("+\n");

    for (const std::vector<std::string>& row : rows)
    {
        for (std::size_t i = 0; i < query.columns.size(); i++)
        {
            const std::string string =
                Pad(row[i], max_sizes[i], !ColumnTypeIsArithmetic(query.columns[i].second));
            std::printf("| %s ", string.c_str());
        }
        std::printf("|\n");
    }

    for (std::size_t i = 0; i < query.columns.size(); i++)
    {
        const std::string string(max_sizes[i] + 2, '-');
        std::printf("+%s", string.c_str());
    }
    std::printf("+\n");

    std::printf("(%u rows in %.1lf ms)\n\n", count, time_delta.count());
}

static void ExecuteTruncate(const TruncateTable& statement)
{
    catalog::TruncateTable(statement.table_id);
}

static void ExecuteDelete(const DeleteConditional& statement)
{
    const auto file_id = catalog::GetTableFileIds(statement.table_id).dat; // TODO
    statement.iter->Open();
    for (;;)
    {
        auto row = statement.iter->Next();
        if (row.has_value())
        {
            const auto row_id              = std::get<ColumnValueInteger>(row->back());
            const auto [page_id, entry_id] = UnpackRowId(row_id);
            const buffer::Pin<page::Slotted<>> page{file_id, page_id};
            page->Remove(entry_id);
        }
        else
        {
            break;
        }
    }
    statement.iter->Close();
}

void ExecuteStatement(const Statement& statement)
{
    std::visit(Overload{[](const CreateTable& statement) { ExecuteCreateTable(statement); },
                        [](const DropTable& statement) { ExecuteDropTable(statement); },
                        [](const InsertValue& statement) { ExecuteInsertValue(statement); },
                        [](const Query& statement) { ExecuteQuery(statement); },
                        [](const TruncateTable& statement) { ExecuteTruncate(statement); },
                        [](const DeleteConditional& statement) { ExecuteDelete(statement); }},
               statement);
}
