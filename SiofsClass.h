/* 
 * File:   SiofsClass.h
 * Author: Lameguy64
 *
 * Created on June 2, 2018, 9:07 PM
 */

#ifndef SIOFSCLASS_H
#define SIOFSCLASS_H

#include <stdio.h>
#include <dirent.h>
#include "SerialClass.h"

#define SIOFS_HANDLES	64
#define SIOFS_READ		0x1
#define SIOFS_WRITE		0x2
#define SIOFS_BINARY	0x4

#define SIOFS_MAJOR		1
#define SIOFS_MINOR		0

class SiofsClass {
public:
	
	SiofsClass();
	virtual ~SiofsClass();
	
	int Query(const char* cmd, SerialClass* comm);
	
private:
	
	typedef struct {
		unsigned short flags;
		unsigned short length;
		char filename[128];
	} SFS_OPENSTRUCT;
	
	typedef struct {
		unsigned short fd;
		unsigned short crc16;
		int length;
	} SFS_WRITESTRUCT;
	
	typedef struct {
		unsigned short fd;
		unsigned short pad;
		int length;
	} SFS_READSTRUCT;
	
	typedef struct {
		unsigned short ret;
		unsigned short crc16;
		unsigned int length;
	} SFS_READREPLY;
	
	typedef struct {
		unsigned short fd;
		unsigned short mode;
		unsigned int offs;
	} SFS_SEEKSTRUCT;
	
	typedef struct {
		int size;
		struct {
			unsigned int seconds:6;
			unsigned int minutes:6;
			unsigned int hours:4;
			unsigned int day:5;
			unsigned int month:4;
			unsigned int year:7;
		} date;
		unsigned short flags;
		unsigned short length;
	} SFS_DIRSTRUCT;
	
	typedef struct {
		int size;
		struct {
			unsigned int seconds:6;
			unsigned int minutes:6;
			unsigned int hours:4;
			unsigned int day:5;
			unsigned int month:4;
			unsigned int year:7;
		} date;
		unsigned short flags;
		unsigned short length;
		char filename[64];
	} SFS_DIRSTRUCT2;
	
	typedef struct {
		short num;
		short offset;
	} SFS_DIRPARAM;
	
	typedef struct {
		int size;
		struct {
			unsigned int seconds:6;
			unsigned int minutes:6;
			unsigned int hours:4;
			unsigned int day:5;
			unsigned int month:4;
			unsigned int year:7;
		} date;
		unsigned short flags;
		unsigned short pad;
	} SFS_STATSTRUCT;
	
	typedef struct {
		unsigned int length;
		unsigned int offset;
	} SFS_QREADSTRUCT;
	
	int TestHandle(int hnum);
	
	void FsInit();
	
	void FsOpen();
	void FsClose();
	void FsReadQuick();
	
	void FsWrite();
	void FsRead();
	void FsGets();
	
	void FsSeek();
	void FsTell();
	
	void FsDirFirst();
	void FsDirNext();
	void FsDirList();
	
	void FsStat();
	void FsChangeDir();
	void FsWorkDir();
	
	SerialClass*	serial;
	FILE*			handles[SIOFS_HANDLES];
	DIR*			hDir;
	char			dPattern[128];
};

#ifndef __WIN32__
void Sleep(int msec);
#endif

#endif /* SIOFSCLASS_H */

