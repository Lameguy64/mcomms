#ifdef __WIN32__
#include <windows.h>
#include <shlwapi.h>
#endif

#include <sys/stat.h>
#include <string.h>
#include <malloc.h>
#include <time.h>
#include <iostream>
#include <vector>
#include <unistd.h>
#include "serial.h"
#include "siofs.h"

int fs_messages = false;

#ifndef __WIN32__
void Sleep(int msec) {
	usleep(1000*msec);
}
#endif

static void initTable16(unsigned short* table) {

	int i, j;
    unsigned short crc, c;

    for (i=0; i<256; i++) {

        crc = 0;
        c   = (unsigned short) i;

        for (j=0; j<8; j++) {

            if ( (crc ^ c) & 0x0001 )
				crc = ( crc >> 1 ) ^ 0xA001;
            else
				crc =   crc >> 1;

            c = c >> 1;
        }

        table[i] = crc;
    }

}

static unsigned short crc16(void* buff, int bytes, unsigned short crc) {

	int i;
	unsigned short tmp, short_c;
	unsigned short crcTable[256];

	initTable16(crcTable);

	for(i=0; i<bytes; i++) {

		short_c = 0x00ff & (unsigned short)((unsigned char*)buff)[i];

		tmp =  crc       ^ short_c;
		crc = (crc >> 8) ^ crcTable[tmp&0xff];

	}

    return(crc);

}

static int testPattern(const char* name, const char* pattern) {
	
#ifdef __WIN32__
	const char *pos = pattern;
	char ext_temp[32];
	int c=0;
	
	while(1) {
		
		const char* npos = strchr(pos, ';');
		
		if ( npos == nullptr ) {
			if ( c == 0 ) {

				return PathMatchSpec(name, pattern);

			} else {
				
				return PathMatchSpec(name, pos);
				
			}
		}
		
		memset(ext_temp, 0x0, 32);
		strncpy(ext_temp, pos, (int)(npos-pos));
		
		if ( PathMatchSpec(name, ext_temp) ) {
			return 1;
		}
				
		pos = npos+1;
		c++;
		
	}
#endif
	
	return 0;
	
}

SiofsClass::SiofsClass() {
	
	memset(handles, 0x00, sizeof(FILE*)*SIOFS_HANDLES);
	hDir = nullptr;
	
}

SiofsClass::~SiofsClass() {
	
	for(int i=0; i<SIOFS_HANDLES; i++) {
		if ( handles[i] ) {
			fclose(handles[i]);
		}
	}
	
	if ( hDir ) {
		closedir(hDir);
	}
	
}

int SiofsClass::Query(const char* cmd, SerialClass* comm) {
	
	serial = comm;
	
	// File open
	if ( strcmp(cmd, "~FRS") == 0 ) {
		
		if ( fs_messages ) {
			printf( "FS: Reset/init.\n" );
		}
		
		FsInit();
		return 1;
		
	} else if ( strcmp(cmd, "~FOP") == 0 ) {
		
		if ( fs_messages ) {
			printf( "FS: File open.\n" );
		}
		
		FsOpen();
		return 1;

	// File close
	} else if ( strcmp(cmd, "~FCL") == 0 ) {

		if ( fs_messages ) {
			printf( "FS: File close.\n" );
		}
		
		FsClose();
		return 1;

	// File quick read 
	} else if ( strcmp(cmd, "~FRQ") == 0 ) {
		
		if ( fs_messages ) {
			printf( "FS: File read quick.\n" );
		}
		
		FsReadQuick();
		return 1;
		
	// File write
	} else if ( strcmp(cmd, "~FWR") == 0 ) {

		if ( fs_messages ) {
			printf( "FS: File write.\n" );
		}
		
		FsWrite();
		return 1;

	// File read
	} else if ( strcmp(cmd, "~FRD") == 0 ) {

		if ( fs_messages ) {
			printf( "FS: File read.\n" );
		}
		
		FsRead();
		return 1;
		
	// File get string
	} else if ( strcmp(cmd, "~FGS") == 0 ) {

		if ( fs_messages ) {
			printf( "FS: File gets.\n" );
		}
		
		FsGets();
		return 1;

	// File seek
	} else if ( strcmp(cmd, "~FSK") == 0 ) {

		if ( fs_messages ) {
			printf( "FS: File seek.\n" );
		}
		
		FsSeek();
		return 1;

	// File tell
	} else if ( strcmp(cmd, "~FTL") == 0 ) {
		
		if ( fs_messages ) {
			printf( "FS: File tell.\n" );
		}
		
		FsTell();
		return 1;
	
	// Find first file
	} else if ( strcmp(cmd, "~FLF") == 0 ) {
		
		if ( fs_messages ) {
			printf( "FS: File list first.\n" );
		}
		
		FsDirFirst();
		return 1;
		
	// Find next file
	} else if ( strcmp(cmd, "~FLN") == 0 ) {
		
		if ( fs_messages ) {
			printf( "FS: File list next.\n" );
		}
	
		FsDirNext();
		return 1;
	
	// File list directory
	} else if ( strcmp(cmd, "~FLS") == 0 ) {
		
		if ( fs_messages ) {
			printf( "FS: File list.\n" );
		}
		
		FsDirList();
		return 1;
	
	// File stat
	} else if ( strcmp(cmd, "~FST") == 0 ) {
		
		if ( fs_messages ) {
			printf( "FS: File stat.\n" );
		}
		
		FsStat();
		return 1;
		
	} else if ( strcmp(cmd, "~FCD") == 0 ) {
		
		if ( fs_messages ) {
			printf( "FS: File change directory.\n" );
		}
		
		FsChangeDir();
		return 1;
		
	} else if ( strcmp(cmd, "~FWD") == 0 ) {
		
		if ( fs_messages ) {
			printf( "FS: File working directory.\n" );
		}
		
		FsWorkDir();
		return 1;
		
	}
	
	return 0;
	
}

int SiofsClass::TestHandle(int hnum) {
	
	if ( ( hnum < 0 ) || ( hnum >= SIOFS_HANDLES ) ) {
		return 2;
	}
	
	if ( handles[hnum] == nullptr ) {
		return 1;
	}
	
	return 0;
	
}

void SiofsClass::FsInit() {
	
	int ver;
	
	ver = SIOFS_MINOR|(SIOFS_MAJOR<<8);
	
	serial->SendBytes(&ver, 2);
	
	for(int i=0; i<SIOFS_HANDLES; i++) {
		if ( handles[i] ) {
			fclose( handles[i] );
			handles[i] = nullptr;
		}
	}
	
	if ( hDir ) {
		closedir(hDir);
		hDir = nullptr;
	}
	
}

void SiofsClass::FsOpen() {
	
	SFS_OPENSTRUCT file;
	char fparam[4];
	
	// Send accept character
	serial->SendBytes((void*)"K", 1);
	
	// Receive flags and file name length
	if ( serial->ReceiveBytes(&file, 4) != 4 ) {
		if ( fs_messages ) {
			printf( "FS: Timeout.\n" );
		}
		return;
	}
	
	// Receive file name
	memset(file.filename, 0x0, 64);
	serial->ReceiveBytes(file.filename, file.length);
	
	if ( fs_messages ){
		printf( "FS: File = %s\n", file.filename );
	}
	
	// Search for a vacant handle
	int hnum = -1;
	for(int i=0; i<SIOFS_HANDLES; i++) {
		if ( handles[i] == nullptr ) {
			hnum = i;
			break;
		}
	}
	
	if ( hnum < 0 ) {	// No vacant handles found
		fparam[0] = -2;
		serial->SendBytes(fparam, 1);
		return;
	}
	
	memset(fparam, 0x0, 4);
	
	if ( file.flags & SIOFS_READ ) {
		strcat(fparam, "r");
	}
	if ( file.flags & SIOFS_WRITE ) {
		strcat(fparam, "w");
	}
	if ( file.flags & SIOFS_BINARY ) {
		strcat(fparam, "b");
	}
	
	if ( fs_messages ) {
		printf( "FS: mode = %d\n", fparam );
	}
	
	// File open
	FILE* fp = fopen(file.filename, fparam);
	
	if ( !fp ) {
		
		if ( fs_messages ) {
			printf( "FS: ERROR - Cannot open file." );
		}
		
		fparam[0] = -1;
		serial->SendBytes(fparam, 1);
		return;
		
	}
	
	// Set and send file handle number
	if ( fs_messages ) {
		printf( "FS: Handle = %s\n", hnum );
	}
	
	handles[hnum] = fp;
	serial->SendBytes(&hnum, 1);
	
}

void SiofsClass::FsReadQuick() {
	
	int ret;
	int len;
	char filename[128];
	SFS_QREADSTRUCT param;
	
	ret = 0;
	memset(filename, 0, 128);
	
	// Send accept character
	serial->SendBytes((void*)"K", 1);
	
	// Receive file name
	serial->ReceiveBytes(&ret, 1);
	serial->ReceiveBytes(filename, ret);
	
	if ( fs_messages ) {
		printf( "FS: Filename = %s\n", filename );
	}
	
	// Open requested file
	FILE* fp = fopen(filename, "rb");
	
	// Send response code
	if ( fp == nullptr ) {
		ret = 1;
	} else {
		ret = 0;
	}
	
	serial->SendBytes(&ret, 1);
	
	if ( ret > 0 ) {
		return;
	}
	
	if ( serial->ReceiveBytes(&param, sizeof(SFS_READSTRUCT)) 
		!= sizeof(SFS_READSTRUCT) ) {
		
		if ( fs_messages ) {
			printf( "FS: Timeout.\n" );
		}
		
		return;
		
	}
	
	printf( "FS: Length = %d\n", param.length );
	printf( "FS: Offset = %d\n", param.offset );
	
	if ( fseek(fp, param.offset, SEEK_SET) ) {
		ret = 2;
		serial->SendBytes(&ret, 2);
		ret = 0;
		serial->SendBytes(&ret, 2);
		fclose(fp);
		return;
	}
	
	char* buffer = (char*)malloc(param.length);
	len = fread(buffer, 1, param.length, fp);
	
	if ( len == 0 ) {
		ret = 1;
		serial->SendBytes(&ret, 2);
		ret = 0;
		serial->SendBytes(&ret, 2);
		free(buffer);
		fclose(fp);
		return;
	}
	
	ret = 0;
	serial->SendBytes(&ret, 2);
	ret = crc16(buffer, len, 0);
	serial->SendBytes(&ret, 2);
	serial->SendBytes(&len, 4);
	
	if ( serial->ReceiveBytes(&ret, 1) != 1 ) {
		
		if ( fs_messages ) {
			printf( "FS: Timeout.\n" );
			return;
		}
		
		free(buffer);
		fclose(fp);
		return;
	}
	
	while( 1 ) {
		
		serial->SendBytes(buffer, len);
		
		if ( serial->ReceiveBytes(&ret, 2) != 2 ) {
		
			if ( fs_messages ) {
				printf( "FS: Timeout H.\n" );
				return;
			}

			free(buffer);
			fclose(fp);
			return;
		
		}
		
		if ( ret == 0 ) {
			break;
		}
		
	}
	
	free(buffer);
	fclose(fp);
	
}

void SiofsClass::FsClose() {
	
	short hnum=0;
	int ret;
	
	// Send accept character
	serial->SendBytes((void*)"K", 1);
	
	if ( serial->ReceiveBytes(&hnum, 1) != 1 ) {
		if ( fs_messages ) {
			printf( "FS: Timeout.\n" );
		}
		return;
	}
	
	if ( fs_messages ) {
		printf( "FS: File handle = %d\n", hnum );
	}
	
	ret = TestHandle(hnum);
	
	if ( ret == 1 ) {
		
		hnum = 1;
		serial->SendBytes(&hnum, 1);
		return;
		
	} else if ( ret == 2 ) {
		
		hnum = 2;
		serial->SendBytes(&hnum, 1);
		return;
		
	}
	
	fclose(handles[hnum]);
	handles[hnum] = nullptr;
	
	hnum = 0;
	serial->SendBytes(&hnum, 1);
	
}

void SiofsClass::FsWrite() {
	
	SFS_WRITESTRUCT info;
	
	// Send accept character
	serial->SendBytes((void*)"K", 1);
	
	if ( serial->ReceiveBytes(&info, sizeof(SFS_WRITESTRUCT)) != sizeof(SFS_WRITESTRUCT) ) {
		if ( fs_messages ) {
			printf( "FS: Timeout.\n" );
		}
		return;
	}
	
	if ( fs_messages ) {
		printf( "FS: Handle = %d\n", info.fd );
		printf( "FS: Chksum = %04x\n", info.crc16 );
		printf( "FS: Length = %d\n", info.length );
	}
	int ret = TestHandle(info.fd);
	
	serial->SendBytes(&ret, 1);
		
	if ( ret ) {
		return;
	}
	
	char* buffer = (char*)malloc(info.length);
	
	while(1) {
		
		int received = 0;

		while( received < info.length ) {

			int s = info.length-received;

			if ( s > 8 ) {
				s = 8;
			}

			ret = serial->ReceiveBytes(buffer+received, s);

			if ( ret <= 0 ) {
				if ( fs_messages ) {
					printf( "FS: Timeout.\n" );
				}
				return;
			}

			received += ret;

		}
		
		// Check if received data is complete
		if ( received < info.length ) {
			ret = -3;
			if ( fs_messages ) {
				printf( "FS: Data incomplete. Retrying.\n" );
			}
			serial->SendBytes(&ret, 4);
			continue;
		}

		// Checksum
		if ( crc16(buffer, info.length, 0) != info.crc16 ) {
			ret = -2;
			if ( fs_messages ) {
				printf( "FS: CRC mismatch. Retrying.\n" );
			}
			serial->SendBytes(&ret, 4);
			continue;
		}
		
		break;
		
	}
	
	printf( "FS: Wrote %d bytes.\n", info.length );
	
	ret = fwrite(buffer, 1, info.length, handles[info.fd]);
	free(buffer);
	
	if ( ret == 0 ) {
		ret = -1;
	}
	
	serial->SendBytes(&ret, 4);
	
}

void SiofsClass::FsRead() {
	
	SFS_READSTRUCT info;
	SFS_READREPLY response;
	char* buffer;
	
	serial->SendBytes((void*)"K", 1);
	
	if ( serial->ReceiveBytes(&info, sizeof(SFS_READSTRUCT)) != sizeof(SFS_READSTRUCT) ) {
		if ( fs_messages ) {
			printf( "FS: Timeout.\n" );
		}
		return;
	}
	
	if ( fs_messages ) {
		printf( "FS: Handle = %d\n", info.fd );
		printf( "FS: Length = %d\n", info.length );
	}
	
	int ret = TestHandle(info.fd);
	
	if ( ret ) {
		response.ret = ret;
		response.crc16 = 0;
		response.length = 0;
		serial->SendBytes(&ret, sizeof(SFS_READREPLY));
		return;
	}
	
	response.ret = 0;
	
	buffer = (char*)malloc(info.length);
	int read = fread(buffer, 1, info.length, handles[info.fd]);
			
	if ( feof(handles[info.fd]) ) {
		response.ret = 4;
	}
	
	if ( read > 0 ) {
		response.crc16 = crc16(buffer, read, 0);
		response.length = read;
	} else {
		response.crc16 = 0;
		response.length = 0;
	}
	
	serial->SendBytes(&response, sizeof(SFS_READREPLY));
	
	if ( read == 0 ) {
		return;
	}
	
	ret = 0;
	if ( serial->ReceiveBytes(&ret, 1) != 1 ) {
		if ( fs_messages ) {
			printf( "FS: Timeout.\n" );
		}
		free(buffer);
		return;
	}
	
	while(1) {
		
		serial->SendBytes(buffer, response.length);
		
		ret = 0;
		if ( serial->ReceiveBytes(&ret, 1) != 1 ) {
			if ( fs_messages ) {
				printf( "FS: Timeout.\n" );
			}
			free(buffer);
			return;
		}
		
		if ( ret == 1 ) {
			if ( fs_messages ) {
				printf( "FS: Data incomplete on client. Retrying.\n" );
			}
			continue;
		}
		
		if ( ret == 2 ) {
			if ( fs_messages ) {
				printf( "FS: CRC16 mismatch on client. Retrying.\n" );
			}
			continue;
		}
		
		break;
	
	}
	
	free(buffer);
	
}

void SiofsClass::FsGets() {
	
	SFS_READSTRUCT info;
	SFS_READREPLY response;
	char* buffer;
	
	serial->SendBytes((void*)"K", 1);
	
	if ( serial->ReceiveBytes(&info, sizeof(SFS_READSTRUCT)) != sizeof(SFS_READSTRUCT) ) {
		if ( fs_messages ) {
			printf( "FS: Timeout.\n" );
		}
		return;
	}
	
	if ( fs_messages ) {
		printf( "FS: Handle = %d\n", info.fd );
		printf( "FS: Length = %d\n", info.length );
	}
	
	int ret = TestHandle(info.fd);
	
	if ( ret ) {
		response.ret = ret;
		response.crc16 = 0;
		response.length = 0;
		serial->SendBytes(&ret, sizeof(SFS_READREPLY));
		return;
	}
	
	response.ret = 0;
	
	buffer = (char*)malloc(info.length);
	
	int read = 0;
	if ( fgets(buffer, info.length, handles[info.fd]) ) {
		read = strlen(buffer)+1;
	}
			
	if ( feof(handles[info.fd]) ) {
		response.ret = 4;
	}
	
	if ( read > 0 ) {
		response.crc16 = crc16(buffer, read, 0);
		response.length = read;
	} else {
		response.crc16 = 0;
		response.length = 0;
	}
	
	serial->SendBytes(&response, sizeof(SFS_READREPLY));
	
	if ( read == 0 ) {
		return;
	}
	
	ret = 0;
	if ( serial->ReceiveBytes(&ret, 1) != 1 ) {
		if ( fs_messages ) {
			printf( "FS: Timeout.\n" );
		}
		free(buffer);
		return;
	}
	
	while(1) {
		
		serial->SendBytes(buffer, response.length);
		
		ret = 0;
		if ( serial->ReceiveBytes(&ret, 1) != 1 ) {
			if ( fs_messages ) {
				printf( "FS: Timeout.\n" );
			}
			free(buffer);
			return;
		}
		
		if ( ret == 1 ) {
			if ( fs_messages ) {
				printf( "FS: Data incomplete on client. Retrying.\n" );
			}
			continue;
		}
		
		if ( ret == 2 ) {
			if ( fs_messages ) {
				printf( "FS: CRC16 mismatch on client. Retrying.\n" );
			}
			continue;
		}
		
		break;
	
	}
	
	free(buffer);
	
}

void SiofsClass::FsSeek() {
	
	SFS_SEEKSTRUCT info;
	
	serial->SendBytes((void*)"K", 1);
	
	if ( serial->ReceiveBytes(&info, sizeof(SFS_SEEKSTRUCT)) != sizeof(SFS_SEEKSTRUCT) ) {
		if ( fs_messages ) {
			printf( "FS: Timeout.\n" );
		}
		return;
	}
	
	int ret = TestHandle(info.fd);
	
	if ( ret ) {
		serial->SendBytes(&ret, 1);
		return;
	}
	
	if ( fs_messages ) {
		printf( "FS: Handle = %d\n", info.fd );
		printf( "FS: Pos    = %d\n", info.offs );
		printf( "FS: Mode   = %d\n", info.mode);
	}
	
	int mode = SEEK_SET;
	switch(info.mode) {
	case 0:
		mode = SEEK_SET;
		break;
	case 1:
		mode = SEEK_CUR;
		break;
	case 2:
		mode = SEEK_END;
		break;
	}
	
	if ( fseek(handles[info.fd], info.offs, mode) ) {
		ret = 3;
		serial->SendBytes(&ret, 1);
		return;
	}
	
	ret = 0;
	serial->SendBytes(&ret, 1);
	
}

void SiofsClass::FsTell() {
	
	int hnum;
	int ret;
	
	// Send accept character
	serial->SendBytes((void*)"K", 1);
	
	hnum = 0;
	if ( serial->ReceiveBytes(&hnum, 1) != 1 ) {
		if ( fs_messages ) {
			printf( "FS: Timeout.\n" );
		}
		return;
	}
	
	if ( fs_messages ) {
		printf( "FS: File handle = %d\n", hnum );
	}
	
	ret = TestHandle(hnum);
	
	if ( ret == 1 ) {
		
		hnum = -1;
		serial->SendBytes(&hnum, 4);
		return;
		
	} else if ( ret == 2 ) {
		
		hnum = -2;
		serial->SendBytes(&hnum, 4);
		return;
		
	}
	
	int pos = ftell(handles[hnum]);
	serial->SendBytes(&pos, 4);
	
}

void SiofsClass::FsDirFirst() {
	
	unsigned char length;
	int ret;
	
	struct dirent*	dir;
	struct stat		attr;
	
	SFS_DIRSTRUCT entry;
	
	// Send accept character
	serial->SendBytes((void*)"K", 1);
	
	// Receive file name length
	if ( serial->ReceiveBytes(&length, 1) != 1 ) {
		if ( fs_messages ) {
			printf( "FS: Timeout.\n" );
		}
		return;
	}
	memset(&entry, 0x0, sizeof(SFS_DIRSTRUCT));
	
	// Receive file name
	memset(dPattern, 0x0, 128);
	serial->ReceiveBytes(dPattern, length);
	
	if ( fs_messages ) {
		printf( "FS: Wildcard = %s\n", dPattern );
	}
	
	if ( hDir ) {
		closedir(hDir);
	}
	
	hDir = opendir(".");

	if (hDir == nullptr) {
		if ( fs_messages ) {
			printf( "FS: ERROR: Cannot open directory.\n" );
		}
		entry.size = -1;
		serial->SendBytes(&entry, sizeof(SFS_DIRSTRUCT));
		return;
	}

	do {
		
		dir = readdir(hDir);
		
		if ( dir == nullptr ) {
			closedir(hDir);
			hDir = nullptr;
			entry.size = -2;
			serial->SendBytes(&entry, sizeof(SFS_DIRSTRUCT));
			return;
		}
		
	} while( (dir->d_name[0] == '.' && ( strlen(dir->d_name) == 1 )) );
	
	while ( dir != nullptr ) {
		
		stat(dir->d_name, &attr);
		
		if ( S_ISDIR(attr.st_mode) ) {
			break;
		}
		
		if ( !testPattern(dir->d_name, dPattern) ) {
			dir = readdir(hDir);
		}
		
		break;
		
	}
	
	if ( dir == nullptr ) {
		closedir(hDir);
		hDir = nullptr;
		entry.size = -2;
		serial->SendBytes(&entry, sizeof(SFS_DIRSTRUCT));
		return;
	}

	entry.flags = 0;
	if ( S_ISDIR(attr.st_mode) ) {
		entry.flags = 1;
	}
	
	entry.size = attr.st_size;
	entry.length = strlen(dir->d_name);
	
	tm* t = gmtime(&attr.st_mtime);
	
	entry.date.seconds	= t->tm_sec;
	entry.date.minutes	= t->tm_min;
	entry.date.hours	= t->tm_hour;
	entry.date.day		= t->tm_mday;
	entry.date.month	= t->tm_mon+1;
	entry.date.year		= (int)(t->tm_year-80);
	
	serial->SendBytes(&entry, sizeof(SFS_DIRSTRUCT));
	
	ret = 0;
	if ( serial->ReceiveBytes(&ret, 1) != 1 ) {
		if ( fs_messages ) {
			printf( "FS: Timeout.\n" );
		}
		return;
	}
	if ( ret != 'K' ) {
		if ( fs_messages ) {
			printf( "FS: Timeout.\n" );
		}
		return;
	}
	
	serial->SendBytes(dir->d_name, entry.length);
	
}

void SiofsClass::FsDirNext() {
	
	int ret;
	
	struct dirent*	dir;
	struct stat		attr;
	
	SFS_DIRSTRUCT entry;
	
	if (hDir == nullptr) {
		if ( fs_messages ) {
			printf( "FS: ERROR: No directory open.\n" );
		}
		entry.size = -1;
		serial->SendBytes(&entry, sizeof(SFS_DIRSTRUCT));
		return;
	}
		
	dir = readdir(hDir);
		
	if ( dir == nullptr ) {
		
		closedir(hDir);
		hDir = nullptr;
		entry.size = -2;
		serial->SendBytes(&entry, sizeof(SFS_DIRSTRUCT));
		
		return;
	}
	
	while ( dir != nullptr ) {
		
		stat(dir->d_name, &attr);
		
		if ( S_ISDIR(attr.st_mode) ) {
			break;
		}
		
		if ( !testPattern(dir->d_name, dPattern) ) {
			dir = readdir(hDir);
		}
		
		break;
		
	}
	
	if ( dir == nullptr ) {
		closedir(hDir);
		hDir = nullptr;
		entry.size = 0;
		serial->SendBytes(&entry, sizeof(SFS_DIRSTRUCT));
		return;
	}

	entry.flags = 0;
	if (S_ISDIR(attr.st_mode)) {
		entry.flags = 1;
	}
	
	entry.size = attr.st_size;
	entry.length = strlen(dir->d_name);
	
	tm* t = gmtime(&attr.st_mtime);
	
	entry.date.seconds	= t->tm_sec;
	entry.date.minutes	= t->tm_min;
	entry.date.hours	= t->tm_hour;
	entry.date.day		= t->tm_mday;
	entry.date.month	= t->tm_mon+1;
	entry.date.year		= (int)(t->tm_year-80);
	
	serial->SendBytes(&entry, sizeof(SFS_DIRSTRUCT));
	
	ret = 0;
	if ( serial->ReceiveBytes(&ret, 1) != 1 ) {
		if ( fs_messages ) {
			printf( "FS: Timeout.\n" );
		}
		return;
	}
	if ( ret != 'K' ) {
		if ( fs_messages ) {
			printf( "FS: Timeout.\n" );
		}
		return;
	}
	serial->SendBytes(dir->d_name, entry.length);
	
}

void SiofsClass::FsDirList() {
	
	char wildcard[128];
	unsigned char length;
	int ret;
	
	struct dirent*	dir;
	struct stat		attr;
	
	SFS_DIRPARAM param,param2;
	SFS_DIRSTRUCT2 entry;
	
	memset(wildcard, 0x0, 128);
	
	// Send accept character
	serial->SendBytes((void*)"K", 1);
	
	// Receive parameters
	if ( serial->ReceiveBytes(&param, sizeof(SFS_DIRPARAM)) != sizeof(SFS_DIRPARAM) ) {
		if ( fs_messages ) {
			printf( "FS: Timeout.\n" );
		}
		return;
	}
	
	// Receive wildcard length
	if ( serial->ReceiveBytes(&length, 1) != 1 ) {
		if ( fs_messages ) {
			printf( "FS: Timeout.\n" );
		}
		return;
	}
	
	// Receive wildcard string
	serial->ReceiveBytes(wildcard, length);
	
	if ( fs_messages ) {
		printf( "FS: items    = %d\n", param.num );
		printf( "FS: offset   = %d\n", param.offset );
		printf( "FS: Wildcard = %s\n", wildcard );
	}
	
	// Close dir handle if opened previously
	if ( hDir ) {
		closedir(hDir);
	}
	
	memset(&entry, 0x0, sizeof(SFS_DIRSTRUCT2));
	
	// Open directory
	hDir = opendir(".");

	// If unable to open directory
	if (hDir == nullptr) {
		
		if ( fs_messages ) {
			printf( "FS: ERROR: Cannot open directory.\n" );
		}
		
		param2.num = -1;
		param2.offset = 0;
		serial->SendBytes(&param2, sizeof(SFS_DIRPARAM));
		
		return;
	}

	std::vector<SFS_DIRSTRUCT2> dir_entries;
	
	do {
		
		dir = readdir(hDir);
		
		if ( dir == nullptr ) {
			
			closedir(hDir);
			hDir = nullptr;
			
			param2.num = -2;
			param2.offset = 0;
			serial->SendBytes(&entry, sizeof(SFS_DIRPARAM));
			
			return;
		}
		
	} while( (dir->d_name[0] == '.') && (strlen(dir->d_name) == 1) );
	
	int item_num = 0;
	
	while( dir != nullptr ) {
		
		stat(dir->d_name, &attr);

		entry.flags = 0;
		if (S_ISDIR(attr.st_mode)) {
			
			entry.flags = 1;
			
		} else {
			
			if ( !testPattern(dir->d_name, wildcard) ) {
				dir = readdir(hDir);
				continue;
			}
			
		}

		entry.size = attr.st_size;
		entry.length = strlen(dir->d_name);

		tm* t = gmtime(&attr.st_mtime);

		entry.date.seconds	= t->tm_sec;
		entry.date.minutes	= t->tm_min;
		entry.date.hours	= t->tm_hour;
		entry.date.day		= t->tm_mday;
		entry.date.month	= t->tm_mon+1;
		entry.date.year		= (int)(t->tm_year-80);

		memset(entry.filename, 0x0, 64);
		strcpy(entry.filename, dir->d_name);
		
		if ( ( item_num >= param.offset ) && 
		( item_num < ( ( param.offset+param.num ) ) ) ) {

			dir_entries.push_back(entry);

		}
		
		item_num++;
		
		dir = readdir(hDir);
		
	}
	
	closedir(hDir);
	
	param2.num = dir_entries.size();
	param2.offset = item_num;
	serial->SendBytes(&param2, sizeof(SFS_DIRPARAM));
	
	if ( param2.num > 0 ) {
		
		ret = 0;
		if ( serial->ReceiveBytes(&ret, 1) != 1 ) {
			if ( fs_messages ) {
				printf( "FS: Timeout.\n" );
			}
			return;
		}
		
		if ( ret != 'K' ) {
			if ( fs_messages ) {
				printf( "FS: Timeout.\n" );
			}
			return;
		}

		for(int i=0; i<dir_entries.size(); i++) {

			serial->SendBytes(&dir_entries[i], sizeof(SFS_DIRSTRUCT2));

		}
		
	}
	
}

void SiofsClass::FsStat() {
	
	int ret;
	char filename[64];
	struct stat	attr;
	SFS_STATSTRUCT st;
	
	// Send accept character
	serial->SendBytes((void*)"K", 1);
	
	// Receive file name length
	ret = 0;
	if ( serial->ReceiveBytes(&ret, 1) != 1 ) {
		if ( fs_messages ) {
			printf( "FS: Timeout.\n" );
		}
		return;
	}
	
	// Receive file name
	memset(filename, 0x0, 64);
	if ( serial->ReceiveBytes(filename, ret) != ret ) {
		if ( fs_messages ) {
			printf( "FS: Timeout.\n" );
		}
		return;
	}
	
	if ( fs_messages ) {
		printf( "FS: File = %s\n", filename );
	}
	
	memset(&st, 0, sizeof(SFS_STATSTRUCT));
	
	if ( stat(filename, &attr) < 0 ) {
		if ( fs_messages ) {
			printf( "FS: ERROR: File not found.\n" );
		}
		st.size = -1;
		serial->SendBytes(&st, 10);
		return;
	}
	
	st.size = attr.st_size;
	
	if ( S_ISDIR(attr.st_mode) ) {
		st.flags = 1;
	}
	
	tm* t = gmtime(&attr.st_mtime);

	st.date.seconds	= t->tm_sec;
	st.date.minutes	= t->tm_min;
	st.date.hours	= t->tm_hour;
	st.date.day		= t->tm_mday;
	st.date.month	= t->tm_mon+1;
	st.date.year	= (int)(t->tm_year-80);
	
	serial->SendBytes(&st, 10);
	
}

void SiofsClass::FsChangeDir() {
	
	int ret;
	char path[128];
	
	// Send accept character
	serial->SendBytes((void*)"K", 1);
	
	// Receive file name length
	ret = 0;
	if ( serial->ReceiveBytes(&ret, 1) != 1 ) {
		if ( fs_messages ) {
			printf( "FS: Timeout.\n" );
		}
		return;
	}
	
	// Receive file name
	memset(path, 0x0, 128);
	if ( serial->ReceiveBytes(path, ret) != ret ) {
		if ( fs_messages ) {
			printf( "FS: Timeout.\n" );
		}
		return;
	}
	
	if ( fs_messages ) {
		printf( "FS: Path = %s\n", path );
	}
	
	if ( chdir(path) ) {
		ret = 1;
	} else {
		ret = 0;
	}
	
	serial->SendBytes((void*)&ret, 1);
	
}

void SiofsClass::FsWorkDir() {
	
	int ret;
	char workdir[256];
	
	getcwd(workdir, 256);
	
	ret = strlen(workdir);
	serial->SendBytes((void*)&ret, 1);
	
	if ( ret > 0 ) {
		Sleep(20);
		serial->SendBytes(workdir, ret);
	}
	
}