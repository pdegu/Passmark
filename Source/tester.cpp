#include "tester.hpp"

#include <Windows.h>
#include <iostream>
#include <string>
#include <sstream>
#include <stdexcept>
#include <vector>

// ----------------------------------------
// Tester class member function definitions
// ----------------------------------------

testerList findTesters() {
    testerList list;
    tester virtualtester;

    // Lambda to handle tester polling and storing info
    auto poll = [&](const std::string& testerType) {
        virtualtester.assignType(testerType);
        std::string output = runCommand(virtualtester, "-f");
        removeBlankLines(output);
        std::cout << testerType << " testers:\n";
        std::stringstream ss(output);
        std::string line;
        while (getline(ss, line)) {
            size_t pos = line.find("=");
            if (pos != std::string::npos) {
                list.type.push_back(testerType);
                std::string tempStr = line.substr(pos + 1);
                list.testers.push_back(tempStr);
                std::cout << "(" << list.testers.size() << ") " << output << std::endl;
            }
        }
    };

    // Poll PM240 testers
    poll("PM240");

    //Poll PM100 testers
    poll("PM100");

    // Check if no testers were found
    if (list.testers.empty()) throw std::runtime_error("No testers found.");

    return list;
}

tester::tester() : hMutex(NULL), serialNumber(""), type("") {}

tester::tester(tester&& other) noexcept // Logic for move constructor
    : hMutex(other.hMutex), // Copy mutex from temporary tester
    serialNumber(other.serialNumber), // Copy serial number from temporary tester
    type(other.type) // Copy type from temporary tester
{
    other.hMutex = NULL; // temporary tester mutex must be NULL after copy or destructor will close copied mutex
}

tester::~tester() {
    if (hMutex != NULL) { // Only release if a mutex is claimed
        ReleaseMutex(hMutex);
        CloseHandle(hMutex);
        std::cout << "DEBUG: Released lock for " << serialNumber << std::endl;
    }
}

bool tester::tryClaim(std::string sn) {
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

std::string tester::getProfiles() const {
    std::string output = "";

    if (!serialNumber.empty()) {
        std::string commandArg = "-d " + this->serialNumber + " -p";
        output = runCommand(*this, commandArg);
        removeBlankLines(output);
        std:: cout << output << std::endl;
    }

    return output;
}

void tester::assignType(const std::string& typeStr) {
    type = (typeStr == "PM240" || typeStr == "PM100") ? typeStr : "none";
}

bool tester::isPM240() const {
    if (type.empty() || type == "none") throw std::runtime_error("Missing type assignment");
    return (type == "PM240") ? true : false;
}

bool tester::isPM100() const {
    if (type.empty() || type == "none") throw std::runtime_error("Missing type assignment");
    return (type == "PM100") ? true : false;
}

void tester::operateHardware(std::string inputStr) const {
    // Attempt to take mutex lock for tester
    DWORD waitResult = WaitForSingleObject(this->hMutex, 5000); // Wait for 5 seconds to acquire lock before timing out and throwing error

    if (waitResult == WAIT_OBJECT_0) {
        // Worker thread has acquired the lock and can safely run tests
        std::cout << "Lock acquired for " << serialNumber << std::endl;

        // Insert member function here
        std::cout << "Hardcore hardware operating!!!!" << std::endl;
        ReleaseMutex(this->hMutex); // Release lock after testing is done
    } else {
        // FAILURE: Throw an error that the wrapper will catch
        if (waitResult == WAIT_TIMEOUT) {
            throw std::runtime_error("Mutex timeout: Device is busy or stuck.");
        } else if (waitResult == WAIT_ABANDONED) {
            throw std::runtime_error("Mutex abandoned: Previous owner crashed.");
        } else {
            throw std::runtime_error("Mutex failed with system error: " + std::to_string(GetLastError()));
        }
    }
}

// ----------------------------------------
// Other functions
// ----------------------------------------

std::string runCommand(const tester& dev, const std::string& commandArg) {
    std::string commandBase = (dev.isPM240()) ? "USBPDPROConsole.exe " : (dev.isPM100()) ? "USBPDConsole.exe " : "Invalid tester type";
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

DWORD WINAPI TesterThreadWrapper(LPVOID lpParam) {
    threadParams* params = static_cast<threadParams*>(lpParam); // Cast the void pointer to threadParams pointer
    // Ensure pointer isn't null
    if (params == nullptr) return ERROR_INVALID_PARAMETER;

    tester* activeTester = params->activeTester; // Extract the active tester from the parameters
    std::string inputStr = params->input; // Extract the input string from the parameters

    DWORD exitCode = 0;

    try {
        // Jump into class logic
        activeTester->operateHardware(inputStr);
    } catch (const std::exception& e) {
        std::cerr << "Thread error (" << activeTester->serialNumber << "): " << e.what() << std::endl;
        exitCode = ERROR_SERVICE_SPECIFIC_ERROR; // Use a generic error code for exceptions
    } catch (...) {
        std::cerr << "Thread error (" << activeTester->serialNumber << "): Unknown error occurred." << std::endl;
        exitCode = ERROR_PROCESS_ABORTED; // Use a generic error code for unknown exceptions
    }

    delete params; // Clean up dynamically allocated parameters
    
    return exitCode; // Return exit code to indicate success or type of failure
}