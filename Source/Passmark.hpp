#pragma once
#include "device.hpp"

#include <Windows.h>
#include <string>
#include <stdexcept>
#include <iostream>
#include <sstream>
#include <vector>
#include <algorithm>

// (((())))
// (((())))
// (((())))
// (((())))
// NEED TO MOVE runCommand INTO ITS OWN CPP FILE. AND GENERALLY FUNCTION DECLARATIONS SHOULD BE IN CPP FILES, NOT HEADERS. NEED TO REVIEW CODE AND MODIFY ACCORDINGLY. 
// (((())))
// (((())))
// (((())))
// (((())))

// Run Passmark executable from cmd prompt and return info provided
// 
// NOTE:
// *****************************************************************
// - Avoid using runCommand outside of device.cpp
// - Define new member function for device if needed
// *****************************************************************
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

// Remove blank lines from Passmark console output string
void removeBlankLines(std::string& string_to_filter) {
    // Remove ' ' if followed by '\n'
    for (size_t i = 0; i < string_to_filter.size() - 1; ++i) {
        if (isspace(string_to_filter[i]) && string_to_filter[i + 1] == '\n') {
            string_to_filter.erase(i, 1);
        }
    }
}

struct deviceList {
    std::vector<std::string> devices;
    std::vector<std::string> type;
};

// Find all available PM240 and PM100 devices
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

std::vector<device> getDevices() {
    // Find available Passmark devices
    deviceList list;
    list = findDevices();

    // User selects device(s)
    std::cout << "Enter device(s) to be used. For multiple devices, separate device numbers with commas. Do not use spaces.\n";
    std::string selection;
    getline(std::cin, selection);

    // Initialize device objects
    std::vector<device> testDevice;
    std::stringstream ss(selection);
    std::string field;
    while (getline(ss, field, ',')) { // Check user selection is valid and store information to device object vector
        device placeHolder;
        int deviceIdx = std::stoi(field) - 1;

        // Check if device is in use
        if (placeHolder.tryClaim(list.devices[deviceIdx])) {
            placeHolder.assignType(list.type[deviceIdx]);
            testDevice.push_back(std::move(placeHolder)); 
        } else throw std::runtime_error(list.devices[deviceIdx] + " is in use.");
    }

    return testDevice;
}