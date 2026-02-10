#include "Passmark.hpp"

#include <string>
#include <sstream>
#include <stdexcept>
#include <iostream>
#include <vector>
#include <Windows.h>

bool is_numeric(std::string numStr) {
    for (char c : numStr) {
        if (!isdigit(c)) return false;
    }

    return true;
}

std::vector<tester> getTesters() {
    // Find available Passmark testers
    testerList list = findTesters();

    // User selects tester(s)
    std::cout << "Enter tester(s) to be used. For multiple testers, separate tester numbers with commas. Do not use spaces.\n";
    std::string selection;
    getline(std::cin, selection);
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