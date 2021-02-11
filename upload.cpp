#include <stdio.h>
#include <string.h>
#include <vector>
#include "upload.h"
#include "siofs.h"

/* main.c */
extern int old_protocol;

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

#define	MAX_prg_entry_count	128

#pragma pack(push, 1)

typedef struct {

	unsigned int magic;				// 0-3
	unsigned char word_size;		// 4
	unsigned char endianness;		// 5
	unsigned char elf_version;		// 6
	unsigned char os_abi;			// 7
	unsigned int unused[2];			// 8-15

	unsigned short type;			// 16-17
	unsigned short instr_set;		// 18-19
	unsigned int elf_version2;		// 20-23

	unsigned int prg_entry_addr;	// 24-27
	unsigned int prg_head_pos;		// 28-31
	unsigned int sec_head_pos;		// 32-35
	unsigned int flags;				// 36-39
	unsigned short head_size;		// 40-41
	unsigned short prg_entry_size;	// 42-23
	unsigned short prg_entry_count;	// 44-45
	unsigned short sec_entry_size;	// 46-47
	unsigned short sec_entry_count;	// 48-49
	unsigned short sec_names_index;	// 50-51

} ELF_HEADER;

typedef struct {
	unsigned int seg_type;
	unsigned int p_offset;
	unsigned int p_vaddr;
	unsigned int undefined;
	unsigned int p_filesz;
	unsigned int p_memsz;
	unsigned int flags;
	unsigned int alignment;
} PRG_HEADER;

#pragma pack(pop)

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

void* loadELF(FILE* fp, EXEC* param)
{
	ELF_HEADER head;
	PRG_HEADER prg_heads[MAX_prg_entry_count];
	
	unsigned int exe_taddr = 0xffffffff;
	unsigned int exe_haddr = 0;
	unsigned int exe_tsize = 0;
	unsigned char* binary;
	
	fseek(fp, 0, SEEK_SET);
	fread(&head, 1, sizeof(head), fp);

	// Check header
	if( head.magic != 0x464c457f ) {
		
		printf("File is neither a PS-EXE, CPE or ELF binary.\n");
		return NULL;
		
	}

	if( head.type != 2 ) {
		
		printf("Only executable ELF files are supported.\n");
		return NULL;
		
	}

	if( head.instr_set != 8 ) {
		
		printf("ELF file is not a MIPS binary.\n");
		return NULL;
		
	}

	if( head.word_size != 1 ) {
		
		printf("Only 32-bit ELF files are supported.\n");
		return NULL;
		
	}

	if( head.endianness != 1 ) {
		
		printf("Only little endian ELF files are supported.\n");
		return NULL;
		
	}

	// Load program headers and determine binary size and load address

	fseek(fp, head.prg_head_pos, SEEK_SET);
	for( int i=0; i<head.prg_entry_count; i++ ) {

		fread( &prg_heads[i], 1, sizeof(PRG_HEADER), fp );

		if( prg_heads[i].flags == 4 ) {
			continue;
		}

		if( prg_heads[i].p_vaddr < exe_taddr ) {
			exe_taddr = prg_heads[i].p_vaddr;
		}

		if( prg_heads[i].p_vaddr > exe_haddr ) {
			exe_haddr = prg_heads[i].p_vaddr;
		}

	}

	exe_tsize = (exe_haddr-exe_taddr);
	exe_tsize += prg_heads[head.prg_entry_count-1].p_filesz;
	
	// Check if load address is appropriate in main RAM locations
	if( ( ( exe_taddr>>24 ) == 0x0 ) || ( ( exe_taddr>>24 ) == 0x80 ) ||
		( ( exe_taddr>>24 ) == 0xA0 ) ) {

		if( ( exe_taddr&0x00ffffff ) < 65536 ) {

			printf( "WARNING: Program text address overlaps kernel area!\n" );

		}

	}


	// Pad out the size to multiples of 2KB
	exe_tsize = 2048*((exe_tsize+2047)/2048);

	// Load the binary data
	binary = (unsigned char*)malloc( exe_tsize );
	memset( binary, 0x0, exe_tsize );

	for( int i=0; i<head.prg_entry_count; i++ ) {

		if( prg_heads[i].flags == 4 ) {
			continue;
		}

		fseek( fp, prg_heads[i].p_offset, SEEK_SET );
		fread( &binary[(int)(prg_heads[i].p_vaddr-exe_taddr)],
			1, prg_heads[i].p_filesz, fp );

	}
	
	memset(param, 0, sizeof(EXEC));
	param->pc0 = head.prg_entry_addr;
	param->t_addr = exe_taddr;
	param->t_size = exe_tsize;
	
	return binary;
	
}

void *loadCPE(FILE* fp, EXEC* param)
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
			// If not CPE, try loading it as ELF
			buffer = (char*)loadELF(fp, &param.params);
			
			if( buffer == NULL )
			{
				printf( "ERROR: Executable file is neither a PS-EXE, CPE or ELF.\n" );
				fclose(fp);
				return -1;
			}
			
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
	
	/* draw the progress bar */
	printf(" ");
	for( int i=0; i<50; i++ )
	{
		printf(".");
	}
	
	printf("]\r[");
	fflush(stdout);
	
	int progress,last_progress = 0;
	int remain = param.params.t_size;
	int bsize;
	char *bpos = buffer;
	
	while( remain > 0 )
	{
		progress = (51*((1024*((param.params.t_size-remain)>>2))
			/(param.params.t_size>>2)))/1024;
		if( progress > last_progress )
		{
			for( int i=0; i<(progress-last_progress); i++ )
			{
				printf( "#" );
			}
			fflush( stdout );
			last_progress = progress;
		}
		bsize = remain;
		if( bsize > 1024 )
			bsize = 1024;
		
		serial->SendBytes( bpos, bsize );
		
		bpos += bsize;
		remain -= bsize;
	}
	
	if( progress < 50 )
	{
		for( int i=0; i<(50-progress); i++ )
			printf( "#" );
	}
	printf( "\n" );
	
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

	/* draw the progress bar */
	printf(" ");
	for( int i=0; i<50; i++ )
	{
		printf(".");
	}
	
	printf("]\r[");
	fflush(stdout);
	
	int progress,last_progress = 0;
	int remain = param.size;
	int bsize;
	char *bpos = buffer;
	
	while( remain > 0 )
	{
		progress = (51*((1024*((param.size-remain)>>2))
			/(param.size>>2)))/1024;
		if( progress > last_progress )
		{
			for( int i=0; i<(progress-last_progress); i++ )
			{
				printf( "#" );
			}
			fflush( stdout );
			last_progress = progress;
		}
		bsize = remain;
		if( bsize > 1024 )
			bsize = 1024;
		
		serial->SendBytes( bpos, bsize );
		
		bpos += bsize;
		remain -= bsize;
	}
	
	if( progress < 50 )
	{
		for( int i=0; i<(50-progress); i++ )
			printf( "#" );
	}
	printf( "\n" );
	
	free( buffer );
	
	return( 0 );
	
} /* uploadBIN */
