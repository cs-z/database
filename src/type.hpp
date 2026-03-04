#pragma once

#include "common.hpp"
#include "error.hpp"
#include "page.hpp"

#include <optional>
#include <string>
#include <vector>

enum class ColumnType : std::uint8_t
{
    BOOLEAN,
    INTEGER,
    REAL,
    VARCHAR,
};

// TODO: make constexpr
inline bool ColumnTypeIsComparable(ColumnType type)
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

// TODO: make constexpr
inline bool ColumnTypeIsArithmetic(ColumnType type)
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

std::string ColumnTypeToString(ColumnType type);
void CompileCast(std::optional<ColumnType> from, const std::pair<ColumnType, SourceText>& to);
std::string ColumnTypeToCatalogString(ColumnType type);
ColumnType  ColumnTypeFromCatalogString(const std::string& name);

class Type
{
public:
    Type(); // TODO: reserve?
    void Push(ColumnType column);

    [[nodiscard]] page::Offset GetAlign() const
    {
        return align_;
    }
    [[nodiscard]] std::size_t Size() const
    {
        return columns_.size();
    }
    [[nodiscard]] ColumnType At(std::size_t index) const
    {
        return columns_.at(index);
    }

private:
    page::Offset            align_;
    std::vector<ColumnType> columns_;
};
