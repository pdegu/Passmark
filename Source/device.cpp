#include "device.hpp"

#include <Windows.h>
#include <iostream>
#include <string>
#include <sstream>
#include <stdexcept>
#include <vector>

// ----------------------------------------
// Device class member function definitions
// ----------------------------------------

deviceList findDevices() {
    deviceList list;
    device virtualDevice;

    // Lambda to handle device polling and storing info
    auto poll = [&](const std::string& deviceType) {
        virtualDevice.assignType(deviceType);
        std::string output = runCommand(virtualDevice, "-f");
        removeBlankLines(output);
        std::cout << deviceType << " devices:\n";
        std::stringstream ss(output);
        std::string line;
        while (getline(ss, line)) {
            size_t pos = line.find("=");
            if (pos != std::string::npos) {
                list.type.push_back(deviceType);
                std::string tempStr = line.substr(pos + 1);
                list.devices.push_back(tempStr);
                std::cout << "(" << list.devices.size() << ") " << output << std::endl;
            }
        }
    };

    // Poll PM240 devices
    poll("PM240");

    //Poll PM100 devices
    poll("PM100");

    return list;
}

device::device() : hMutex(NULL), serialNumber(""), type("") {}

device::device(device&& other) noexcept // Logic for move constructor
    : hMutex(other.hMutex), // Copy mutex from temporary device
    serialNumber(other.serialNumber), // Copy serial number from temporary device
    type(other.type) // Copy type from temporary device
{
    other.hMutex = NULL; // temporary device mutex must be NULL after copy or destructor will close copied mutex
}

device::~device() {
    if (hMutex != NULL) { // Only release if a mutex is claimed
        ReleaseMutex(hMutex);
        CloseHandle(hMutex);
        std::cout << "DEBUG: Released lock for " << serialNumber << std::endl;
    }
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

void device::assignType(const std::string& typeStr) {
    type = (typeStr == "PM240" || typeStr == "PM100") ? typeStr : "none";
}

bool device::isPM240() const {
    if (type.empty() || type == "none") throw std::runtime_error("Missing type assignment");
    return (type == "PM240") ? true : false;
}

bool device::isPM100() const {
    if (type.empty() || type == "none") throw std::runtime_error("Missing type assignment");
    return (type == "PM100") ? true : false;
}

// ----------------------------------------
// Other functions
// ----------------------------------------

std::string runCommand(const device& dev, const std::string& commandArg) {
    std::string commandBase = (dev.isPM240()) ? "USBPDPROConsole.exe " : (dev.isPM100()) ? "USBPDConsole.exe " : "Invalid device type";
    std::string command = commandBase + commandArg;
    
    HANDLE hRead, hWrite;
    SECURITY_ATTRIBUTES sa = { sizeof(SECURITY_ATTRIBUTES), NULL, TRUE};

    // Create pipe for child process output
    if (!CreatePipe(&hRead, &hWrite, &sa, 0)) {
        throw std::runtime_error("Failed to create pipe");
    }
    
    // Ensure program doesn't pass read side of pipe to command
    SetHandleInformation(hRead, HANDLE_FLAG_INHERIT, 0);

    // Set up output redirection
    STARTUPINFOA si = {};
    si.cb = sizeof(si);
    si.dwFlags |= STARTF_USESTDHANDLES;
    si.hStdOutput = hWrite;
    si.hStdError = hWrite; // Redirect stderr to the same pipe

    // Build the command line
    PROCESS_INFORMATION pi = {};
    std::string cmdLine = "cmd.exe /C " + command; // Use cmd.exe to run the command

    //Create child process
    if (!CreateProcessA(
        NULL,
        &cmdLine[0],    // Command line must be mutable
        NULL, NULL, TRUE,
        0, NULL, NULL,
        &si, &pi)) {
            CloseHandle(hWrite);
            CloseHandle(hRead);
            throw std::runtime_error("Failed to create process");
        }

    CloseHandle(hWrite); // Close the write end of the pipe in the parent process

    // Read output from the pipe
    char buffer[128];
    DWORD bytesRead;
    std::string output;
    while (ReadFile(hRead, buffer, sizeof(buffer) - 1, &bytesRead, NULL) && bytesRead > 0) {
        buffer[bytesRead] = '\0'; // Null-terminate the string
        output += buffer;
    }

    CloseHandle(hRead);
    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD exitCode = 0;
    GetExitCodeProcess(pi.hProcess, &exitCode);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    return output;
}

void removeBlankLines(std::string& string_to_filter) {
    // Remove ' ' if followed by '\n'
    for (size_t i = 0; i < string_to_filter.size() - 1; ++i) {
        if (isspace(string_to_filter[i]) && string_to_filter[i + 1] == '\n') {
            string_to_filter.erase(i, 1);
        }
    }
}