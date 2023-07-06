#ifndef DS2_h
#define DS2_h

#if ARDUINO >= 100
  #include "Arduino.h"
#else
  #include "WProgram.h"
  #include "pins_arduino.h"
  #include "WConstants.h"
#endif

/**
*	DS2 Library
*	Made to simplyfy the communication between arduino code and ECUs using DS2 k-line protocol ISO 9141.
*	It was made and tested for L9637D but any interface should work the same way. 
*	Echo - if you send any command on DS2 it responds with whatever was send due to way K-line is like constructed.
*	Imagine K-line being a single guitar string and our TX is making it vibrate and our RX is constantly picking up this vibrations. This is echo
	
*	1st byte in command/response is device which should receive command or which send the command
	0x12 - ECU default
*	2nd byte is command/response length
*	3rd byte for response it's acknowledge byte A0 meaning positive response, B0 being negative and any other (usually FF) being error; for command is specific to command itself.
	
*	4th - Xth bytes are for payload
*	Last byte is always XOR checksum. You can use this calculator to get it: https://www.scadacore.com/tools/programming-calculators/online-checksum-calculator/
*	Some commands for DS2 for BMW:
	- 	ECU id - {0x12, 0x04, 0x00, 0x16}
	-	General values - {0x12, 0x05, 0x0B, 0x03, 0x1F}
	
**/
// Default timeout for message
#ifndef ISO_TIMEOUT
#define ISO_TIMEOUT 250
#endif

// Default max data length
#ifndef MAX_DATA_LENGTH
#define MAX_DATA_LENGTH 255
#endif


// Default receive flags
#ifndef RECEIVE_WAITING
#define RECEIVE_WAITING 0
#endif

#ifndef RECEIVE_TIMEOUT
#define RECEIVE_TIMEOUT 1
#endif

#ifndef RECEIVE_OK
#define RECEIVE_OK 2
#endif


#ifndef RECEIVE_BAD
#define RECEIVE_BAD 3
#endif


class DS2 {
	public:
		// Constructor, you can pass any Serial, SoftwareSerial or BluetoothSerial - anything that extends 'stream'
		DS2(Stream &stream);
		
		// You san use library in blocking mode (default is false) and it waits ISO 112ms time for response.
		void setBlocking(boolean mode);
		boolean getBlocking();
		void setTimeout(uint8_t timeoutMs);
		void setAckByte(uint8_t ack, uint8_t offset, boolean check); // sets ack byte and whether we should perform datacheck
		void setMaxDataLength(uint8_t dataLength); // Useful for saving space if you know you don't want to have longer messages
		
		// Sets or gets device first code, after setting command device is automatically set to first byte value
		uint8_t getDevice();
		uint8_t setDevice(uint8_t dev);
		
		// Data handling - use those commands to get and check data (data is automatically check when read but you can use command to check it
		uint8_t writeData(uint8_t data[], uint8_t length = 0); // sets device to first byte value; sets echo to second byte value
		boolean readCommand(uint8_t data[]); // sets echo to 0 so you can use readData and it will read data without echo after calling this command
		boolean readData(uint8_t data[]); // reads command and checks data
		boolean checkData(uint8_t data[]); // just use setEcho to 0 if want to check only response or command otherwise it will check whole data (echo + response) for checksum
		uint8_t available();
		void flush();
		
		boolean obtainValues(uint8_t command[], uint8_t data[], uint8_t respLen = 0); // automated forced blocking one-liner to get data quickly; respLen == 0 - automatic responseLength recognition (might be slower)
		
		// Non blocking if set; those two below are great to use at the beggining and end of loops to get data/gui/touch processing while we wait for command
		uint8_t sendCommand(uint8_t command[], uint8_t respLen = 0);	// sends command - if already send and no response it won't send it again (safe to use in loop); returns number of bytes send
																		// if default 0 - same but without respLength (auto learning - might be slower if commands change often)
		uint8_t receiveData(uint8_t data[]); // use at end of loop if we want to do other stuff while we wait for command. Can be blocking or non blocking; returns receive flags
		void newCommand(); // use to force new command - clear RX buffer and allow to send new command
		boolean compareCommands(uint8_t compA[], uint8_t compB[]); // we can use it to easily check if the messages are the same or not
		boolean copyCommand(uint8_t target[], uint8_t source[]); // copies command from source to target, returns true if they were the same;
		boolean checkDataOk(uint8_t data[]); // checks if data ACK byte is == A0;
		
		// Gets certain byte(s) from data. Again you can use setEcho to 0 if you have data without echo message at the beggining
		//	otherwise it should get echo from message automatically
		uint8_t getByte(uint8_t data[], uint8_t offset);
		uint16_t getInt(uint8_t data[], uint8_t offset);
		uint64_t getUint64(uint8_t data[], uint8_t offset, boolean reverseEndianess, uint8_t length);
		uint8_t getString(uint8_t data[], char string[], uint8_t offset, uint8_t length = 255); // returns string length
		uint8_t getArray(uint8_t data[], uint8_t array[], uint8_t offset, uint8_t length = 255); 
		void clearData(uint8_t data[]); // Fast way to clear data if needed
		
		// You can get response and echo lengths from commands below
		uint8_t getResponseLength();
		uint8_t getEcho();
		uint8_t setEcho(uint8_t echo);
		
		// KWP protocol handling
		boolean setKwp(boolean kwpSet) { return (kwp = kwpSet); };
		boolean getKwp() { return kwp; };
		
		// Some ECUs like DDE4 need delay between bytes sent
		boolean setSlowSend(boolean setS, uint8_t delay = 3) { 
			slowSend = delay;
			return (slow = setS);
			};
		
		
		// Get commands per second calculated from write command followed by readData
		float getRespondsPerSecond();
		
		// Clear RX buffer if overload happen
		void clearRX();
		void clearRX(uint8_t available, uint8_t length);
	
		Stream &serial;
	private:
		boolean kwp = false;
		boolean slow = false;
		boolean blocking = false;
		boolean messageSend = false;
		
		uint8_t slowSend = 4;
		uint8_t device = 0;
		uint8_t echoLength = 0, responseLength, maxDataLength = MAX_DATA_LENGTH;
		
		uint8_t ackByteOffset = 2;
		uint8_t ackByte = 0xA0;
		boolean ackByteCheck = true;
		
		uint8_t isoTimeout = ISO_TIMEOUT;
		uint32_t timeout = isoTimeout;
		
		volatile uint32_t timeStamp;
		float commandsPerSecond;
		
		uint8_t writeToSerial(uint8_t data[], uint8_t length);
};

#endif /* DS2_h */