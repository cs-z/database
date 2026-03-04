#pragma once

#include "compile.hpp"

#include <string>
#include <vector>

[[nodiscard]] std::vector<Value> ExecuteIinternalStatement(const std::string& source);
void                             ExecuteStatement(const Statement& statement);
