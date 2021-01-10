#ifndef _UPLOAD_H
#define _UPLOAD_H

#include "serial.h"

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

void initTable32(unsigned int* table);
unsigned int crc32(void* buff, int bytes, unsigned int crc);

int uploadEXE( const char* exefile, SerialClass* serial );
int uploadBIN( const char* file, unsigned int addr, SerialClass* serial, int patch );

#endif // _UPLOAD_H
