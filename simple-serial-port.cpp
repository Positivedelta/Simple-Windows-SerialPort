//
// (c) Bit Parallel Ltd (Max van Daalen), April 2022
//

#include <iostream>

#include "simple-serial-port.hpp"

SimpleSerialPort::SimpleSerialPort(const std::string& deviceName, const int32_t baudRate, const bool enableReceiver, const ReadListener rxListener):
	deviceName(deviceName), enableReceiver(enableReceiver) {
		// open the device
		//
		handle = CreateFileA(deviceName.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, nullptr);
		if (handle == INVALID_HANDLE_VALUE) throw "Unable to open device: " + deviceName + ", failed with ERRNO =  " + std::to_string(GetLastError());

		// setup the serial port
		//
		DCB dcbSerialParams = DCB();
		dcbSerialParams.DCBlength = sizeof(dcbSerialParams);
		int32_t status = GetCommState(handle, &dcbSerialParams);
		if (status == 0) throw "Unable to read device state for " + deviceName + ", failed with ERRNO =  " + std::to_string(GetLastError());

		// using the predefined magic numbers, only the commonly used speeds are defined below, others exist
		//
		int32_t wBaudRate = 0;
		switch (baudRate)
		{
			// note, there are lower values that can be added in if needed
			//
			case 1200:
				wBaudRate = CBR_1200;
				break;

			case 2400:
				wBaudRate = CBR_2400;
				break;

			case 4800:
				wBaudRate = CBR_4800;
				break;

			case 9600:
				wBaudRate = CBR_9600;
				break;

			case 57600:
				wBaudRate = CBR_57600;
				break;

			case 115200:
				wBaudRate = CBR_115200;
				break;

			//
			// note, for historic reasons the following baud rates are not defined as part of the DCB struct as old UARTS didn't support
			//       rates this fast! however, on modern hardware they work just fine
			//

			case 230400:
				wBaudRate = 230400;
				break;

			case 460800:
				wBaudRate = 460800;
				break;

			case 921600:
				wBaudRate = 921600;
				break;

			default:
				throw "Invalid baud rate for " + deviceName + ", supported values are 1200, 2400, 4800, 9600, 57600, 115200, 230400, 460800 and 921600";
		}

		dcbSerialParams.BaudRate = wBaudRate;
		dcbSerialParams.ByteSize = 8;
		dcbSerialParams.StopBits = ONESTOPBIT;
		dcbSerialParams.Parity = NOPARITY;
		SetCommState(handle, &dcbSerialParams);

		COMMTIMEOUTS timeouts = COMMTIMEOUTS();
		timeouts.ReadIntervalTimeout = MAXDWORD;
		timeouts.ReadTotalTimeoutMultiplier = 0;
		timeouts.ReadTotalTimeoutConstant = 0;
		timeouts.WriteTotalTimeoutConstant = 0;
		timeouts.WriteTotalTimeoutMultiplier = 0;
		SetCommTimeouts(handle, &timeouts);

		// prepare to start receiving RX and TX events
		//
		SetCommMask(handle, EV_RXCHAR | EV_TXEMPTY);

		// if enabled, start up the receiver task
		//
		doReceive = true;
		if (enableReceiver)
		{
			rxTask = std::thread([this, rxListener]() {
				uint8_t rxBuffer[RX_BUFFER_SIZE];
				while (doReceive)
				{
					try
					{
						const int32_t bytesRead = doRead(rxBuffer);
						if (bytesRead > 0) rxListener(rxBuffer, bytesRead);
					}
					catch (const std::string& exception)
					{
						std::cerr << "Exception during read(), exiting the receiver thread (" << exception << ")\n" << std::flush;
						doReceive = false;
					}
				}
			});
		}
}

int32_t SimpleSerialPort::doRead(uint8_t rxBuffer[])
{
	ZeroMemory(&overlappedRead, sizeof(overlappedRead));
	overlappedRead.hEvent = CreateEvent(nullptr, true, false, nullptr);
//	SetCommMask(handle, EV_RXCHAR);

	// wait for a read event
	//
	int32_t waitEventType = 0;
	int32_t status = -1;
	if (!WaitCommEvent(handle, (LPDWORD)&waitEventType, &overlappedRead))
	{
		if (GetLastError() != ERROR_IO_PENDING)
		{
			// setup the next RX event in case this is recoverable
			//
			SetCommMask(handle, EV_RXCHAR);
			throw "Waiting for read event on " + deviceName + ", failed with ERRNO =  " + std::to_string(GetLastError());
		}
	}

	switch (WaitForSingleObject(overlappedRead.hEvent, RX_READ_TIMEOUT_MS))
	{
		// success, the event signal has been received!
		//
		case WAIT_OBJECT_0:
			status = 0;
			break;

		case WAIT_TIMEOUT:
			status = 1;
			break;

		default:
		{
			// setup the next RX event in case this is recoverable
			//
			SetCommMask(handle, EV_RXCHAR);
			throw "Waiting for single read object on " + deviceName + ", failed with ERRNO =  " + std::to_string(GetLastError());
		}
	}

	// now read the RXed data
	//
	DWORD bytesRead = 0;
	if (status == 0)
	{
		auto rf = ReadFile(handle, rxBuffer, static_cast<DWORD>(RX_BUFFER_SIZE), nullptr, &overlappedRead);
		if ((rf == 0) && GetLastError() == ERROR_IO_PENDING)
		{
			while (!GetOverlappedResult(handle, &overlappedRead, &bytesRead, false))
			{
				// note, if the last error was ERROR_IO_PENDING then this could be harnessed to do 'overlapped' work, i.e. call a useful method
				// otherwise loop and wait for the next event, or an error has occured, other return values include ERROR_HANDLE_EOF (file reads)
				//
				if (GetLastError() != ERROR_IO_PENDING)
				{
					// setup the next RX event in case this is recoverable
					//
					SetCommMask(handle, EV_RXCHAR);
					throw "Unable to read overlapped data on " + deviceName + ", failed with ERRNO =  " + std::to_string(GetLastError());
				}
			}
		}

		bytesRead = static_cast<DWORD>(overlappedRead.InternalHigh);
	}
	else if (status == 1)
	{
		// the read has timed out
		//
		bytesRead = 0;
	}

	status = CloseHandle(_Notnull_ overlappedRead.hEvent);
	if (status == 0)
	{
		// setup the next RX event in case this is recoverable
		//
		SetCommMask(handle, EV_RXCHAR);
		throw "Unable to close event handle for " + deviceName + ", failed with ERRNO =  " + std::to_string(GetLastError());
	}

	// setup the next RX event
	//
	SetCommMask(handle, EV_RXCHAR);

	return bytesRead;
}

void SimpleSerialPort::write(const uint8_t bytes[], int32_t length) const
{
	if (length == 0) return;

	OVERLAPPED overlappedWrite;
	ZeroMemory(&overlappedWrite, sizeof(overlappedWrite));
	overlappedWrite.hEvent = CreateEvent(nullptr, true, false, nullptr);

	int32_t bytesWritten = 0;
//	SetCommMask(handle, EV_TXEMPTY);
	if (!WriteFile(handle, bytes, length, (LPDWORD)&bytesWritten, &overlappedWrite))
	{
		if (GetLastError() != ERROR_IO_PENDING)
		{
			// reset the next TX event in case this is recoverable, doubt it...
			//
			SetCommMask(handle, EV_TXEMPTY);
			throw "Write to " + deviceName + ", failed with ERRNO =  " + std::to_string(GetLastError());
		}
		else
		{
			// the write is pending...
			// as bWait is true, GetOverlappedResult() will block until the write is complete or an error occurs
			//
			if (GetOverlappedResult(handle, &overlappedWrite, (LPDWORD)&bytesWritten, true) == 0)
			{
				// reset the next TX event in case this is recoverable, doubt it...
				//
				SetCommMask(handle, EV_TXEMPTY);
				throw "Overlapped write to " + deviceName + ", failed with ERRNO =  " + std::to_string(GetLastError());
			}
		}
	}

	// FIXME! remove the (overlappedWrite.hEvent == 0) check if it causes a problem...
	//        perhaps could use _Notnull_, check...
	//
	if ((overlappedWrite.hEvent == 0) || (CloseHandle(overlappedWrite.hEvent) == 0))
	{
		// set up the next TX event in case this is recoverable
		//
		SetCommMask(handle, EV_TXEMPTY);
		throw "Unable to close the write handle for " + deviceName + ", failed with ERRNO =  " + std::to_string(GetLastError());
	}

	// set up the next TX event
	//
	SetCommMask(handle, EV_TXEMPTY);
}

void SimpleSerialPort::write(const uint8_t singleByte) const
{
	const uint8_t bytes[1] = {singleByte};
	write(bytes, 1);
}

void SimpleSerialPort::print(const std::string& text) const
{
	write(reinterpret_cast<const uint8_t*>(text.data()), int32_t(text.size()));
}

void SimpleSerialPort::printLine() const
{
	write(NEW_LINE, sizeof(NEW_LINE));
}

void SimpleSerialPort::printLine(const std::string& text) const
{
	print(text);
	write(NEW_LINE, sizeof(NEW_LINE));
}

SimpleSerialPort::~SimpleSerialPort()
{
	if (enableReceiver && doReceive)
	{
		doReceive = false;
		rxTask.join();
	}

	CloseHandle(handle);
}