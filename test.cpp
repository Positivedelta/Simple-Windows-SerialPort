//
// (c) Bit Parallel Ltd (Max van Daalen), April 2022
//

#include <chrono>
#include <cstdint>
#include <iostream>
#include <string>
#include <thread>

#include "simple-serial-port.hpp"

inline static const uint8_t CR = 0x0d;
inline static const uint8_t LF = 0x0a;

int32_t main()
{
    try
    {
        // create and open the serial port, enable the receiver thread add a listener for any RXed data
        // note, the destructor will close the selected device
        //
        SimpleSerialPort serialPort = SimpleSerialPort("com5", 57600, true, [&](const uint8_t bytes[], const int32_t length) {
            serialPort.write(bytes, length);
            const bool issueLF = bytes[length - 1] == CR;

            // echo to the console for good measure...
            //
            const auto asText = std::string(reinterpret_cast<const char*>(bytes), length);
            std::cout << asText << std::flush;

            // diy the LF when required
            //
            if (issueLF)
            {
                serialPort.write(LF);
                std::cout << "\n" << std::flush;
            }
        });

        // say hello and then sleep for 30 seconds to allow some data to be RXed...
        //
        serialPort.printLine("Hello World!");
        serialPort.print("Please type some text: ");

        std::this_thread::sleep_for(std::chrono::seconds(30));
        serialPort.printLine();
    }
    catch (const std::string& message)
    {
        std::cout << "Exception, " + message;
    }

    return 0;
}