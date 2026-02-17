#define _WIN32_WINNT 0x0600

#include "Passmark.hpp"

#include <vector>
#include <stdexcept>
#include <iostream>
#include <Windows.h>
#include <string>
#include <sstream>
#include <chrono>

// Determine max output from available profiles
std::string getMax(const tester& Tester) {
    std::string indexStr; // return value

    std::string output = Tester.getProfiles(false);
    removeBlankLines(output); 

    std::stringstream ss(output);
    std::string line;
    int vLast = 5000, iLast = 500, vNow, iNow;
    while (getline(ss, line)) {
        size_t pos1 = line.find("V:") + 2;
        vNow = (pos1 != std::string::npos) ? std::stoi(line.substr(pos1, line.find("mV") - pos1)) : 0;

        size_t pos2 = line.find("I:") + 2;
        iNow = (pos2 != std::string::npos) ? std::stoi(line.substr(pos2, line.find("mA") - pos2)) : 0;

        size_t pos3 = line.find("INDEX:") + 6;
        std::string temp = (pos3 != std::string::npos) ? line.substr(pos3, 1) : "";
        indexStr = (vNow > vLast) ?  temp : (iNow > iLast) ? temp : "";
    }

    return indexStr;
}

/**
 * Logic for battery stress test
 * (1) Set profile to profileStr
 * (2) Define time limit and begin test
 * (3) While test is running, check if voltage has dropped
 */
void StressTest(const tester& Tester, const std::string& profileStr, const std::string& duration) {
    
    tester::status Stats;
    auto magic = [&Tester, &profileStr, &Stats]() {
        int setVoltage; // return value
        tester::ProfileInfo info = Tester.getProfileInfo(profileStr);
        
        // Determine profile type
        if (info.isVariableVoltage) {
        // Set to max voltage
        size_t pos = info.voltageRange.find("-");
        if (pos != std::string::npos) {
            setVoltage = std::stoi(info.voltageRange.substr(pos + 1));
            Stats = Tester.setVariableVoltageProfile(profileStr, setVoltage);
        }
        } else {
            setVoltage = std::stoi(info.voltageRange);
            Stats = Tester.setProfile(profileStr);
        }

        return setVoltage;
    };

    int testVoltage = magic();

    // Check that voltage is set
    int voltage = std::stoi(Stats.sinkVoltage);
    if (voltage < testVoltage * 0.95 && voltage > testVoltage * 1.05) {
        throw std::runtime_error("(" + Tester.serialNumber + ") Unable to set voltage.");
    }

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

        // Detect when output has decreased
        Stats = Tester.getStatus();
        if (std::stoi(Stats.sinkVoltage) < testVoltage * 0.8 && std::stoi(Stats.sinkVoltage) > testVoltage * 1.2) {

            // Check that load is nonzero
            if (Stats.sinkMeasCurrent == "0") {
                std::string profileStr = getMax(Tester);

                if (!profileStr.empty()) { // Check that backup profile exists
                    Stats = Tester.setProfile(profileStr);
                    testVoltage = magic(); // Set backup profile

                    if (std::stoi(Stats.sinkVoltage) > testVoltage * 0.95 && std::stoi(Stats.sinkVoltage) < testVoltage * 1.05) {
                        // Stats = Tester.setLoad(info.maxCurrent);
                    }

                } else {
                    std::cerr << "(" << Tester.serialNumber << ") Voltage dropped, unable to set new profile." << std::endl;
                }
            }
        }

        // Print stats to console
        printf("sinkVoltage = %smV, sinkMeasCurrent = %smA\n", Stats.sinkVoltage.c_str(), Stats.sinkMeasCurrent.c_str());
        
        if (timeRemaining.count() <= 0) { // Check if test time has expired
            std::cout << "Time limit reached. Terminating test..." << std::endl;
            break;
        }

        int checkInterval = 30 * 1000;
        Sleep(checkInterval);
    }
}

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
    } catch (std::runtime_error& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return -1;
    }

    return 0;
}