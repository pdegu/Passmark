#pragma once

// Project headers
#include "device.hpp"

// Standard headers
#include <vector>

// Check which devices are available and claim
std::vector<device> getDevices();