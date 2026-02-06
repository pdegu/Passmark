#include <Windows.h>
#include <string>
#include <stdexcept>
#include <iostream>
#include <sstream>
#include <vector>
#include <algorithm>

class device
{
private:
    std::string type;

public:
    std::string serialNumber;
    
    void assignType(const std::string& typeStr) {
        type = (typeStr == "PM240" || typeStr == "PM100") ? typeStr : "none";
    }

    bool isPM240() {
        if (type.empty() || type == "none") throw std::runtime_error("Missing type assignment");
        return (type == "PM240") ? true : false;
    }

    bool isPM100() {
        if (type.empty() || type == "none") throw std::runtime_error("Missing type assignment");
        return (type == "PM100") ? true : false;
    }
};

// Run Passmark executable from cmd prompt and return info provided
std::string runCommand(device tester, const std::string& commandArg) {
    std::string commandBase = (tester.isPM240()) ? "USBPDPROConsole.exe " : (tester.isPM100()) ? "USBPDConsole.exe " : "Invalid device type";
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
    int count = 0, countPM240 = 0, countPM100 = 0;
    std::vector<std::string> devices;
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
                list.count += 1;
                std::cout << "(" << list.count << ")\t" << output << std::endl;
                std::string tempStr = line.substr(pos + 1);
                list.devices.push_back(tempStr);
                if (deviceType == "PM240") list.countPM240 += 1;
                else list.countPM100 += 1;
            }
        }
    };

    // Poll PM240 devices
    poll("PM240");

    //Poll PM100 devices
    poll("PM100");

    return list;
}