#ifndef SERIALCLASS_H
#define SERIALCLASS_H

#include <string>
#ifdef __WIN32__
#include <windows.h>
#endif

class SerialClass {
public:
	SerialClass();
	virtual ~SerialClass();
	
	enum ErrorType {
		OK = 0,
		ERROR_OPENING,
		ERROR_CONFIG,
		ERROR_NOT_OPEN,
		ERROR_WRITE_FAIL,
	};
	
	ErrorType OpenPort(const char* name, int rate, int handshake = 0);
	ErrorType SetRate(int rate);
	void ClosePort();
	
	int SendBytes(void* data, int bytes);
	int ReceiveBytes(void* data, int bytes);
	int PendingBytes();
	
#ifdef __WIN32__
	HANDLE hComm;
#else
	int hComm;
#endif
	
private:

};

#endif /* SERIALCLASS_H */

