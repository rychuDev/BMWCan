
#include "DS2.h"

DS2::DS2(Stream &stream):serial(stream) {
	serial = stream;
}


boolean DS2::obtainValues(uint8_t command[], uint8_t data[], uint8_t respLen) {
	responseLength = respLen;
	clearData(data);
	clearRX();
	boolean block = blocking;
	blocking = true;
	writeData(command);
	boolean result = readData(data);
//	log_e("Data: %d2, %d2, %d2 ,%d2 ,%d2 ,%d2, %d2", data[0], data[1], data[2], data[3], data[4], data[5], data[6]);
	blocking = block;
	if(!result) clearRX();
	return (result && checkDataOk(data));
}


uint8_t DS2::sendCommand(uint8_t command[], uint8_t respLen) {
	if(messageSend) {
		return 0;
	} else {
		if(respLen != 0) responseLength = respLen;
		messageSend = true;
		clearRX();
		return writeData(command);
	}
}



uint8_t DS2::receiveData(uint8_t data[]) {
	uint32_t time;
//	log_e("Time: %d", millis() - timeStamp);
	if(messageSend) {
		if(readData(data)) {
			messageSend = false;
			if(!checkDataOk(data) || echoLength == responseLength) {
				return RECEIVE_BAD;
			}
			return RECEIVE_OK;
		} else if((time = (millis() - timeStamp)) < timeout) {
			return RECEIVE_WAITING;
		} else {
			messageSend = false;
//			return time;
			return RECEIVE_TIMEOUT;
		}
	} else return RECEIVE_WAITING;
}

void DS2::newCommand() {
	clearRX();
	messageSend = false;
}


boolean DS2::compareCommands(uint8_t compA[], uint8_t compB[]) {
	boolean same = true;
	uint8_t length;
	if(!kwp && compA[1] == compB[1]) length = compA[1];
	else if(kwp && compA[3] == compB[3]) length = compA[3] + 5;
	else return false;
	
	for(uint8_t i = 0; i < length; i++) {
		if(compA[i] != compB[i]) {
			same = false;
			break;
		}
	}
	return same;
}

boolean DS2::copyCommand(uint8_t target[], uint8_t source[]) {
	if(compareCommands(target, source)) return true;
	uint8_t length = source[1];
	if(kwp) length = source[3] + 5;
	for(uint8_t i = 0; i < length; i++) {
		target[i] = source[i];
	}
	return false;
}


uint8_t DS2::writeData(uint8_t data[], uint8_t length) {
	timeStamp = millis();
	if(!kwp) {
		device = data[0];
		echoLength = data[1];
	} else {
		device = data[1];
		echoLength = data[3] + 5;
	}
	if(length != 0) {
		responseLength = length;
		return writeToSerial(data, length);
	} else return writeToSerial(data, echoLength);
}

uint8_t DS2::writeToSerial(uint8_t data[], uint8_t length) {
	if(!slow) {
		length = serial.write(data, length);
	//	serial.flush();
		return length;
	}
	for(uint8_t i = 0; i < length; i++) {
		delay(slowSend);
		serial.write(data[i]);
	}
	return length;
}

boolean DS2::readCommand(uint8_t data[]) {
	if(!kwp && (blocking || serial.available() > 1)) {
			uint32_t startTime = millis();
			uint8_t checksum = serial.read();
			data[0] = checksum;
			echoLength = serial.read();
			data[1] = echoLength;
			checksum ^= echoLength;
			while(echoLength-2 > serial.available()) if(millis() - startTime > timeout) break;
			for(uint8_t i = 2; i < echoLength; i++) {
				data[i] = serial.read();
				checksum ^= data[i];
			}
			echoLength = 0;
			if(checksum != 0) return false;
			device = data[0];
			return true;
	}
	if(kwp && (blocking || serial.available() > 3)) {
		uint32_t startTime = millis();
		uint8_t checksum = serial.read();
		data[0] = checksum;
		data[1] = serial.read();
		data[2] = serial.read();
		data[3] = serial.read();
		echoLength = data[3] + 5;
		while(echoLength-4 > serial.available()) if(millis() - startTime > timeout) break;
		for(uint8_t i = 1; i < echoLength; i++) {
			if(i > 3) data[i] = serial.read();
			checksum ^= data[i];
		}
		echoLength = 0;
		if(checksum != 0) return false;
		device = data[1];
		return true;
	}
	return false;
}

boolean DS2::readData(uint8_t data[]) {
	uint32_t startTime = millis();
	uint8_t availible = serial.available();
	if(!blocking && echoLength != 0 && availible == 0) return false;
	if(!kwp && device != 0 && availible > 0 && serial.peek() != device) {
		serial.read();
		return readData(data);
	}
	if(blocking || availible > 2) {
		data[0] = 0xFF;
		uint32_t extraTimeout = 0;
		uint8_t echoOffset = kwp ? 4 : 2;
		if(echoLength + echoOffset > responseLength) responseLength = echoLength + echoOffset;
		data[echoLength + echoOffset] = 0xFF;
		if(echoLength > 100) extraTimeout = 100UL;
		
		for(uint8_t i = 0; i < responseLength; i++) {
			while((availible = serial.available()) == 0) {
				if(millis() - startTime > timeout + extraTimeout) break;
				delay(1);
			}
			if(availible == 0) break;
			data[i] = serial.read();
			// Check Echo
			if(i == echoOffset - 1) {
				if(echoLength != 0 && data[i] + (kwp ? 5 : 0) != echoLength) {
					echoLength = 0;
				}
			}
			if(i == echoLength + echoOffset - 1) responseLength = data[i] + echoLength + (kwp ? 5 : 0);
		}
		return checkData(data);
	}
	return false;
}

void DS2::clearData(uint8_t data[]) {
	for(uint8_t i = 0; i < maxDataLength; i++) {
		data[i] = 0;
	}
}


boolean DS2::checkData(uint8_t data[]) {
	uint8_t echo = 0;
	if(echoLength != 0) echo += (kwp ? data[3] + 5 : data[1]);
	uint8_t checksum = data[echo];
	uint8_t checkLen = (kwp ? data[echo+3]+echo + 5 : data[echo+1]+echo);
	for(uint8_t i = echo+1; i < checkLen; i++) {
		checksum ^= data[i];
	}

	if(checksum == 0) {
		/*
		if(echo != 0 && echo < maxDataLength/2 && data[echo+1] == echo) {
			boolean sameData = true;
			for(uint8_t i = 0; i < echo; i++) {
				if(data[i] != data[echo + i]) sameData = false;
			}
			if(sameData) return false;
		} */
		
		commandsPerSecond = 1000.0/(millis() - timeStamp);
		timeStamp = millis();
		return true;
	} else return false;
}

boolean DS2::checkDataOk(uint8_t data[]) {
	if(kwp) {
		if(data[echoLength + 2] == device) return true;
		else return false;
	}
	if(!ackByteCheck || data[echoLength+ackByteOffset] == ackByte) return true;
	else return false;
}

void DS2::setAckByte(uint8_t ack, uint8_t offset, boolean check) {
	ackByte = ack;
	ackByteOffset = offset;
	ackByteCheck = check;
}

void DS2::setBlocking(boolean mode) {
	blocking = mode;
}

boolean DS2::getBlocking() {
	return blocking;
}

void DS2::setTimeout(uint8_t timeoutMs) {
	isoTimeout = timeoutMs;
	timeout = isoTimeout;
}

void DS2::clearRX() {
	uint32_t startTime = millis();
	while(serial.available() > 0) {
		serial.read();
		if(millis() - startTime > timeout) break;
	}
}

void DS2::clearRX(uint8_t available, uint8_t length) {
	uint32_t startTime = millis();
	while(serial.available() > available) {
		for(uint8_t i = 0; i < length; i++) {
			serial.read();
		}
		if(millis() - startTime > timeout) break;
	}
}

uint8_t DS2::available() {
	return serial.available();
}

void DS2::flush() {
	serial.flush();
}

float DS2::getRespondsPerSecond(){
	return commandsPerSecond;
}

uint8_t DS2::getDevice() {
	return device;
}

uint8_t DS2::setDevice(uint8_t dev) {
	return device = dev;
}

uint8_t DS2::getEcho() {
	return echoLength;
}

uint8_t DS2::setEcho(uint8_t echo) {
	return (echoLength = echo);
}

uint8_t DS2::getResponseLength() {
	return responseLength;
}

void DS2::setMaxDataLength(uint8_t dataLength) {
	maxDataLength = dataLength;
}


// Getting data
uint8_t DS2::getByte(uint8_t data[], uint8_t offset) {
	uint8_t dataPoint = echoLength + offset + (kwp ? 4 : 3);
	return data[dataPoint];
}

uint16_t DS2::getInt(uint8_t data[], uint8_t offset){
	uint16_t result = 0;
	uint8_t dataPoint = echoLength + offset + (kwp ? 4 : 3);
	((uint8_t *)&result)[1] = data[dataPoint++];
	((uint8_t *)&result)[0] = data[dataPoint];
	return result;
}

uint64_t DS2::getUint64(uint8_t data[], uint8_t offset, boolean reverseEndianess = false, uint8_t length = 8) {
	uint64_t result = 0;
	uint8_t dataPoint = echoLength + offset + (kwp ? 4 : 3);
	for(uint8_t i = 0; i < length && i < 8; i++) {
		if(reverseEndianess) ((uint8_t *)&result)[i] = data[dataPoint+i];
		else ((uint8_t *)&result)[length-1-i] = data[dataPoint+i];
	}
	return result;
}
	
uint8_t DS2::getString(uint8_t data[], char string[], uint8_t offset, uint8_t length) {
	uint8_t charPos = 0;
	uint8_t totalOffset = offset + echoLength + (kwp ? 4 : 3);
	for(uint8_t i = totalOffset; i < length + totalOffset; i++) {
		string[charPos++] = (char) data[i];
		if(i + 1 == length + totalOffset) string[charPos++] = (char) 0;
		if(data[i] == 0) {
			charPos--;
			break;
		}
	}
	return charPos;
}

uint8_t DS2::getArray(uint8_t data[], uint8_t array[], uint8_t offset, uint8_t length) {
	uint8_t charPos = 0;
	uint8_t totalOffset = offset + echoLength + (kwp ? 4 : 3);
	for(uint16_t i = totalOffset; i < length + totalOffset; i++) {
		array[charPos++] = (char) data[i];
	}
	return charPos;
}