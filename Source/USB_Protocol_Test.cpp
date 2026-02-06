#include <iostream>
#include "Passmark.hpp"
#include <vector>
#include <string>

int main () {
    // Discover Passmark devices and select which ones to use
    std::vector<device> testDevice = getDevices();

    return 0;
}