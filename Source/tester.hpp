#pragma once
#include <string>
#include <vector>
#include <Windows.h>
#include <functional>
#include <sstream>
#include <iostream>

struct testerList {
    std::vector<std::string> testers;
    std::vector<std::string> type;
};

// Find all connected PM240 and PM125 testers
testerList findTesters();

class TesterStream;

class tester
{
private:
    HANDLE hMutex; // Stores "lock" on Passmark tester

public:
    std::string serialNumber;
    std::string type;

    // Constructors and destructors
    tester(); // Default constructor
    tester(tester&& other) noexcept; // Move constructor, argument is temporary tester object
    ~tester(); // Deconstructor

    // Disable standard copying to prevent "Double Releasing" the lock
    tester(const tester&) = delete;
    tester& operator=(const tester&) = delete;

    // Declarations for functions defined in tester.cpp
    bool tryClaim(std::string sn);

    int consoleColor = 7; // Default to white

    // Returns a temporary stream object
    TesterStream log() const;

    std::vector<std::string> profileList;

    void getProfileList();

    // Get supported profiles from DUT
    std::string getProfiles(bool toConsole) const;

    // Object to store detected profile info
    struct ProfileInfo {
        bool isVariableVoltage = false; // False by default
        std::string voltageRange;
        std::string maxCurrent;
    };

    // Get profile info
    ProfileInfo getProfileInfo(std::string profile) const;
    
    // Assign tester type
    void assignType(const std::string& typeStr);

    // Return true if tester type is PM240
    bool isPM240() const;

    // Return true if tester type is PM125
    bool isPM125() const;

    struct status {
        std::string sinkVoltage;
        std::string sinkSetCurrent;
        std::string sinkMeasCurrent;
    };
    
    status getStatus() const;

    // Set DUT profile
    status setProfile(std::string profileNumStr) const;

    // Set DUT variable voltage profile
    status setVariableVoltageProfile(std::string profileNumStr, int sinkVoltage) const;

    // Set load current
    status setLoad(std::string maxCurrent, const std::string& loadSpeed = "200") const;

    // Set load to zero
    status unload() const;

    // Lock tester and run core 
    void testSinkVoltage(std::string profileStr) const;
};

class TesterStream {
public:
    TesterStream(const tester& t) : tRef(t) {}

    // ADD THIS: Move Constructor
    // It transfers the buffer from the old object to the new one
    TesterStream(TesterStream&& other) noexcept 
        : tRef(other.tRef), buffer(std::move(other.buffer)) {}

    // Disable copying explicitly to prevent accidents
    TesterStream(const TesterStream&) = delete;
    TesterStream& operator=(const TesterStream&) = delete;

    ~TesterStream() {
        // Only print if the buffer isn't empty (avoids double printing after a move)
        std::string out = buffer.str();
        if (out.empty()) return;

        // Use a static Windows Critical Section
        static CRITICAL_SECTION cs;
        static bool initialized = false;
        if (!initialized) {
            InitializeCriticalSection(&cs);
            initialized = true;
        }

        EnterCriticalSection(&cs); // Lock

        HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
        SetConsoleTextAttribute(hConsole, tRef.consoleColor);
        std::cout << "(" << tRef.serialNumber << ") " << out << std::endl;
        SetConsoleTextAttribute(hConsole, 7);

        LeaveCriticalSection(&cs); // Unlock
    }

    template <typename T>
    TesterStream& operator<<(const T& msg) {
        buffer << msg;
        return *this;
    }

private:
    const tester& tRef;
    std::stringstream buffer;
};

// Run Passmark executable from cmd prompt and return info provided
std::string runCommand(const tester& dev, const std::string& commandArg);

// Remove blank lines from Passmark console output string
void removeBlankLines(std::string& string_to_filter);

extern std::vector<std::string> VariableVoltageTypes;