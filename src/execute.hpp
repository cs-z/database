#pragma once

#include "compile.hpp"

std::vector<Value> execute_internal_statement(const std::string& source);
void               execute_statement(const Statement& statement);
