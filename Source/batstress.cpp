#define _WIN32_WINNT 0x0600

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
                int testVoltage;
                if (info.isVariableVoltage) {
                    // Set to max voltage
                    size_t pos = info.voltageRange.find("-");
                    if (pos != std::string::npos) {
                        testVoltage = std::stoi(info.voltageRange.substr(pos + 1));
                        Stats = Tester.setVariableVoltageProfile(profileStr, testVoltage);
                    }
                } else {
                    testVoltage = std::stoi(info.voltageRange);
                    Stats = Tester.setProfile(profileStr);
                }

                // Check that voltage is set
                int voltage = std::stoi(Stats.sinkVoltage);
                if (voltage < testVoltage * 0.95 && voltage > testVoltage * 1.05) {
                    throw std::runtime_error("(" + Tester.serialNumber + ") Unable to set voltage.");
                }

                auto startTime = std::chrono::steady_clock::now();
                auto limitMinutes = std::chrono::minutes(std::stoi(duration));
                while (true) {
                    // Print stats to console
                    Stats = Tester.getStatus();
                    printf("sinkVoltage = %smV, sinkMeasCurrent = %smA\n", Stats.sinkVoltage.c_str(), Stats.sinkMeasCurrent.c_str());
                    
                    // Check remaining time
                    auto timeNow = std::chrono::steady_clock::now();
                    auto timeElapsed = std::chrono::duration_cast<std::chrono::seconds>(timeNow - startTime);
                    if (timeElapsed > limitMinutes) {
                        std::cout << "Time limit reached. Terminating test..." << std::endl; 
                        break;
                    } else {
                        auto timeRemaining = std::chrono::duration_cast<std::chrono::seconds>(limitMinutes) - timeElapsed;
                        auto hours = std::chrono::duration_cast<std::chrono::hours>(timeRemaining);
                        auto minutes = std::chrono::duration_cast<std::chrono::minutes>(timeRemaining % std::chrono::hours(1));
                        auto seconds = std::chrono::duration_cast<std::chrono::seconds>(timeRemaining % std::chrono::minutes(1));

                        // Format output as h:mm:ss
                        // %u for unsigned int, %02u to ensure 2 digits with a leading zero
                        printf("Time remaining: %i:%02i:%02i\n", hours.count(), minutes.count(), seconds.count());
                    }

                    int checkInterval = 30 * 1000;
                    Sleep(checkInterval);
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