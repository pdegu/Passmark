#include <iostream>
#include "Passmark.hpp"
#include <vector>
#include <string>

int main () {
    // Find available Passmark devices
    deviceList list;
    try {
        list = findDevices();
    } catch (const std::runtime_error& e) {
        std::cout << "Error: " << e.what() << std::endl;
        return -1;
    } for (std::string hm : list.devices) std::cout << hm << std::endl;

    // User selects device(s)
    std::cout << "Enter device(s) to be used. For multiple devices, separate device numbers with commas. Do not use spaces.\n";
    std::string selection;
    getline(std::cin, selection);

    // Initialize device objects
    std::vector<device> testDevice;
    std::stringstream ss(selection);
    std::string field;
    while (getline(ss, field, ',')) {
        int deviceIdx = std::stoi(field); 
        std::string deviceType = (deviceIdx > 0 && deviceIdx <= list.countPM240) ? "PM240" : (deviceIdx <= list.count) ? "PM100" : "invalid";
        if (deviceType == "invalid") {
            std::cout << deviceIdx << " is not a valid selection" << std::endl;
            return -1;
        }
        device placeHolder;
        placeHolder.assignType(deviceType);
        placeHolder.serialNumber = list.devices[deviceIdx];
        testDevice.push_back(placeHolder);
    }

    // for (device eh : testDevice) std::cout << eh.serialNumber << std::endl;

    return 0;
}