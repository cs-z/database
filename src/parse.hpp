#pragma once

#include "ast.hpp"
#include "lexer.hpp"
#include "type.hpp"

#include <string>

[[nodiscard]] ColumnType   ParseType(const std::string& name); // for catalog
[[nodiscard]] AstStatement ParseStatement(Lexer& lexer);
