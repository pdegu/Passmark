#pragma once

// Project headers
#include "tester.hpp"
#include "ThreadBridge.hpp"

// Standard headers
#include <vector>
#include <Windows.h>

// Check if string is numeric
bool is_numeric(std::string numStr);

// Check which testers are available and claim
std::vector<tester> getTesters();