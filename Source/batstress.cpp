#include "Passmark.hpp"

#include <vector>
#include <stdexcept>
#include <iostream>
#include <Windows.h>
#include <string>
#include <sstream>
#include <chrono>

int main() {
    std::vector<tester> validTesters; // Initialize tester object(s)

    try {
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
            std::string output = Tester.getProfiles(true); // Discover supported profiles for DUT
            size_t pos = output.find("NUM PROFILES:");
            int numProfiles;
            if (pos != std::string::npos) {
                std::string numProfilesStr = output.substr(pos + 13); // Extract number of profiles from output
                numProfiles = std::stoi(numProfilesStr);
            }
            if (numProfiles == 0) throw std::runtime_error("No DUT found."); // Check if no profiles are found

            // Ask user which profile to test
            std::cout << "Select profile to test:\t";
            std::string profileStr = "";
            getline(std::cin, profileStr);

            // Check if profileStr is valid
            if (profileStr.empty()) throw std::runtime_error("Profile selection cannot be empty!");
            if (!is_numeric(profileStr)) throw std::runtime_error("Profile selection must be an integer!");
            int profileNum = std::stoi(profileStr);
            if (profileNum < 1 || profileNum > numProfiles) throw std::runtime_error("Selected profile is out of range!");

            /**
             * Main logic for battery stress test
             * (1) Set profile to profileStr
             * (2) Define time limit
             */

            auto batstress = [&Tester, &duration](std::string profileStr) {
                tester::ProfileInfo info = Tester.getProfileInfo(profileStr);
                tester::status Stats;
                if (info.isVariableVoltage) {
                    // Set to max voltage
                    size_t pos = info.voltageRange.find("-");
                    if (pos != std::string::npos) {
                        int testVoltage = std::stoi(info.voltageRange.substr(pos + 1));
                        Stats = Tester.setVariableVoltageProfile(profileStr, testVoltage);
                    }
                } else {
                    Stats = Tester.setProfile(profileStr);
                }
            };

            // Create thread in suspended state
            HANDLE hThread = Bridge::startSuspended([&Tester, &profileStr, &duration, &batstress]() {
                batstress(profileStr); // <<< CHANGE TO STRESS TEST FUNCTION ONCE WRITTEN
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
    } catch (std::runtime_error& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return -1;
    }

    return 0;
}