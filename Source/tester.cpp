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

    //Poll PM125 testers
    poll("PM125");

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

   // Move the Wait logic here. Using 0ms timeout for an immediate check.
    DWORD waitResult = WaitForSingleObject(hMutex, 0);

    if (waitResult == WAIT_OBJECT_0 || waitResult == WAIT_ABANDONED) {
        // We successfully took ownership of the lock (Fresh or Abandoned)
        std::cout << "DEBUG: Lock acquired for " << sn << std::endl;
        this->serialNumber = sn;
        return true;
    } 

    // If we get here, someone else owns it (WAIT_TIMEOUT)
    CloseHandle(hMutex);
    hMutex = NULL;
    return false;
}

std::string tester::getProfiles(bool toConsole) const {
    std::string output = runCommand(*this, "-p");
    if (output.empty()) {
        std::string errorMsg = (this->serialNumber.empty()) ? "" : "(" + this->serialNumber + ") ";
        throw std::runtime_error(errorMsg + "No response from tester.");
    }

    removeBlankLines(output);
    if (toConsole) std:: cout << output << std::endl;

    return output;
}

tester::ProfileInfo tester::getProfileInfo(std::string profile) const {
    ProfileInfo info;
    std::string searchStr = "INDEX:" + profile;
    std::string output = this->getProfiles(false);
    size_t foundPos = output.find(searchStr);
    if (foundPos != std::string::npos) { // match for searchStr was found
        size_t tPos1 = output.find("TYPE:", foundPos) + 5;
        size_t tPos2 = output.find(",", foundPos);
        std::string type = output.substr(tPos1, tPos2 - tPos1);

         // Modify this string to add additional types
         std::vector<std::string> VariableVoltageTypes{"PD-APDO", "PD-PPS", "QC3"};

        // Set variable voltage flag if detected
        for (std::string s : VariableVoltageTypes) {
            if (type == s) info.isVariableVoltage = true;
        }

        // Populate voltage range
        size_t vPos1 = output.find("V:") + 2;
        size_t vPos2 = output.find("mV");
        info.voltageRange = output.substr(vPos1, vPos2 - vPos1);

        // Populate max current
        size_t iPos1 = output.find("I:") + 2;
        size_t iPos2 = output.find("mA");
        info.maxCurrent = output.substr(iPos1, iPos2 - iPos1);
    } else throw std::runtime_error("(" + this->serialNumber + ") Profile not found.");

    return info;
}

void tester::assignType(const std::string& typeStr) {
    type = (typeStr == "PM240" || typeStr == "PM125") ? typeStr : "none";
}

bool tester::isPM240() const {
    if (type.empty() || type == "none") throw std::runtime_error("Missing type assignment");
    return (type == "PM240") ? true : false;
}

bool tester::isPM125() const {
    if (type.empty() || type == "none") throw std::runtime_error("Missing type assignment");
    return (type == "PM125") ? true : false;
}

tester::status tester::getStatus() const {
    std::string output = runCommand(*this, "-s");
    removeBlankLines(output);

    status Stats;

    auto getReturnStr = [this](std::string inputStr) {
        size_t startPos = inputStr.find(inputStr) + inputStr.size() - 1;
        if (startPos != std::string::npos) { // Tester responded
            size_t p = startPos;
            std::string ReturnStr = "";
            while (isdigit(inputStr[p])) {
                ReturnStr.push_back(inputStr[p]);
                if (p < inputStr.size()) p += 1;
                else break;
            }
            return ReturnStr;
        } else throw std::runtime_error("(" + this->serialNumber + ") Tester failed to respond.");
    };

    Stats.sinkVoltage = getReturnStr("SINK VOLTAGE:");
    Stats.sinkSetCurrent = getReturnStr("SINK SET CURRENT:");
    Stats.sinkMeasCurrent = getReturnStr("SINK MEASURED CURRENT:");
    return Stats;
}

tester::status tester::setProfile(std::string profileNumStr) const {
    runCommand(*this, "-v " + profileNumStr); // Console command to set profile
    Sleep(3000); // Allow time for voltage to settle
    return getStatus();
}

tester::status tester::setVariableVoltageProfile(std::string profileNumStr, int sinkVoltage) const {
    runCommand(*this, "-v " + profileNumStr + "," + std::to_string(sinkVoltage)); // Console command to set profile
    Sleep(3000); // Allow time for voltage to settle
    return getStatus();
}

tester::status tester::setLoad(std::string loadCurrent) const {
    // Send set load command to tester
    if (this->isPM125()) runCommand(*this, "-l " + loadCurrent);
    if (this->isPM240()) runCommand(*this, "-l " + loadCurrent + ",200");
 
    Sleep(500); // Allow time for current to settle
    return getStatus();
}

void tester::testSinkVoltage(std::string profileStr) const {
    /**
     * Main logic for USB protocol test is implemented here.
     * (1) Check if profileStr is empty. If its not empty, only test profiles specified. Assume that profiles were already checked to be valid.
     * (2) Iterate through each profile.
     * (3) Check if profile is a variable voltage profile. If true, set up loop to iterate through voltage range
     */

    auto runCurrentSweep = [this](std::string profile) {
        auto CurrentSweep = [this](std::string maxCurrent) {
            int c = 0;
            int currentStep = 50; // Current step in mA
            while (c < std::stoi(maxCurrent)) {
                status Stats = this->setLoad(std::to_string(c));
                std::cout << Stats.sinkVoltage << ", " << Stats.sinkMeasCurrent << std::endl;
            }
        };

        ProfileInfo info = getProfileInfo(profile);
        if (info.isVariableVoltage) {
            size_t pos = info.voltageRange.find("-");
            if (pos != std::string::npos) {
                std::cout << info.voltageRange.substr(0,pos) << std::endl;
                int vMin = std::stoi(info.voltageRange.substr(0,pos));
                std::cout << "Check 1" << std::endl;
                int vMax = std::stoi(info.voltageRange.substr(pos + 1));
                std::cout << "Check 2" << std::endl;
                int vStep = 1000; // Voltage step in mV for variable voltage profiles

                for (int v = vMin; v < vMax; v += vStep - v % vStep) {
                    status Stats = this->setVariableVoltageProfile(profile, v);
                    std::cout << "Check 3" << std::endl;

                    // Check that profile was set successfully
                    int setVoltage = std::stoi(Stats.sinkVoltage);
                    if (setVoltage > v * 0.95 && setVoltage < v * 1.05) {
                        CurrentSweep(info.maxCurrent);
                    }
                }
            }
        } else {
            status Stats = this->setProfile(profile);

            // Check that profile was set successfully
            int setVoltage = std::stoi(Stats.sinkVoltage);
            int targetVoltage = std::stoi(info.voltageRange);
            if (setVoltage > targetVoltage * 0.95 && setVoltage < targetVoltage * 1.05) {
                CurrentSweep(info.maxCurrent);
            }
        }
    };

    if (!profileStr.empty()) { // One or more profiles were specified
        std::stringstream ss(profileStr);
        std::string field;
        while (getline(ss, field, ',')) {
            runCurrentSweep(field);
        }
    }
}

// ----------------------------------------
// Other functions
// ----------------------------------------

std::string runCommand(const tester& Tester, const std::string& commandArg) {
    std::string commandBase = (Tester.isPM240()) ? "USBPDPROConsole.exe " : (Tester.isPM125()) ? "USBPDConsole.exe " : "Invalid tester type";
    if (commandBase == "Invalid tester type") throw std::runtime_error(commandBase);
    
    // Append serial number to commandBase if not empty, i.e., if Tester object represents a real tester
    if (!Tester.serialNumber.empty()) commandBase.append("-d " + Tester.serialNumber + " ");
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