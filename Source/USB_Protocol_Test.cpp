#include "Passmark.hpp"

#include <iostream>
#include <vector>
#include <string>

int main () {
    // Initialize device vector
    std::vector<device> testDevice;

    // Discover Passmark devices and select which ones to use
    try {
        testDevice = getDevices();
    } catch (const std::runtime_error&e) {
        std::cout << "Error: " << e.what() << std::endl;
    }

    try {
        for (const device& dev : testDevice) dev.getProfiles();
    } catch (const std::runtime_error&e) {
        std::cout << "Error: " << e.what() << std::endl;
    }

    return 0;
}