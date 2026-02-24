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
#include <iomanip>
#include <cstdlib>

// Determine max output from available profiles
std::string getMax(tester& Tester) {
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
void StressTest(tester& Tester, const std::string& profileStr, const std::string& duration) { //std::cout << "Pit stop!" << std::endl;

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

        return std::vector<int>{setVoltage, std::stoi(Stats.sinkVoltage), std::stoi(info.maxCurrent)};
    };

    std::vector<int> initialState = magic();

    // Check that voltage is set. Retry up to 3 times
    int attempts = 1;
    while (true) {
        int Vt = initialState[0] /* Target voltage */, Vm = initialState[1]; // Measured voltage
        if (Vm > Vt * 0.95 && Vm < Vt * 1.05) break;
        else if (attempts > 3) throw std::runtime_error("(" + Tester.serialNumber + ") Unable to set voltage.");
        initialState = magic();
        ++attempts;
    }

    // Set load
    std::string iLoad = std::to_string(initialState[2]);
    Tester.setLoad(iLoad);

    // Test loop
    using namespace std::chrono;

    auto startTime = steady_clock::now();
    auto limitMinutes = minutes(std::stoi(duration));
    Tester.log() << "Starting " << limitMinutes.count() << "min test...";
    
    int errCount = 0; bool errWarning = false;

    while (true) {
        // Check for abort at the start of every iteration
        if (g_abortRequested.load(std::memory_order_relaxed)) {
            Tester.setLoad("0"); // Safety: Unload before exiting
            throw CtrlCAbort{};
        }

        // Check remaining time
        auto timeNow = steady_clock::now();
        auto timerStart = timeNow;
        auto timeRemaining = duration_cast<seconds>(limitMinutes - (timeNow - startTime));

        auto Hours = duration_cast<hours>(timeRemaining);
        auto Minutes = duration_cast<minutes>(timeRemaining % hours(1));
        auto Seconds = duration_cast<seconds>(timeRemaining % minutes(1));

        if (Seconds.count() < 0) Seconds = seconds(0);

        // Format output as h:mm:ss
        Tester.log() << "Time remaining: "
                     << Hours.count() << ":"
                     << std::setfill('0') << std::setw(2) << Minutes.count() << ":"
                     << std::setfill('0') << std::setw(2) << Seconds.count();

        // Print stats to console
        tester::status Stats = Tester.getStatus();
        Tester.log() << "Sink voltage = " << Stats.sinkVoltage << "mV, Sink measured current = " << Stats.sinkMeasCurrent << "mA";
        
        if (timeRemaining.count() <= 0) { // Check if test time has expired
            Tester.log() << "Time limit reached. Terminating test...";
            Tester.unload();
            break;
        }

        // Check error status
        if (errCount > 0 && !errWarning) errCount -= 1;
        errWarning = false;

        // Helper lambda to handle early termination of test
        auto abortTest = [&Tester]() {
            Tester.unload();
            throw std::runtime_error("DUT unresponsive.");
        };

        // Detect when output has decreased
        int Im = std::stoi(Stats.sinkMeasCurrent);
        
        if (Im == 0) { // Detect if load dropped

            // Check if DUT has been disconnected and attempt to reconnect
            for (int attempts = 0; attempts < 3; ++attempts) {
                if (!Tester.sink.isConnected()) {
                    Tester.log() << "Attempting to reconnect...";
                    Tester.sink.reconnect();
                }
                else break;
            }
            
            // If DUT is still disconnected, terminate test
            if (!Tester.sink.isConnected()) {
                Tester.logErr() << "Could not connect to DUT after 3 attempts. Terminating test...";
                abortTest();
            }

            // Check for change in advertised profiles
            Tester.getProfileList();
            std::string newProfileStr = getMax(Tester);
            Tester.log() << "Drop in output detected. Setting new profile...";

            Stats = Tester.setProfile(newProfileStr);
            std::vector<int> currentState = magic(); // Set new profile
            int Vt = currentState[0], Vm = currentState[1];
            Tester.log() << "New profile: " << Tester.profileList[std::stoi(newProfileStr) - 1];

            // Check that profile is set
            if (Vm > Vt * 0.95 && Vm < Vt * 1.05) {
                auto resetLoad = [&](std::string iLoad) {
                    Stats = Tester.setLoad(iLoad, "200", 1000); // Set load
                    return std::stoi(Stats.sinkMeasCurrent);
                };
                
                iLoad = std::to_string(currentState[2]);
                Im = resetLoad(iLoad);

                int timerDuration = 5; // seconds
                for (int t = 0; t < timerDuration; t += 1) {
                    if (Im > 0) break;
                    Im = resetLoad(iLoad);
                } 

                // if (iErr > errLimit) {
                if (Im == 0) {
                    Tester.logErr() << "Unable to set load within " << timerDuration << "sec.";
                    errCount += 1;
                    errWarning = true;
                }

            } else {
                Tester.logErr() << "Unable to set new profile.";
                errCount += 1;
                errWarning = true;
            }
        }

        if (errCount >= 3) {
            Tester.logErr() << "DUT failed to respond after " << errCount << " attempts. Terminating test...";
            abortTest();
        }

        int sleepTime = 30 - duration_cast<seconds>(steady_clock::now() - timerStart).count(); // 30 seconds - time spent in loop iteration
        for (int i = 0; i < sleepTime; ++i) {
            if (g_abortRequested.load(std::memory_order_relaxed)) {
                Tester.unload();
                throw CtrlCAbort{};
            }
            Sleep(1000); 
        }
    }
}

int main() {
    std::vector<tester> validTesters; // Initialize tester object(s)

    if (!SetConsoleCtrlHandler(CtrlHandler, TRUE)) { // Register control handler to handle Ctrl+C
        std::cerr << "ERROR: Could not set control handler." << std::endl;
        return -1;
    }

    try {
        validTesters = getTesters();

        // User specifies time limit for test
        std::cout << "Enter test duration in minutes. Default is 120m.\n\nTest duration:\t";
        std::string duration = "";
        getline(std::cin, duration);
        if (duration.empty() || !is_numeric(duration)) duration = "120";

        // Create a thread for each tester to run tests simultaneously
        std::vector<HANDLE> threadHandles;
        int colorIdx = 0;
        for (auto& Tester : validTesters) {
            Tester.consoleColor = colors[colorIdx % 4]; // Assign a unique color
            ++colorIdx;

            std::cout << "\nTester: " << Tester.serialNumber << "\n--------------------------" << std::endl;

            /**
             * Insert code for getting part number here
             */

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