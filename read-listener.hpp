//
// (c) Bit Parallel Ltd (Max van Daalen), April 2022
//

#ifndef H_READ_LISTENER
#define H_READ_LISTENER

#include <cstdint>
#include <functional>

using ReadListener = std::function<void(const uint8_t bytes[], const int32_t length)>;

#endif