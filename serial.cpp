#include <math.h>
#include <stdio.h>
#include <sys/file.h>
#include <string.h>
#include <unistd.h>
#include <iostream>
#ifndef __WIN32__
#include <sys/ioctl.h>
#include <termios.h>
#include <bits/time.h>
#endif
#include "serial.h"

SerialClass::SerialClass()
{
#ifdef __WIN32__
	hComm = INVALID_HANDLE_VALUE;
#else
	hComm = -1;
#endif

} /* SerialClass::SerialClass */

SerialClass::~SerialClass()
{	
#ifdef __WIN32__
	if ( hComm != INVALID_HANDLE_VALUE ) {
		CloseHandle( hComm );
	}
#else
	if ( hComm >= 0 ) {
		close( hComm );
	}
#endif
	
} /* SerialClass::~SerialClass */

SerialClass::ErrorType SerialClass::OpenPort(const char* name, int rate, int handshake)
{	
#ifdef __WIN32__

	/* serial device open for Win32 */
	
	DCB dcbSerialParams;
	
	if( hComm != INVALID_HANDLE_VALUE )
	{
		CloseHandle( hComm );
	}
	
	hComm = CreateFile( name,			// port name
		GENERIC_READ | GENERIC_WRITE,	// Read/Write
		0,								// No Sharing
		NULL,							// No Security
		OPEN_EXISTING,					// Open existing port only
		0,								// Non Overlapped I/O
		NULL );							// Null for Comm Devices
	 
	if ( hComm == INVALID_HANDLE_VALUE )
	{
		return( ERROR_OPENING );
	}
	
	if ( !GetCommState( hComm, &dcbSerialParams ) )
	{
		return( ERROR_CONFIG );
	}
	
	dcbSerialParams.BaudRate		= rate;			// Setting BaudRate = rate
	dcbSerialParams.ByteSize		= 8;			// Setting ByteSize = 8
	dcbSerialParams.StopBits		= ONESTOPBIT;	// Setting StopBits = 1
	dcbSerialParams.Parity			= NOPARITY;		// Setting Parity = None
	dcbSerialParams.fDtrControl		= DTR_CONTROL_ENABLE;
		
	if( handshake )
	{
		dcbSerialParams.fOutxCtsFlow    = TRUE;
		dcbSerialParams.fOutxDsrFlow    = TRUE;
		dcbSerialParams.fRtsControl		= RTS_CONTROL_HANDSHAKE;
		printf( "Hardware handshake enabled.\n" );
	}
	else
	{
		dcbSerialParams.fOutxCtsFlow    = FALSE;
		dcbSerialParams.fOutxDsrFlow    = FALSE;
		dcbSerialParams.fRtsControl		= RTS_CONTROL_ENABLE;
	}
	
	if( !SetCommState( hComm, &dcbSerialParams ) )
	{
		return ERROR_CONFIG;
	}
	
	COMMTIMEOUTS timeouts;
	
	timeouts.ReadIntervalTimeout         = 50;
	timeouts.ReadTotalTimeoutConstant    = 50;
	timeouts.ReadTotalTimeoutMultiplier  = 2;
	timeouts.WriteTotalTimeoutConstant   = 50;
	timeouts.WriteTotalTimeoutMultiplier = 2;
	
	if( !SetCommTimeouts( hComm, &timeouts ) )
	{
		return ERROR_CONFIG;
	}
	
#else
	
	struct termios tty;
	
	/* serial device open routine for Linux */
	
	hComm = open( name , O_RDWR|O_NOCTTY );
	
	if ( hComm == -1 )
	{
		return( ERROR_OPENING );
	}
	
	/* configure the serial device */
	tcgetattr( hComm, &tty );
	
	/* setup serial UART (this works best for me -Lameguy64) */
	tty.c_cflag |= (CLOCAL | CREAD);    // ignore modem controls 
	tty.c_cflag &= ~CSIZE;
	tty.c_cflag |= CS8;					// 8-bit characters 
	tty.c_cflag &= ~PARENB;				// no parity bit 
	tty.c_cflag &= ~CSTOPB;				// only need 1 stop bit 
	tty.c_cflag &= ~CRTSCTS;			// no hardware flowcontrol 
	
	// setup for non-canonical mode
	tty.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL | IXON);
	tty.c_lflag &= ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);
	tty.c_oflag &= ~OPOST;
	
	/* set flow control if enabled */
	if( handshake )
	{
		printf( "Hardware handshake enabled.\n" );
		tty.c_cflag |= CRTSCTS;
	}
	
	/* ignore parity errors */
	tty.c_iflag = IGNPAR;
	
	/* set the baud rate */
	cfsetspeed( &tty, (speed_t)rate );
	
	/* fetch bytes as they become available */
	tty.c_cc[VMIN] = 0;
	tty.c_cc[VTIME] = 2;
		
	/* apply port configuration */
	if ( tcsetattr( hComm, TCSANOW, &tty ) != 0 )
	{
        return( ERROR_CONFIG );
    }
	
#endif
	
	return( OK );
	
} /* SerialClass::OpenPort */

SerialClass::ErrorType SerialClass::SetRate(int rate) {

#ifdef __WIN32__	

	DCB dcbSerialParams;

	if ( hComm == INVALID_HANDLE_VALUE ) {
		return ERROR_NOT_OPEN;
	}
		
	if( !GetCommState( hComm, &dcbSerialParams ) )
	{
		return ERROR_CONFIG;
	}
	
	dcbSerialParams.BaudRate = rate;		// Setting BaudRate = 9600
	dcbSerialParams.ByteSize = 8;			// Setting ByteSize = 8
	dcbSerialParams.StopBits = ONESTOPBIT;	// Setting StopBits = 1
	dcbSerialParams.Parity   = NOPARITY;	// Setting Parity = None
	
	if ( !SetCommState( hComm, &dcbSerialParams ) )
	{
		return ERROR_CONFIG;
	}
	
#else

	struct termios tty;

	if ( hComm == -1 )
	{
		return ERROR_NOT_OPEN;
	}
	
	tcgetattr( hComm, &tty );
	
	cfsetspeed( &tty, (speed_t)rate );
	
    if ( tcsetattr( hComm, TCSANOW, &tty ) != 0 )
	{
        return( ERROR_CONFIG );
    }
	
#endif
	
	return( OK );
}

int SerialClass::PendingBytes()
{
#ifdef __WIN32__
	
	DWORD dwErrorFlags;
	COMSTAT ComStat;

	ClearCommError(hComm, &dwErrorFlags, &ComStat);

	return( (int)ComStat.cbInQue );
	
#else
	
	int bytes;
	ioctl(hComm, FIONREAD, &bytes);
	
	return( bytes );
	
#endif
	
} /* SerialClass::PendingBytes */

int SerialClass::SendBytes(void* data, int length)
{	
#ifdef __WIN32__
	DWORD bytesWritten;
	
	if ( !WriteFile( hComm, data, length, &bytesWritten, NULL ) ) {
		return -1;
	}
	
#else
	
	int bytesWritten;
	
	bytesWritten = write(hComm, data, length);
	
#endif
	
	return( bytesWritten );
	
} /* SerialClass::SendBytes */

int SerialClass::ReceiveBytes(void* data, int bytes)
{
#ifdef __WIN32__

	DWORD bytesReceived;
	
	if( !ReadFile( hComm, data, bytes, &bytesReceived, NULL )  )
	{
		return( -1 );
	}
	
#else
	int bytesReceived;
	struct timeval timeout;
	
	fd_set read_fds, write_fds, except_fds;
	
	FD_ZERO(&read_fds);
	FD_ZERO(&write_fds);
	FD_ZERO(&except_fds);
	FD_SET(hComm, &read_fds);
	
	timeout.tv_sec = 1;
	timeout.tv_usec = 0;
	
	if ( select(hComm+1, &read_fds, &write_fds, &except_fds, &timeout) == 1 ) {
	
		bytesReceived = read(hComm, data, bytes);
	
	} else {
		
		return( -1 );
		
	}
	
#endif
	
	return( bytesReceived );
	
} /* SerialClass::ReceiveBytes */

void SerialClass::ClosePort() {
	
#ifdef __WIN32__
	if ( hComm != INVALID_HANDLE_VALUE ) {
		CloseHandle( hComm );
		hComm = INVALID_HANDLE_VALUE;
	}
#else
	if ( hComm >= 0 ) {
		close( hComm );
		hComm = -1;
	}
#endif
	
}
