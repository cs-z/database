#pragma once

#include "common.hpp"
#include "error.hpp"
#include "page.hpp"

enum class ColumnType : std::uint8_t
{
    BOOLEAN,
    INTEGER,
    REAL,
    VARCHAR,
};

inline bool column_type_is_comparable(ColumnType type)
{
    switch (type)
    {
    case ColumnType::INTEGER:
    case ColumnType::REAL:
    case ColumnType::VARCHAR:
        return true;
    case ColumnType::BOOLEAN:
        return false;
    }
    UNREACHABLE();
}

inline bool column_type_is_arithmetic(ColumnType type)
{
    switch (type)
    {
    case ColumnType::INTEGER:
    case ColumnType::REAL:
        return true;
    case ColumnType::BOOLEAN:
    case ColumnType::VARCHAR:
        return false;
    }
    UNREACHABLE();
}

std::string column_type_to_string(ColumnType type);
void compile_cast(std::optional<ColumnType> from, const std::pair<ColumnType, SourceText>& to);
std::string column_type_to_catalog_string(ColumnType type);
ColumnType  column_type_from_catalog_string(const std::string& name);

class Type
{
  public:
    Type();
    void push(ColumnType column);

    [[nodiscard]] page::Offset get_align() const { return align; }
    [[nodiscard]] std::size_t  size() const { return columns.size(); }
    [[nodiscard]] ColumnType   at(std::size_t index) const { return columns.at(index); }

  private:
    page::Offset            align;
    std::vector<ColumnType> columns;
};
