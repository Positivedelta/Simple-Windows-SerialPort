//
// (c) Bit Parallel Ltd (Max van Daalen), April 2022
//

#ifndef H_SIMPLE_SERIAL_PORT
#define H_SIMPLE_SERIAL_PORT

#include <cstdint>
#include <thread>
#include <string>

#include <windows.h>

#include "read-listener.hpp"

class SimpleSerialPort
{
public:
    static inline const uint8_t NEW_LINE[] = {0x0d, 0x0a};

private:
    const static inline int32_t RX_BUFFER_SIZE = 4096;
    const static inline int32_t RX_READ_TIMEOUT_MS = 100;
    const static inline ReadListener DEFAULT_RX_LISTENER = [](const uint8_t rxedBytes[], const int32_t length) {};

    const bool enableReceiver;
    const std::string& deviceName;
    HANDLE handle;
    OVERLAPPED overlappedRead;
    bool doReceive;
    std::thread rxTask;

public:
    SimpleSerialPort(const std::string& deviceName, const int32_t baudRate, const bool enableReceiver = false, const ReadListener rxListener = DEFAULT_RX_LISTENER);
    void write(const uint8_t bytes[], int32_t length) const;
    void write(const uint8_t singleByte) const;
    void print(const std::string& text) const;
    void printLine() const;
    void printLine(const std::string& text) const;
    ~SimpleSerialPort();

private:
    int32_t doRead(uint8_t rxBuffer[]);
};

#endif
