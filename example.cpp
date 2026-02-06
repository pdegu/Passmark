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

    return 0;
}