#pragma once
#include <string>
#include <vector>

struct deviceList {
    std::vector<std::string> devices;
    std::vector<std::string> type;
};

// Find all connected PM240 and PM100 devices
deviceList findDevices();

typedef void* HANDLE;

class device
{
private:
    std::string type;
    HANDLE hMutex; // Stores "lock" on Passmark device

public:
    std::string serialNumber;

    // Constructors and destructors
    device(); // Default constructor
    device(device&& other) noexcept; // Move constructor, argument is temporary device object
    ~device(); // Deconstructor

    // Disable standard copying to prevent "Double Releasing" the lock
    device(const device&) = delete;
    device& operator=(const device&) = delete;

    // Declarations for functions defined in device.cpp
    bool tryClaim(std::string sn);

    // Get supported profiles from DUT
    std::string getProfiles() const;
    
    // Assign device type
    void assignType(const std::string& typeStr);

    // Return true if device type is PM240
    bool isPM240() const;

    // Return true if device type is PM100
    bool isPM100() const;
};

// Run Passmark executable from cmd prompt and return info provided
std::string runCommand(const device& dev, const std::string& commandArg);

// Remove blank lines from Passmark console output string
void removeBlankLines(std::string& string_to_filter);