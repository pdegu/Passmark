#include "Passmark.hpp"

#include <string>
#include <sstream>
#include <stdexcept>
#include <iostream>
#include <vector>

std::vector<device> getDevices() {
    // Find available Passmark devices
    deviceList list = findDevices();

    // User selects device(s)
    std::cout << "Enter device(s) to be used. For multiple devices, separate device numbers with commas. Do not use spaces.\n";
    std::string selection;
    getline(std::cin, selection);

    // Initialize device objects
    std::vector<device> testDevice;
    std::stringstream ss(selection);
    std::string field;
    while (getline(ss, field, ',')) { // Check user selection is valid and store information to device object vector
        device placeHolder;
        int deviceIdx = std::stoi(field) - 1;

        // Check if device is in use
        if (placeHolder.tryClaim(list.devices[deviceIdx])) {
            placeHolder.assignType(list.type[deviceIdx]);
            testDevice.push_back(std::move(placeHolder)); 
        } else throw std::runtime_error(list.devices[deviceIdx] + " is in use.");
    }

    return testDevice;
}