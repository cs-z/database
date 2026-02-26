#pragma once

#include "compile.hpp"

#include <string>
#include <vector>

std::vector<Value> execute_internal_statement(const std::string& source);
void               execute_statement(const Statement& statement);
