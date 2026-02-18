#pragma once
#include <string>
#include <vector>
#include <Windows.h>
#include <functional>

struct testerList {
    std::vector<std::string> testers;
    std::vector<std::string> type;
};

// Find all connected PM240 and PM125 testers
testerList findTesters();

class tester
{
private:
    std::string type;
    HANDLE hMutex; // Stores "lock" on Passmark tester

public:
    std::string serialNumber;

    // Constructors and destructors
    tester(); // Default constructor
    tester(tester&& other) noexcept; // Move constructor, argument is temporary tester object
    ~tester(); // Deconstructor

    // Disable standard copying to prevent "Double Releasing" the lock
    tester(const tester&) = delete;
    tester& operator=(const tester&) = delete;

    // Declarations for functions defined in tester.cpp
    bool tryClaim(std::string sn);

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

    // Lock tester and run core 
    void testSinkVoltage(std::string profileStr) const;
};

// Run Passmark executable from cmd prompt and return info provided
std::string runCommand(const tester& dev, const std::string& commandArg);

// Remove blank lines from Passmark console output string
void removeBlankLines(std::string& string_to_filter);

extern std::vector<std::string> VariableVoltageTypes;