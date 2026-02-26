#pragma once

#include "ast.hpp"
#include "lexer.hpp"
#include "type.hpp"

#include <string>

ColumnType   parse_type(const std::string& name); // for catalog
AstStatement parse_statement(Lexer& lexer);
