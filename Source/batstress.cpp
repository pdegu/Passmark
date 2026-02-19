#define _WIN32_WINNT 0x0600

#include "Passmark.hpp"

#include <vector>
#include <stdexcept>
#include <iostream>
#include <Windows.h>
#include <string>
#include <sstream>
#include <chrono>
#include <atomic>
#include <algorithm>

// Determine max output from available profiles
std::string getMax(const tester& Tester) {
    std::string indexStr = ""; // return value

    // Initialize loop variables
    int voltage = 5000, current = 500, pNow;
    int pLast = voltage * current;

    for (std::string line : Tester.profileList) { // Iterate through output line by line
        std::string searchStr = "INDEX:";
        size_t pos1 = line.find(searchStr);

        if (pos1 != std::string::npos) { // Get index of current profile
            std::string temp = line.substr(pos1 + searchStr.size(), 1);
            searchStr = "V:";
            size_t pos2 = line.find(searchStr, pos1);
            
            // Check if profile is variable voltage type
            bool isVariableVoltage = false;
            std::for_each(VariableVoltageTypes.begin(), VariableVoltageTypes.end(), [&](std::string s) {
                size_t Pos1 = line.find("TYPE:", pos1);
                size_t Pos2 = line.find(",", Pos1);
                if (Pos1 != std::string::npos && Pos2 != std::string::npos) {
                    Pos1 += 5;
                    isVariableVoltage = (line.substr(Pos1, Pos2 - Pos1) == s) ? true : isVariableVoltage;
                }
            });

            // Get max supported voltage of profile
            if (isVariableVoltage) {
                searchStr = "-";
                pos2 = (pos2 != std::string::npos) ? line.find(searchStr, pos2) : std::string::npos;
            }
            voltage = (pos2 != std::string::npos) ? std::stoi(getNumStr(line, pos2 + searchStr.size())) : 0;

            // Get max supported current of profile
            searchStr = "I:";
            size_t pos3 = line.find(searchStr, pos2);
            current = (pos3 != std::string::npos) ? std::stoi(getNumStr(line, pos3 + searchStr.size())) : 0;

            // Determine max power supported by profile and if greater than previous profile, set return value to current profile index
            pNow = voltage * current;
            indexStr = (pNow > pLast) ?  temp : indexStr;
            pLast = (indexStr == temp) ? pNow : pLast;
        }
    }

    return indexStr;
}

// Logic for power bank stress test
void StressTest(const tester& Tester, const std::string& profileStr, const std::string& duration) { std::cout << "Pit stop!" << std::endl;

    auto magic = [&Tester, &profileStr]() {
        int setVoltage; // return value
        tester::ProfileInfo info = Tester.getProfileInfo(profileStr);
        tester::status Stats;
        
        // Determine profile type and set to max supported voltage
        if (info.isVariableVoltage) {
            size_t pos = info.voltageRange.find("-");

            if (pos != std::string::npos) {
                setVoltage = std::stoi(info.voltageRange.substr(pos + 1));
                Stats = Tester.setVariableVoltageProfile(profileStr, setVoltage);
            }

        } else {
            setVoltage = std::stoi(info.voltageRange);
            Stats = Tester.setProfile(profileStr);
        }

        return std::vector<int>{setVoltage,std::stoi(Stats.sinkVoltage),std::stoi(info.maxCurrent)};
    };

    std::vector<int> initialState = magic();

    // Check that voltage is set
    int Vt = initialState[0] /* Target voltage */, Vm = initialState[1]; // Measured voltage
    if (Vm < Vt * 0.95 && Vm > Vt * 1.05) {
        throw std::runtime_error("(" + Tester.serialNumber + ") Unable to set voltage.");
    }

    // Set load
    std::string iLoad = std::to_string(initialState[2]);
    Tester.setLoad(iLoad);

    // Test loop
    auto startTime = std::chrono::steady_clock::now();
    auto limitMinutes = std::chrono::minutes(std::stoi(duration));
    std::cout << "Starting " << limitMinutes.count() << "min test..." << std::endl;
    
    while (true) {
        // Check remaining time
        auto timeNow = std::chrono::steady_clock::now();
        auto timeRemaining = std::chrono::duration_cast<std::chrono::seconds>(limitMinutes - (timeNow - startTime));

        auto hours = std::chrono::duration_cast<std::chrono::hours>(timeRemaining);
        auto minutes = std::chrono::duration_cast<std::chrono::minutes>(timeRemaining % std::chrono::hours(1));
        auto seconds = std::chrono::duration_cast<std::chrono::seconds>(timeRemaining % std::chrono::minutes(1));

        // Format output as h:mm:ss
        printf("Time remaining: %i:%02i:%02i\n", 
            static_cast<int>(hours.count()), 
            static_cast<int>(minutes.count()), 
            static_cast<int>(seconds.count()));

        // Print stats to console
        tester::status Stats = Tester.getStatus();
        printf("sinkVoltage = %smV, sinkMeasCurrent = %smA\n", Stats.sinkVoltage.c_str(), Stats.sinkMeasCurrent.c_str());
        
        if (timeRemaining.count() <= 0) { // Check if test time has expired
            std::cout << "Time limit reached. Terminating test..." << std::endl;
            Tester.setProfile("1");
            Tester.setLoad("0");
            break;
        }

        // Detect when output has decreased
        int Vm = std::stoi(Stats.sinkVoltage), Im = std::stoi(Stats.sinkMeasCurrent);
        if (Vm < Vt * 0.8 && Vm > Vt * 1.2 || Im == 0) {
            std::string newProfileStr = getMax(Tester);
            std::cout << "Drop in output detected. Setting new profile..." << std::endl;

            // Check that backup profile exists
            if (!newProfileStr.empty()) {
                Stats = Tester.setProfile(newProfileStr);
                std::vector<int> currentState = magic(); // Set backup profile
                Vt = currentState[0], Vm = currentState[1];
                std::cout << "New profile: " << newProfileStr << std::endl;

                // Check that profile is set
                if (Vm > Vt * 0.95 && Vm < Vt * 1.05) {
                    iLoad = std::to_string(currentState[2]);
                    Stats = Tester.setLoad(iLoad); // Set load
                }

            } else {
                std::cerr << "(" << Tester.serialNumber << ") Unable to set new profile." << std::endl;
            }
        }

        int checkInterval = 30 * 1000;
        Sleep(checkInterval);
    }
}

int main() {
    std::vector<tester> validTesters; // Initialize tester object(s)

    if (!SetConsoleCtrlHandler(CtrlHandler, TRUE)) { // Register control handler to handle Ctrl+C
        std::cerr << "ERROR: Could not set control handler." << std::endl;
        return -1;
    }

    try {
        // if (g_abortRequested.load(std::memory_order_relaxed)) {
        //     throw CtrlCAbort{};
        // }

        validTesters = getTesters();

        // User specifies time limit for test
        std::cout << "Enter test duration in minutes. Default is 120m.\n";
        std::string duration = "";
        getline(std::cin, duration);
        if (duration.empty() || !is_numeric(duration)) duration = "120";

        // Create a thread for each tester to run tests simultaneously
        std::vector<HANDLE> threadHandles;
        for (auto& Tester : validTesters) {
            std::cout << "\nTester: " << Tester.serialNumber << "\n--------------------------" << std::endl; 
            Tester.getProfileList();
            int numProfiles = Tester.profileList.size();
            if (numProfiles == 0) throw std::runtime_error("No DUT found."); // Check if no profiles are found

            std::cout << "NUM PROFILES:" << numProfiles << std::endl;
            for (std::string s : Tester.profileList) std::cout << s << std::endl;

            // Ask user which profile to test
            std::cout << "\nSelect profile to test or press enter for auto select:\t";
            std::string profileStr = "";
            getline(std::cin, profileStr);

            // Check if profileStr is valid
            if (profileStr.empty()) {
                profileStr = getMax(Tester);
                std::cout << "Profile " << profileStr << " selected." << std::endl;
            }
            else if (!is_numeric(profileStr)) throw std::runtime_error("Profile selection must be an integer!");
            int profileNum = std::stoi(profileStr);
            if (profileNum < 1 || profileNum > numProfiles) throw std::runtime_error("Selected profile is out of range!");

            // Create thread in suspended state
            HANDLE hThread = Bridge::startSuspended([&Tester, &profileStr, &duration]() {
                StressTest(Tester, profileStr, duration);
            });

            // Check that handle isn't NULL
            if (hThread != NULL) threadHandles.push_back(hThread);
            else std::cerr << "Failed to create thread for tester. Error: " << GetLastError() << std::endl;
        }

        // Start threads once preparations are made
        for (HANDLE h : threadHandles) {
            if (h != NULL && h != INVALID_HANDLE_VALUE) ResumeThread(h);
        }

        // Halt main program until threads are finished
        DWORD waitResult = WaitForMultipleObjects(
            (DWORD)threadHandles.size(),
            threadHandles.data(),
            TRUE,
            9e6 // 2.5hr time limit
        );

        if (waitResult == WAIT_TIMEOUT || waitResult == WAIT_FAILED) throw std::runtime_error("One or more threads failed.");

        // Close handles
        for (HANDLE h : threadHandles) CloseHandle(h);
        threadHandles.clear();
    } catch (const std::runtime_error& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return -1;
    } catch (const CtrlCAbort& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return -1;
    }

    return 0;
}