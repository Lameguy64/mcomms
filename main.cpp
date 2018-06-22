/* 
 * File:   main.cpp
 * Author: Lameguy64
 *
 * Created on June 2, 2018, 9:06 PM
 */

#ifdef __WIN32__
#include <conio.h>
#else
#include <unistd.h>
#endif

#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include <iostream>
#include <iomanip>
#include <vector>

#include "SerialClass.h"
#include "SiofsClass.h"

#define VERSION "0.80"

#ifdef __WIN32__
#define SERIAL_DEFAULT "COM1"
#else
#define SERIAL_DEFAULT "/dev/ttyUSB0"
#endif

std::string serial_device = SERIAL_DEFAULT;
int serial_baud = 115200;

std::string psexe_file;
std::string bin_file;
unsigned int bin_addr;

int terminal_mode = false;
int hex_mode = false;
extern int fs_messages;


#define CRC32_REMAINDER		0xFFFFFFFF

void initTable32(unsigned int* table) {

	int i,j;
	unsigned int crcVal;

	for(i=0; i<256; i++) {

		crcVal = i;

		for(j=0; j<8; j++) {

			if (crcVal&0x00000001L)
				crcVal = (crcVal>>1)^0xEDB88320L;
			else
				crcVal = crcVal>>1;

		}

		table[i] = crcVal;

	}

}

unsigned int crc32(void* buff, int bytes, unsigned int crc) {

	int	i;
	unsigned char*	byteBuff = (unsigned char*)buff;
	unsigned int	byte;
	unsigned int	crcTable[256];

    initTable32(crcTable);

	for(i=0; i<bytes; i++) {

		byte = 0x000000ffL&(unsigned int)byteBuff[i];
		crc = (crc>>8)^crcTable[(crc^byte)&0xff];

	}

	return(crc^0xFFFFFFFF);

}

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
} EXEPARAM;

typedef struct {
    int size;
    unsigned int addr;
    unsigned int crc32;
} BINPARAM;

void* loadCPE(FILE* fp, EXEC* param) {
	
	int v;
	unsigned int uv;
	
	char* exe_buff = nullptr;
	int exe_size = 0;
	unsigned int exe_entry = 0;
	
	std::vector<unsigned int> addr_list;
	
	fseek(fp, 0, SEEK_SET);
	fread(&v, 1, 4, fp);
	
	if ( v != 0x01455043 ) {
		std::cout << "ERROR: File is not a PlayStation EXE or CPE file." << std::endl;
		return nullptr;
	}
	
	memset(param, 0x0, sizeof(EXEC));
	
	v = 0;
	fread(&v, 1, 1, fp);
	
	while( v ) {
		
		switch( v ) {
			case 0x1:
				
				fread(&uv, 1, 4, fp);
				fread(&v, 1, 4, fp);
				
				addr_list.push_back(uv);
				exe_size += v;
				
				fseek(fp, v, SEEK_CUR);
				
				break;
				
			case 0x3:
				
				v = 0;
				fread(&v, 1, 2, fp);
				
				if ( v != 0x90 ) {
					std::cout << "ERROR: Unknown SETREG code: " << v << std::endl;
					return nullptr;
				}
				
				fread(&exe_entry, 1, 4, fp);
				
				break;
				
			case 0x8:	// Select unit
				
				v = 0;
				fread(&v, 1, 1, fp);
				
				break;
				
			default:
				std::cout << "Unknown chunk found: " << v << std::endl;
				return nullptr;
		}
		
		v = 0;
		fread(&v, 1, 1, fp);
		
	}
	
	
	unsigned int addr_upper=0;
	unsigned int addr_lower=0;
	
	for(int i=0; i<addr_list.size(); i++) {
		if ( addr_list[i] > addr_upper ) {
			addr_upper = addr_list[i];
		}
	}
	
	addr_lower = addr_upper;
	
	for(int i=0; i<addr_list.size(); i++) {
		if ( addr_list[i] < addr_lower ) {
			addr_lower = addr_list[i];
		}
	}
	
	exe_size = 2048*((exe_size+2047)/2048);
	
	exe_buff = (char*)malloc(exe_size);
	memset(exe_buff, 0x0, exe_size);
	
	v = 0;
	fseek(fp, 4, SEEK_SET);
	fread(&v, 1, 1, fp);
	
	while( v ) {
		
		switch( v ) {
			case 0x1:
				
				fread(&uv, 1, 4, fp);
				fread(&v, 1, 4, fp);
				
				fread(exe_buff+(uv-addr_lower), 1, v, fp);
				
				break;
				
			case 0x3:
				
				v = 0;
				fread(&v, 1, 2, fp);
				
				if ( v == 0x90 ) {
					fseek(fp, 4, SEEK_CUR);
				}
				
				break;
				
			case 0x8:
				
				fseek(fp, 1, SEEK_CUR);
				
				break;
		}
		
		v = 0;
		fread(&v, 1, 1, fp);
		
	}
	
	param->pc0 = exe_entry;
	param->t_addr = addr_lower;
	param->t_size = exe_size;
	param->sp_addr = 0x801ffff0;
	
	return exe_buff;
	
}

int uploadEXE(const char* exefile, SerialClass* serial) {
	
	PSEXE exe;
	EXEPARAM param;
	char* buffer;
	
	FILE* fp = fopen(exefile, "rb");
	
	if ( fp == nullptr ) {
		std::cout << "ERROR: File not found." << std::endl;
		return -1;
	}
	
	if ( fread(&exe, 1, sizeof(PSEXE), fp) != sizeof(PSEXE) ) {
		std::cout << "ERROR: Read error or invalid file." << std::endl;
		fclose(fp);
		return -1;
	}
	
	if ( strcmp(exe.header, "PS-X EXE") ) {
		
		buffer = (char*)loadCPE(fp, &param.params);
		
		if ( buffer == nullptr ) {
			fclose(fp);
			return -1;
		}
		
	} else {
		
		buffer = (char*)malloc(exe.params.t_size);
	
		if ( fread(buffer, 1, exe.params.t_size, fp) != exe.params.t_size ) {
			
			std::cout << "ERROR: Incomplete file or read error occurred." << std::endl;
			free(buffer);
			fclose(fp);
			
			return -1;
			
		}
		
		memcpy(&param.params, &exe.params, sizeof(EXEPARAM));
		
	}
	
	fclose(fp);
	
	param.crc32 = crc32(buffer, param.params.t_size, CRC32_REMAINDER);
	
	serial->SetRate(115200);
	serial->SendBytes((void*)"MEXE", 4);
	
	char reply[4];
	int timeout = true;
	for( int i=0; i<10; i++ ) {
		if ( serial->ReceiveBytes(reply, 1) > 0 ) {
			timeout = false;
			break;
		}
		Sleep(10);
	}
	if ( timeout ) {
		std::cout << "ERROR: No response from console." << std::endl;
		return -1;
	}
	if ( reply[0] != 'K' ) {
		std::cout << "ERROR: No valid response from console." << std::endl;
		return -1;
	}
	
	serial->SendBytes(&param, sizeof(EXEPARAM));
	Sleep(20);
	serial->SendBytes(buffer, param.params.t_size);
	
	free(buffer);
	
	return 0;
	
}

int uploadBIN(const char* file, unsigned int addr, SerialClass* serial) {
	
	BINPARAM param;
	char* buffer;
	
	FILE* fp = fopen(file, "rb");
	
	if ( fp == nullptr ) {
		std::cout << "ERROR: File not found." << std::endl;
		return -1;
	}
	
	fseek(fp, 0, SEEK_END);
	param.size = ftell(fp);
	fseek(fp, 0, SEEK_SET);
	
	buffer = (char*)malloc(param.size);
	
	if ( fread(buffer, 1, param.size, fp) != param.size ) {
		std::cout << "ERROR: Read error occurred." << std::endl;
		free(buffer);
		fclose(fp);
		return -1;
	}
	
	fclose(fp);
	
	serial->SetRate(115200);
	serial->SendBytes((void*)"MBIN", 4);
	
	char reply[4];
	int timeout = true;
	
	for( int i=0; i<10; i++ ) {
		if ( serial->ReceiveBytes(reply, 1) > 0 ) {
			timeout = false;
			break;
		}
		Sleep(10);
	}
	
	if ( timeout ) {
		std::cout << "ERROR: No response from console." << std::endl;
		return -1;
	}
	
	param.addr = addr;
	param.crc32 = crc32(buffer, param.size, CRC32_REMAINDER);
	
	serial->SendBytes(&param, sizeof(BINPARAM));
	Sleep(20);
	serial->SendBytes(buffer, param.size);
	
	free(buffer);
	
	return 0;
	
}

int main(int argc, char** argv) {
	
	SerialClass serial;
	SiofsClass siofs;	
	
	int quit = false;
	
	std::cout << "Meido-Comms v" VERSION " by Lameguy64 (SIOFS Revision " << SIOFS_MAJOR 
				<< "." << SIOFS_MINOR << ")" << std::endl;
	std::cout << "2018 Meido-Tek Productions" << std::endl << std::endl;
	
	if ( argc >= 2 ) {
		
		if ( strcmp( "-h", argv[1] ) == 0 ) {
			
			std::cout << "Usage:" << std::endl;
			std::cout << "  mcomms [-dev <device>] [-baud <rate>] [-dir <path>] [-term] [-fsmsg]" <<
				std::endl << "  [up <file> <addr>] [run <exefile>]" << std::endl << std::endl;

			std::cout << "    -dev <device> - Specify serial port device (default: " SERIAL_DEFAULT ")." << std::endl;
			std::cout << "    -baud <rate>  - Specify serial baud rate (default: 115200)." << std::endl;
			std::cout << "                    Note: This does not affect the rate used for PS-EXE upload." << std::endl;
			std::cout << "    -dir <path>   - Specify initial host directory for SIOFS." << std::endl;
#ifdef __WIN32__
			std::cout << "    -term         - Enable terminal mode (forward keystrokes to serial)." << std::endl;
			std::cout << "    -hex          - Output received data in hex." << std::endl;
#endif
			std::cout << "    -fsmsg        - Enable SIOFS messages." << std::endl << std::endl;

			std::cout << "  LITELOAD Parameters (catflap inspired):" << std::endl;
			std::cout << "    up <file> <addr> - Upload a file to specified address." << std::endl;
			std::cout << "    run <file>       - Upload a PS-EXE file (CPE format is supported)." << std::endl << std::endl;
			std::cout << "    Specified memory address must be in hex." << std::endl << std::endl;

			std::cout << "  Defaults can be changed with environment variables:" << std::endl;
			std::cout << "    MC_DEVICE - Serial device." << std::endl;
			std::cout << "    MC_BAUD   - Baud rate." << std::endl;

			return EXIT_SUCCESS;
			
		}
	}
	
	// Search environment variables
	if ( getenv("MC_DEVICE") ) {
		serial_device = getenv("MC_DEVICE");
	}
	if ( getenv("MC_BAUD") ) {
		serial_baud = atoi(getenv("MC_BAUD"));
	}
	
	for ( int i=1; i<argc; i++ ) {
		
		if ( strcmp("-dev", argv[i]) == 0 ) {
			
			i++;
			if ( i >= argc ) {
				std::cout << "Missing device parameter." << std::endl;
				return EXIT_FAILURE;
			}
			serial_device = argv[i];
			
		} else if ( strcmp("-baud", argv[i]) == 0 ) {
			
			i++;
			if ( i >= argc ) {
				std::cout << "Missing baud rate parameter." << std::endl;
				return EXIT_FAILURE;
			}
			serial_baud = atoi(argv[i]);
			
		} else if ( strcmp("-dir", argv[i]) == 0 ) {
			
			i++;
			if ( i >= argc ) {
				std::cout << "Missing path parameter." << std::endl;
				return EXIT_FAILURE;
			}
			
			if ( chdir(argv[i]) ) {
				std::cout << "Unable to change to specified directory " 
					<< argv[i] << std::endl;
				return EXIT_FAILURE;
			}
#ifdef __WIN32__	
		} else if ( strcmp("-term", argv[i]) == 0 ) {
			
			terminal_mode = true;
			
		} else if ( strcmp("-hex", argv[i]) == 0 ) {
			
			hex_mode = true;
			
#endif
		} else if ( strcmp("-fsmsg", argv[i]) == 0 ) {
			
			fs_messages = true;
			
		} else if ( strcmp("run", argv[i]) == 0 ) {
			
			i++;
			if ( i >= argc ) {
				std::cout << "Missing filename parameter." << std::endl;
				return EXIT_FAILURE;
			}
			psexe_file = argv[i];
			break;
		
		} else if ( strcmp("up", argv[i]) == 0 ) {
			
			i++;
			if ( i >= argc ) {
				std::cout << "Missing filename parameter." << std::endl;
				return EXIT_FAILURE;
			}
			bin_file = argv[i];
			i++;
			if ( i >= argc ) {
				std::cout << "Missing address parameter." << std::endl;
				return EXIT_FAILURE;
			}
			sscanf(argv[i], "%x", &bin_addr);
			break;
			
		} else {
			
			std::cout << "Unknown parameter: " << argv[i] << std::endl;
			return EXIT_FAILURE;
			
		}
		
	}
	
	switch( serial.OpenPort(serial_device.c_str()) ) {
		case SerialClass::ERROR_OPENING:
			std::cout << "ERROR: Unable to open " << serial_device << "." << std::endl;
			return EXIT_FAILURE;
		case SerialClass::ERROR_CONFIG:
			std::cout << "ERROR: Unable to configure " << serial_device << "." << std::endl;
			return EXIT_FAILURE;
	}
	
	// Upload binary file if specified
	if ( !bin_file.empty() ) {
		
		std::cout << "Uploading binary file..." << std::endl;
		
		if ( uploadBIN(bin_file.c_str(), bin_addr, &serial) < 0 ) {
			return EXIT_FAILURE;
		}
		
		return EXIT_SUCCESS;
		
	}
	
	// Upload executable if specified
	if ( !psexe_file.empty() ) {
		
		std::cout << "Uploading executable..." << std::endl;
		
		if ( uploadEXE(psexe_file.c_str(), &serial) < 0 ) {
			return EXIT_FAILURE;
		}
		
	}
	
	
	// Set serial speed
	if ( serial.SetRate(serial_baud) != SerialClass::OK ) {
		std::cout << "ERROR: Unsupported baud rate specified for " << serial_device << "." << std::endl;
		return EXIT_FAILURE;
	}
	
	std::cout << "Listening " << serial_device << " at " << serial_baud << " baud." << std::endl;
	
#ifdef __WIN32__
	if ( !terminal_mode ) {
		std::cout << "Press ESC to quit..." << std::endl << std::endl;
	} else {
		std::cout << "Terminal Mode! Press F12 to quit..." << std::endl << std::endl;
	}
#endif
	
	unsigned char keypress[4] = {0};
	int keylen = 0;
	
	while( !quit ) {
		
		char buffer[256];
		memset(buffer, 0, 128);
		
		// Read characters
		keylen = 0;
		
#ifdef __WIN32__
		
		while( _kbhit() ) {
			keypress[keylen] = _getch();
			keylen++;
		}
		
		if ( keylen > 0 ) {
			
			if ( keypress[0] == 224 ) {
				if ( keypress[1] == 134 ) {
					quit = true;
				}
			}
			
			if ( terminal_mode ) {
				
				serial.SendBytes(keypress, keylen);
				
			} else {
				
				if ( keypress[0] == 27 ) {
					quit = true;
				}
				
			}
			
		}
		
#endif
		
		if ( serial.PendingBytes() ) {
			
			int len = serial.ReceiveBytes(buffer, 256);
			
			if ( len > 0 ) {

				if ( strchr(buffer, '~') ) {
				
					if ( siofs.Query(strchr(buffer, '~'), &serial) ) {
						
						*strchr(buffer, '~') = 0x0;
						len -= 4;
						
					}
					
				}

				if ( !hex_mode ) {
					
					std::cout << buffer;
				
				} else {
					
					std::cout << std::hex;
					
					for(int i=0; i<len; i++) {
						
						int val = *((unsigned char*)&buffer[i]);
						std::cout << std::setfill('0') << std::setw(2) << val << ",";
						
						if ( (i%4) == 3 ) {
							std::cout << std::endl;
						}
						
					}
					std::cout << std::dec << std::setw(0) << "\n--" << std::endl;
					
				}
				
			}
			
		}
		
#ifndef __WIN32__
		usleep(1000);
#endif
	
	}
	
	serial.ClosePort();
	
	return EXIT_SUCCESS;
	
}

