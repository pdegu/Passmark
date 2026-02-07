#pragma once
#include <string>
#include <stdexcept>

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

    std::string getProfiles() const;
    
    // Other simple functions that do not require external logic
    void assignType(const std::string& typeStr) {
        type = (typeStr == "PM240" || typeStr == "PM100") ? typeStr : "none";
    }

    bool isPM240() const {
        if (type.empty() || type == "none") throw std::runtime_error("Missing type assignment");
        return (type == "PM240") ? true : false;
    }

    bool isPM100() const {
        if (type.empty() || type == "none") throw std::runtime_error("Missing type assignment");
        return (type == "PM100") ? true : false;
    }
};