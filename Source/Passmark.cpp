#include "Passmark.hpp"

#include <string>
#include <sstream>
#include <stdexcept>
#include <iostream>
#include <vector>
#include <Windows.h>
#include <atomic>

bool is_numeric(const std::string& numStr) {
    for (char c : numStr) {
        if (!isdigit(c)) return false;
    }

    return true;
}

std::string getNumStr(const std::string& inputStr, const size_t& startPos) {
    std::string NumStr = "";

    size_t p = startPos;
    while (p < inputStr.size()) {
        if (isdigit(inputStr[p])) {
            NumStr.push_back(inputStr[p]);
            ++p;
        } else break;
    }

    return NumStr;
}

std::vector<tester> getTesters() {
    // Find available Passmark testers
    testerList list = findTesters();

    // User selects tester(s)
    std::cout << "Enter tester(s) to be used. For multiple testers, separate tester numbers with commas. Do not use spaces.\n\nSelection:\t";
    std::string selection;
    getline(std::cin, selection);
    std::cout << std::endl;
    if (selection.empty()) throw std::runtime_error("No tester(s) selected.");

    // Initialize tester objects
    std::vector<tester> validTesters;
    std::stringstream ss(selection);
    std::string field;
    while (getline(ss, field, ',')) { // Check user selection is valid and store information to tester object vector
        tester placeHolder;
        if (!is_numeric(field)) throw std::runtime_error("Tester selection must be an interger!");
        int testerIdx = std::stoi(field) - 1;

        // Check if tester is in use
        if (placeHolder.tryClaim(list.testers[testerIdx])) {
            placeHolder.assignType(list.type[testerIdx]);
            validTesters.push_back(std::move(placeHolder)); 
        } else throw std::runtime_error(list.testers[testerIdx] + " is in use.");
    }

    return validTesters;
}

std::atomic<bool> g_abortRequested(false);

const char* CtrlCAbort::what() const noexcept {
    return "Ctrl+C abort requested";
}

BOOL WINAPI CtrlHandler(DWORD ctrlType) {
    
    switch (ctrlType)
    {
        case CTRL_C_EVENT:
        case CTRL_BREAK_EVENT:
        case CTRL_CLOSE_EVENT:
            g_abortRequested.store(true, std::memory_order_relaxed);
            // Do NOT throw here, just set the flag
            return TRUE;

        default:
            return FALSE;
    }
}

int colors[] = {10, 11, 14, 13}; // Green, Cyan, Yellow, Magenta