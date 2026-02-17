#pragma once

// Project headers
#include "tester.hpp"
#include "ThreadBridge.hpp"

// Standard headers
#include <vector>
#include <Windows.h>
#include <atomic>
#include <stdexcept>

// Check if string is numeric
bool is_numeric(std::string numStr);

// Return string with only numeric characters
std::string getNumStr(const std::string& inputStr, const size_t& startPos);

// Check which testers are available and claim
std::vector<tester> getTesters();

std::atomic<bool> g_abortRequested(false);

struct CtrlCAbort : public std::exception {
    const char* what() const noexcept override {
        return "Ctrl+C abort requested";
    }
};

BOOL WINAPI CtrlHandler(DWORD ctrlType);