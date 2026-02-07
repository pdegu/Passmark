#include "device.hpp"
#include "Passmark.hpp"
#include <Windows.h>
#include <iostream>

device::device() : hMutex(NULL), serialNumber(""), type("") {}

device::device(device&& other) noexcept // Logic for move constructor
    : hMutex(other.hMutex), // Copy mutex from temporary device
    serialNumber(other.serialNumber), // Copy serial number from temporary device
    type(other.type) // Copy type from temporary device
{
    other.hMutex = NULL; // temporary device mutex must be NULL after copy or destructor will close copied mutex
}

bool device::tryClaim(std::string sn) {
    // Build unique gloable name for mutex
    std::string mutexName = "Global\\Lock_SN_" + sn;

    // Create/open mutex
    hMutex = CreateMutexA(NULL, FALSE, mutexName.c_str());

    if (hMutex == NULL) return false; // OS failed to try to open mutex (rare)

    // Check if mutex is already claimed
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        CloseHandle(hMutex); // Decrement calls to mutex
        hMutex = NULL;
        return false;
    }

    // Mutex claim is successful
    this->serialNumber = sn;
    return true;
}

device::~device() {
    if (hMutex != NULL) { // Only release if a mutex is claimed
        ReleaseMutex(hMutex);
        CloseHandle(hMutex);
        std::cout << "DEBUG: Released lock for " << serialNumber << std::endl;
    }
}

std::string device::getProfiles() const {
    std::string output = "";

    if (!serialNumber.empty()) {
        std::string commandArg = "-d " + this->serialNumber + " -p";
        output = runCommand(*this, commandArg);
        removeBlankLines(output);
        std:: cout << output << std::endl;
    } else std::cout << "uh oh..." << std::endl;

    return output;
}