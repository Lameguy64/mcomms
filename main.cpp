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
#include "siofs.h"

#define VERSION "0.85b"

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


#define CRC32_REMAINDER		0xFFFFFFFF

void initTable32(unsigned int* table)
{
	int i,j;
	unsigned int crcVal;

	for( i=0; i<256; i++ )
	{
		crcVal = i;

		for( j=0; j<8; j++ )
		{
			if (crcVal&0x00000001L)
				crcVal = (crcVal>>1)^0xEDB88320L;
			else
				crcVal = crcVal>>1;
		}

		table[i] = crcVal;

	}

} /* initTable32 */

unsigned int crc32(void* buff, int bytes, unsigned int crc)
{
	int	i;
	unsigned char*	byteBuff = (unsigned char*)buff;
	unsigned int	byte;
	unsigned int	crcTable[256];

    initTable32( crcTable );

	for( i=0; i<bytes; i++ )
	{
		byte = 0x000000ffL&(unsigned int)byteBuff[i];
		crc = (crc>>8)^crcTable[(crc^byte)&0xff];
	}

	return( crc^0xFFFFFFFF );

} /* crc32 */

typedef struct {
	unsigned int pc0;
	unsigned int gp0;
	unsigned int t_addr;
	unsigned int t_size;
	unsigned int d_addr;
	unsigned int d_size;
	unsigned int b_addr;
	unsigned int b_size;
	unsigned int sp_addr;
	unsigned int sp_size;
	unsigned int sp;
	unsigned int fp;
	unsigned int gp;
	unsigned int ret;
	unsigned int base;
} EXEC;

typedef struct {
	char header[8];
	char pad[8];
	EXEC params;
	char license[64];
	char pad2[1908];
} PSEXE;

typedef struct {
	EXEC params;
	unsigned int crc32;
	unsigned int flags;
} EXEPARAM;

typedef struct {
    int size;
    unsigned int addr;
    unsigned int crc32;
} BINPARAM;

void* loadCPE(FILE* fp, EXEC* param)
{	
	int v;
	unsigned int uv;
	
	char* exe_buff = nullptr;
	int exe_size = 0;
	unsigned int exe_entry = 0;
	
	std::vector<unsigned int> addr_list;
	
	fseek( fp, 0, SEEK_SET );
	fread( &v, 1, 4, fp );
	
	if( v != 0x01455043 )
	{
		printf( "ERROR: File is not a PlayStation EXE or CPE file.\n" );
		return nullptr;
	}
	
	memset( param, 0x0, sizeof(EXEC) );
	
	v = 0;
	fread( &v, 1, 1, fp );
	
	while( v )
	{
		switch( v )
		{
		case 0x1:
			
			fread( &uv, 1, 4, fp );
			fread( &v, 1, 4, fp );
			
			addr_list.push_back( uv );
			exe_size += v;
			
			fseek( fp, v, SEEK_CUR );
			
			break;
			
		case 0x3:
			
			v = 0;
			fread( &v, 1, 2, fp );
			
			if( v != 0x90 )
			{
				printf( "ERROR: Unknown SETREG code: %d\n" );
				return nullptr;
			}
			
			fread( &exe_entry, 1, 4, fp );
			
			break;
			
		case 0x8:	// Select unit
			
			v = 0;
			fread( &v, 1, 1, fp );
			
			break;
			
		default:
			printf( "Unknown chunk found: %d\n", v );
			return nullptr;
		}
		
		v = 0;
		fread(&v, 1, 1, fp);
		
	}
	
	unsigned int addr_upper=0;
	unsigned int addr_lower=0;
	
	for( int i=0; i<addr_list.size(); i++ )
	{
		if( addr_list[i] > addr_upper )
		{
			addr_upper = addr_list[i];
		}
	}
	
	addr_lower = addr_upper;
	
	for( int i=0; i<addr_list.size(); i++ )
	{
		if( addr_list[i] < addr_lower )
		{
			addr_lower = addr_list[i];
		}
	}
	
	exe_size = 2048*((exe_size+2047)/2048);
	
	exe_buff = (char*)malloc( exe_size );
	memset( exe_buff, 0x0, exe_size );
	
	v = 0;
	fseek( fp, 4, SEEK_SET);
	fread( &v, 1, 1, fp );
	
	while( v )
	{	
		switch( v )
		{
		case 0x1:
			
			fread( &uv, 1, 4, fp );
			fread( &v, 1, 4, fp );
			
			fread( exe_buff+(uv-addr_lower), 1, v, fp );
			
			break;
			
		case 0x3:
			
			v = 0;
			fread( &v, 1, 2, fp );
			
			if ( v == 0x90 )
				fseek( fp, 4, SEEK_CUR);
			
			break;
			
		case 0x8:
			
			fseek( fp, 1, SEEK_CUR);
			
			break;
		}
		
		v = 0;
		fread( &v, 1, 1, fp );
		
	}
	
	param->pc0 = exe_entry;
	param->t_addr = addr_lower;
	param->t_size = exe_size;
	param->sp_addr = 0x801ffff0;
	
	return( exe_buff );
	
} /* loadCPE */

int uploadEXE( const char* exefile, SerialClass* serial )
{
	
	PSEXE exe;
	EXEPARAM param;
	char* buffer;
	
	FILE* fp = fopen(exefile, "rb");
	
	if( fp == nullptr )
	{
		printf( "ERROR: File not found.\n" );
		return( -1 );
	}
	
	if( fread( &exe, 1, sizeof(PSEXE), fp) != sizeof(PSEXE) )
	{
		printf( "ERROR: Read error or invalid file.\n" );
		fclose( fp );
		return( -1 );
	}
	
	if ( memcmp( exe.header, "PS-X EXE", 8 ) )
	{
		buffer = (char*)loadCPE( fp, &param.params );
		
		if( buffer == nullptr )
		{
			fclose( fp );
			return( -1 );
		}
		
	}
	else
	{
		buffer = (char*)malloc( exe.params.t_size );
	
		if ( fread( buffer, 1, exe.params.t_size, fp ) != exe.params.t_size )
		{
			printf( "ERROR: Incomplete file or read error occurred.\n" );
			free( buffer );
			fclose(fp);
			
			return( -1 );
		}
		
		memcpy( &param.params, &exe.params, sizeof(EXEPARAM) );
		
	}
	
	fclose( fp );
	
	param.crc32 = crc32( buffer, param.params.t_size, CRC32_REMAINDER );
	param.flags = 0;
	
	serial->SendBytes( (void*)"MEXE", 4 );
	
	char reply[4];
	int timeout = true;
	for( int i=0; i<10; i++ )
	{
		if( serial->ReceiveBytes( reply, 1 ) > 0 )
		{
			timeout = false;
			break;
		}
		Sleep(10);
	}
	
	if( timeout )
	{
		printf( "ERROR: No response from console.\n" );
		return( -1 );
	}
	
	if( reply[0] != 'K' )
	{
		printf( "ERROR: No valid response from console.\n" );
		return -1;
	}
	
	if( !old_protocol )
	{
		serial->SendBytes( &param, sizeof(EXEPARAM) );
	}
	else
	{
		serial->SendBytes( &param, sizeof(EXEPARAM)-4 );
	}
	
	Sleep( 20 );
	serial->SendBytes( buffer, param.params.t_size );
	
	free( buffer );
	
	return( 0 );
	
} /* uploadEXE */

int uploadBIN( const char* file, unsigned int addr, SerialClass* serial, int patch )
{
	BINPARAM param;
	char* buffer;
	
	FILE* fp = fopen( file, "rb" );
	
	if( fp == nullptr )
	{
		printf( "ERROR: File not found.\n" );
		return( -1 );
	}
	
	fseek( fp, 0, SEEK_END );
	param.size = ftell( fp );
	fseek( fp, 0, SEEK_SET );
	
	buffer = (char*)malloc( param.size );
	
	if( fread( buffer, 1, param.size, fp ) != param.size )
	{
		printf( "ERROR: Read error occurred.\n" );
		free( buffer );
		fclose( fp );
		return( -1 );
	}
	
	fclose( fp );
	
	if( patch )
	{
		serial->SendBytes( (void*)"MPAT", 4 );
	}
	else
	{
		serial->SendBytes( (void*)"MBIN", 4 );
	}
	
	char reply[4];
	int timeout = true;
	
	for( int i=0; i<10; i++ )
	{
		if ( serial->ReceiveBytes( reply, 1 ) > 0 )
		{
			timeout = false;
			break;
		}
		Sleep( 10 );
	}
	
	if( timeout )
	{
		printf( "ERROR: No response from console.\n" );
		return( -1 );
	}
	
	param.addr = addr;
	param.crc32 = crc32( buffer, param.size, CRC32_REMAINDER );
	
	serial->SendBytes( &param, sizeof(BINPARAM) );
	Sleep( 20 );
	serial->SendBytes( buffer, param.size );
	
	free( buffer );
	
	return( 0 );
	
} /* uploadBIN */

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
			printf( "    -baud <rate>  - Specify serial baud rate (default: 115200).\n" );
			printf( "                    Note: This does not affect the rate used for PS-EXE upload.\n" );
			printf( "    -dir <path>   - Specify initial host directory for SIOFS.\n" );
			printf( "    -term         - Enable terminal mode (forward keystrokes to serial).\n" );
			printf( "    -hex          - Output received data in hex.\n" );
			printf( "    -fsmsg        - Output SIOFS messages.\n" );
			printf( "    -nocons       - Quit immediately, no console mode.\n" );
			printf( "    -hshake       - Enable serial handshake, DTR and RTS always high otherwise.\n" );
			printf( "    -old          - Use old LITELOAD 1.0 protocol.\n\n" );

			printf( "  LITELOAD Parameters (catflap inspired):\n" );
			printf( "    up <file> <addr> - Upload a file to specified address.\n" );
			printf( "    patch <file>     - Upload a patch binary.\n" );
			printf( "    run <file>       - Upload a PS-EXE file (CPE format is supported).\n" );
			printf( "    Specified memory address must be in hex.\n\n" );

			printf( "  Defaults can be changed with environment variables:\n" );
			printf( "    MC_DEVICE - Serial device.\n" );
			printf( "    MC_BAUD   - Baud rate.\n" );

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
	
	printf( "Listening %s at %d baud.\n", serial_device.c_str(), serial_baud );
	
	if ( !terminal_mode )
	{
		printf( "Press ESC to quit...\n\n" );
	}
	else
	{
		printf( "Terminal Mode! Press Ctrl+C to quit...\n\n" );
	}
	
#ifndef __WIN32__
	
	do_quit = 0;
	st.sa_handler = term_func;
	sigemptyset( &st.sa_mask );
	st.sa_flags = SA_RESTART;
	sigaction( SIGINT, &st, NULL );
	
	enable_raw_mode();
	
#endif /* __WIN32__ */
	
	unsigned char keypress[4] = {0};
	int keylen = 0;
	
	while( (!quit) && (!do_quit) )
	{
		char buffer[256];
		
		// Read characters
		keylen = 0;
		
		while( _kbhit() )
		{
			keypress[keylen] = getchar();
			keylen++;
		}
		
		if( keylen > 0 )
		{
			if( keypress[0] == 224 )
			{
				if( keypress[1] == 134 )
				{
					quit = true;
				}
			}
			
			if( terminal_mode )
			{
				serial.SendBytes( keypress, keylen );
			}
			else
			{
				if( keypress[0] == 27 )
				{
					quit = true;
				}
			}
		}
		
		memset( buffer, 0, 256 );
		
		if ( serial.PendingBytes() )
		{
			int len = serial.ReceiveBytes( buffer, 256 );
			
			if( len > 0 )
			{
				if( strchr( buffer, '~' ) )
				{
					if( siofs.Query( strchr( buffer, '~' ), &serial ) )
					{
						*strchr( buffer, '~' ) = 0x0;
						len -= 4;
					}
				}

				if ( !hex_mode )
				{
					fputs( buffer, stdout );
					fflush( stdout );
				}
				else
				{
					for(int i=0; i<len; i++) {
						
						int val = *((unsigned char*)&buffer[i]);
						printf( "%02x,", val );
						
						if ( (i%4) == 3 )
						{
							putchar( '\n' );
						}
						
					}
					printf( "\n--\n" );
				}
			}
		}
		
#ifndef __WIN32__
		usleep(1000);
#endif
	
	}

#ifndef __WIN32__
	disable_raw_mode();
#endif
	
	serial.ClosePort();
	
	return( EXIT_SUCCESS );
	
} /* main */
