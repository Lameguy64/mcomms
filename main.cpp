#ifdef __WIN32__
#include <conio.h>
#else
#include <unistd.h>
#include <termios.h>
#include <signal.h>
#endif

#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include <iostream>
#include <iomanip>
#include <vector>

#include "serial.h"
#include "upload.h"
#include "siofs.h"

#define VERSION "0.87"

#ifdef __WIN32__
#define SERIAL_DEFAULT "COM1"
#else
#define SERIAL_DEFAULT "/dev/ttyUSB0"
#endif

std::string serial_device = SERIAL_DEFAULT;
int serial_baud = 115200;

std::string psexe_file;
std::string bin_file;
std::string pat_file;
unsigned int bin_addr;

int old_protocol = false;
int terminal_mode = false;
int no_console = false;
int hex_mode = false;
extern int fs_messages;

int do_quit;

SerialClass		serial;
SiofsClass		siofs;	

#ifndef __WIN32__

struct termios orig_term;

void enable_raw_mode()
{
    struct termios term;
    tcgetattr(0, &orig_term);
	
	term = orig_term;
    term.c_lflag &= ~(ICANON | ECHO); // Disable echo as well
    tcsetattr(0, TCSANOW, &term);
}

void disable_raw_mode()
{
    tcsetattr(0, TCSANOW, &orig_term);
}

int _kbhit()
{    
	struct timeval tv = { 0L, 0L };
    
	fd_set fds;
    FD_ZERO(&fds);
    FD_SET(0, &fds);
	
    return select(1, &fds, NULL, NULL, &tv);
	
} /* _kbhit */

void term_func(int signum)
{
	if( do_quit )
	{
		printf("Ok, I will kill myself.\n");
		disable_raw_mode();
		exit( 0 );
	}
	printf("Catching SIGINT...\n");
	serial.ClosePort();
	
	do_quit = 1;
	
} /* term_func */

#else

BOOL WINAPI term_func(DWORD dwCtrlType)
{
	if( do_quit )
	{
		printf("Ok, I will commit sepukku.\n");
		exit(0);
	}
	
	printf("Catching Ctrl+C...\n");
	serial.ClosePort();
	
	putchar('\n');
	do_quit = 1;
}

#endif /* __WIN32__ */

int main( int argc, char** argv )
{
#ifndef __WIN32
	struct sigaction st;
#endif // __WIN32
	
	int quit = false;
	int hshake = false;
	
	printf( "MCOMMS " VERSION " for LITELOAD and n00bROM by Lameguy64 (SIOFS Revision %d.%d)\n",
		SIOFS_MAJOR, SIOFS_MINOR );
		
	printf( "2018-2020 Meido-Tek Productions (specify '-h' or '-?' for help)\n\n" );
	
	if ( argc >= 2 )
	{
		if( ( strcmp( "-h", argv[1] ) == 0 ) || ( strcmp( "-?", argv[1] ) == 0 ) )
		{
			printf( "Usage:\n" );
			printf( "  mcomms [-dev <device>] [-baud <rate>] [-dir <path>] [-term] [-fsmsg]\n" );
			printf( "  [up <file> <addr>] [run <exefile>]\n\n" );

			printf( "    -dev <device> - Specify serial port device (default: " SERIAL_DEFAULT ").\n" );
			printf( "    -baud <rate>  - Specify serial console baud rate (default: 115200).\n" );
			printf( "                    Note: PS-EXE and binary uploads still use 115200 baud.\n" );
			printf( "    -dir <path>   - Specify initial directory for SIOFS.\n" );
			//printf( "    -term         - Enable terminal mode (forward keystrokes to serial).\n" );
			printf( "    -hex          - Output received bytes in hex.\n" );
			printf( "    -fsmsg        - Output SIOFS messages.\n" );
			printf( "    -nocons       - Upload only, no console mode.\n" );
			printf( "    -hshake       - Enable serial flow control, DTR and RTS always set otherwise.\n" );
			printf( "    -old          - Use old LITELOAD 1.0 protocol.\n\n" );

			printf( "  LITELOAD Commands (catflap inspired):\n" );
			printf( "    up <file> <addr> - Upload a file to specified address.\n" );
			printf( "    patch <file>     - Upload a patch binary.\n" );
			printf( "    run <file>       - Upload a PS-EXE file (CPE format is supported).\n" );
			printf( "    Memory address must be specified in hex.\n\n" );

			printf( "  Serial defaults can be changed with environment variables:\n" );
			printf( "    MC_DEVICE - Serial device.\n" );
			printf( "    MC_BAUD   - Baud rate.\n" );
			printf( "    MC_HSHAKE - Hardware handshake (specify true or false).\n" );

			return( EXIT_SUCCESS );
		}
	}
	
	// Search environment variables
	if( getenv( "MC_DEVICE" ) )
	{
		serial_device = getenv( "MC_DEVICE" );
	}
	if( getenv( "MC_BAUD" ) )
	{
		serial_baud = atoi( getenv( "MC_BAUD" ) );
	}
	if( getenv( "MC_HSHAKE" ) )
	{
		if( strcasecmp( getenv( "MC_HSHAKE" ), "true" ) == 0 )
		{
			hshake = true;
		}
		else
		{
			hshake = false;
		}
	}
	
	for( int i=1; i<argc; i++ )
	{
		
		if( strcmp( "-dev", argv[i] ) == 0 )
		{	
			i++;
			if( i >= argc )
			{
				printf( "Missing device parameter.\n" );
				return( EXIT_FAILURE );
			}
			serial_device = argv[i];	
		}
		else if ( strcmp( "-baud", argv[i] ) == 0 )
		{
			i++;
			if( i >= argc )
			{
				printf( "Missing baud rate parameter.\n" );
				return( EXIT_FAILURE );
			}
			
			serial_baud = atoi( argv[i] );
			
		}
		else if( strcmp( "-dir", argv[i] ) == 0 )
		{
			i++;
			if( i >= argc )
			{
				printf( "Missing path parameter.\n" );
				return( EXIT_FAILURE );
			}
			
			if( chdir( argv[i] ) )
			{
				printf( "Unable to change to specified directory.\n" );
				return( EXIT_FAILURE );
			}
		}
		else if( strcmp( "-term", argv[i] ) == 0 )
		{
			terminal_mode = true;
		}
		else if( strcmp( "-hex", argv[i] ) == 0 )
		{
			hex_mode = true;
		}
		else if( strcmp( "-nocons", argv[i] ) == 0 )
		{
			no_console = true;
		}
		else if( strcmp( "-old", argv[i] ) == 0 )
		{
			old_protocol = true;	
		}
		else if( strcmp( "-hshake", argv[i] ) == 0 )
		{
			hshake = true;
		}
		else if( strcmp( "-fsmsg", argv[i] ) == 0 )
		{
			fs_messages = true;
		}
		else if( strcmp( "run", argv[i] ) == 0 )
		{
			i++;
			if ( i >= argc )
			{
				printf( "Missing filename parameter.\n" );
				return( EXIT_FAILURE );
			}
			psexe_file = argv[i];
			break;
		
		}
		else if( strcmp( "patch", argv[i] ) == 0 )
		{
			i++;
			if( i >= argc )
			{
				printf( "Missing filename parameter.\n" );
				return( EXIT_FAILURE );
			}
			pat_file = argv[i];
			break;
		}
		else if( strcmp( "up", argv[i] ) == 0 )
		{
			i++;
			if( i >= argc )
			{
				printf( "Missing filename parameter.\n" );
				return( EXIT_FAILURE );
			}
			bin_file = argv[i];
			i++;
			if( i >= argc )
			{
				printf( "Missing address parameter.\n" );
				return( EXIT_FAILURE );
			}
			sscanf( argv[i], "%x", &bin_addr );
			break;
		}
		else
		{
			printf( "Unknown parameter: %s\n", argv[i] );
			return( EXIT_FAILURE );
		}
		
	}

#ifdef __WIN32
	printf( "Using %s...\n", serial_device.c_str() );
#else
	printf( "Using serial device %s...\n", serial_device.c_str() );
#endif

	// Open serial port
	switch( serial.OpenPort( serial_device.c_str(), serial_baud, hshake ) )
	{
	case SerialClass::ERROR_OPENING:
		printf( "ERROR: Unable to open %s.\n", serial_device.c_str() );
		return( EXIT_FAILURE );
	case SerialClass::ERROR_CONFIG:
		printf( "ERROR: Unable to configure %s.\n", serial_device.c_str() );
		return( EXIT_FAILURE );
	}
	
	// Upload patch data
	if( !pat_file.empty() )
	{
		printf( "Uploading patch file...\n" );
		
		if( uploadBIN( pat_file.c_str(), 0, &serial, 1 ) < 0 )
		{
			serial.ClosePort();
			return( EXIT_FAILURE );
		}
		
		serial.ClosePort();
		
		return( EXIT_SUCCESS );
	}
	
	// Upload binary file if specified
	if( !bin_file.empty() )
	{
		printf( "Uploading binary file...\n" );
		
		if( uploadBIN( bin_file.c_str(), bin_addr, &serial, 0 ) < 0 )
		{
			serial.ClosePort();
			return( EXIT_FAILURE );
		}
		
		serial.ClosePort();
		return( EXIT_SUCCESS );
	}
	
	// Upload executable if specified
	if( !psexe_file.empty() )
	{
		printf( "Uploading executable...\n" );
		
		if ( uploadEXE(psexe_file.c_str(), &serial) < 0 )
		{
			serial.ClosePort();
			return( EXIT_FAILURE );
		}
	}
	
	if( no_console )
	{
		serial.ClosePort();
		return( EXIT_SUCCESS );
	}
	
	printf( "Listening at %d baud.\n", serial_baud );
	printf( "Press Ctrl+C to quit...\n---\n" );
	
#ifndef __WIN32__
	
	do_quit = 0;
	st.sa_handler = term_func;
	sigemptyset( &st.sa_mask );
	st.sa_flags = SA_RESTART;
	sigaction( SIGINT, &st, NULL );
	
	enable_raw_mode();
	
#else

	SetConsoleCtrlHandler(term_func, TRUE);

#endif /* __WIN32__ */
	
	unsigned char keypress[4] = {0};
	int keylen = 0;
	
	while( (!quit) && (!do_quit) )
	{
		char buffer[256];
		
		// Read characters
		keylen = 0;
		
#ifdef __WIN32
		while( _kbhit() )
		{
			keypress[keylen] = _getch();
			keylen++;
		}
#else
		if( _kbhit() )
		{
			if( read( 0, &keypress[keylen], 1 ) > 0 )
				keylen++;
		}
#endif
		
		if( keylen > 0 )
		{
			serial.SendBytes( keypress, keylen );
		}
		
		memset( buffer, 0, 256 );
		
		if ( serial.PendingBytes() )
		{
			int len = serial.ReceiveBytes( buffer, 256 );
			
			if( len > 0 )
			{
				// if it contains a tilde, check if it is a SIOFS command
				if( strchr( buffer, '~' ) )
				{
					if( siofs.Query( strchr( buffer, '~' ), &serial ) )
					{
						// if command was recognized, trim off the command
						*strchr( buffer, '~' ) = 0x0;
						len -= 4;
					}
				}

				// output received text
				if ( !hex_mode )
				{
					fwrite( buffer, 1, len, stdout );
					fflush( stdout );
				}
				else
				{
					for( int i=0; i<len; i++ )
					{
						
						int val = *((unsigned char*)&buffer[i]);
						printf( "%02x,", val );
						
						if( (i%4) == 3 )
						{
							putchar( '\n' );
						}
						
					}
					printf( "\n--\n" );
				}
			}
		}
		
#ifndef __WIN32__
		usleep( 1000 );
#endif
	
	}

#ifndef __WIN32__
	disable_raw_mode();
#endif
	
	serial.ClosePort();
	
	return( EXIT_SUCCESS );
	
} /* main */
