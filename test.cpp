#include <iostream>
#include <algorithm>

int main() {
    uint8_t backlight_level_ = 248;
    uint8_t target = 255;
    backlight_level_ = std::min(static_cast<uint8_t>(backlight_level_ + 8), target);
    std::cout << (int)backlight_level_ << std::endl;
    return 0;
}
