#pragma once
#include <string>
#include <vector>
#include <Windows.h>
#include <functional>

struct testerList {
    std::vector<std::string> testers;
    std::vector<std::string> type;
};

// Find all connected PM240 and PM100 testers
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
    std::string getProfiles() const;
    
    // Assign tester type
    void assignType(const std::string& typeStr);

    // Return true if tester type is PM240
    bool isPM240() const;

    // Return true if tester type is PM100
    bool isPM100() const;

    // Lock tester and run core 
    void operateHardware(std::string inputStr) const;
};

// Run Passmark executable from cmd prompt and return info provided
std::string runCommand(const tester& dev, const std::string& commandArg);

// Remove blank lines from Passmark console output string
void removeBlankLines(std::string& string_to_filter);