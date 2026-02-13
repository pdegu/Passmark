#include "Passmark.hpp"

#include <iostream>
#include <vector>
#include <string>
#include <Windows.h>
#include <sstream>
#include <stdexcept>

int main () {
    // Initialize tester vector
    std::vector<tester> validTesters;

    // ------------------
    // Core test sequence
    // ------------------
    try {
        validTesters = getTesters(); // Discover Passmark testers and select which ones to use

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

            // Ask user which profile(s) to test
            std::cout << "Select profile(s) to test or press enter to test all profiles:\t";
            std::string profileStr = "";
            getline(std::cin, profileStr);

            // Check if profileStr is valid
            if (profileStr.empty()) {
                std::cout << "Testing all profiles..." << std::endl;
            } else {
                std::string field;
                std::stringstream ss(profileStr);
                while (getline(ss,field,',')) {
                    if (!is_numeric(field)) throw std::runtime_error("Profile selection must be an integer!");
                    int profileNum = std::stoi(field);
                    if (profileNum < 1 || profileNum > numProfiles) throw std::runtime_error("Selected profile is out of range!");
                }
            }

            // Create thread in suspended state
            HANDLE hThread = Bridge::startSuspended([&Tester, &profileStr]() {
                Tester.testSinkVoltage(profileStr); 
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
            6e7 // 1hr time limit
        );

        if (waitResult == WAIT_TIMEOUT || waitResult == WAIT_FAILED) {
            // One of the testers hung or failed
        }

        // Close handles
        for (HANDLE h : threadHandles) {
            CloseHandle(h);
        }
        threadHandles.clear();
    } catch (const std::runtime_error&e) {
        std::cout << "Error: " << e.what() << std::endl;
        return -1;
    }

    return 0;
}