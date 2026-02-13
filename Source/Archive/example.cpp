#include <iostream>
#include "Passmark.hpp"
#include <vector>

int main () {
    // Find available Passmark devices
    deviceList list;
    try {
        list = findDevices();
    } catch (const std::runtime_error& e) {
        std::cout << "Error: " << e.what() << std::endl;
        return -1;
    }

    // Declare PM240 objects
    device tester[2];

    return 0;
}