/* This file is part of 34S.
 * 
 * 34S is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * 34S is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with 34S.  If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * This module handles all load/save operations in the real build or emulator
 * Module written by MvC
 */
#ifdef REALBUILD
#define PERSISTENT_RAM __attribute__((section(".persistentram")))
#define SLCDCMEM       __attribute__((section(".slcdcmem")))
#define VOLATILE_RAM   __attribute__((section(".volatileram")))
#define BACKUP_FLASH   __attribute__((section(".backupflash")))
#ifndef NULL
#define NULL 0
#endif
#else
// Emulator definitions
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <stdarg.h>
#ifdef WINGUI
#define shutdown _shutdown
#include <windows.h>
#undef shutdown
#endif
#if defined(QTGUI) || ( defined(USECURSES) && !defined(WIN32) )
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#endif

#define PERSISTENT_RAM
#define SLCDCMEM
#define VOLATILE_RAM
#define BACKUP_FLASH
#define STATE_FILE "wp34s.dat"
#define BACKUP_FILE "wp34s-backup.dat"
#define LIBRARY_FILE "wp34s-lib.dat"
#endif

#include "xeq.h"
#include "storage.h"
#include "display.h"
#include "stats.h"
#include "alpha.h"

#define PAGE_SIZE	 256

/*
 *  Setup the persistent RAM
 */
PERSISTENT_RAM TPersistentRam PersistentRam;

/*
 *  Data that is saved in the SLCD controller during deep sleep
 */
SLCDCMEM TStateWhileOn StateWhileOn;

/*
 *  A private register area for XROM code in volatile RAM
 *  It replaces the local registers and flags if active.
 */
TXromParams XromParams;
VOLATILE_RAM TXromLocal XromLocal;

/* Private space for four registers temporarily
 */
VOLATILE_RAM REGISTER XromA2D[4];

/*
 *  The backup flash area:
 *  2 KB for storage of programs and registers
 *  Same data as in persistent RAM but in flash memory
 */
BACKUP_FLASH TPersistentRam BackupFlash;

#ifndef REALBUILD
/*
 *  We need to define the Library space here.
 *  On the device the linker takes care of this.
 */
FLASH_REGION UserFlash;
#endif

/*
 *  The CCITT 16 bit CRC algorithm (X^16 + X^12 + X^5 + 1)
 */
unsigned short int crc16( const void *base, unsigned int length )
{
	unsigned short int crc = 0x5aa5;
	unsigned char *d = (unsigned char *) base;
	unsigned int i;

	for ( i = 0; i < length; ++i ) {
		crc  = ( (unsigned char)( crc >> 8 ) ) | ( crc << 8 );
		crc ^= *d++;
		crc ^= ( (unsigned char)( crc & 0xff ) ) >> 4;
		crc ^= crc << 12;
		crc ^= ( crc & 0xff ) << 5;
	}
	return crc;
}


/*
 *  Compute a checksum and compare it against the stored sum
 *  Returns non zero value if failure
 */
static int test_checksum( const void *data, unsigned int length, unsigned short oldcrc, unsigned short *pcrc )
{
	unsigned short crc;
	crc = crc16( data, length );
	if ( pcrc != NULL ) {
		*pcrc = crc;
	}
	return crc != oldcrc && oldcrc != MAGIC_MARKER;
}


/*
 *  Checksum the current program.
 */
short unsigned int checksum_program( void )
{
	update_program_bounds( 1 );
	return crc16( get_current_prog(), ProgEnd - ProgBegin + 1 );
}


/*
 *  Checksum the persistent RAM area
 *  Returns non zero value if failure
 */
int checksum_ram( void )
{
	return test_checksum( &PersistentRam, sizeof( PersistentRam ) - sizeof( short ),
			      Crc, &Crc );
}


/*
 *  Checksum the backup flash region
 *  Returns non zero value if failure
 */
int checksum_backup( void )
{
	return test_checksum( &BackupFlash, sizeof( BackupFlash ) - sizeof( short ),
		              BackupFlash._crc, NULL );
}


/*
 *  Checksum a flash region
 *  Returns non zero value if failure
 */
static int checksum_region( FLASH_REGION *fr, FLASH_REGION *header )
{
	unsigned int l = header->size * sizeof( s_opcode );
	return l > sizeof( fr->prog ) || test_checksum( fr->prog, l, fr->crc, &(header->crc ) );
}


/*
 *  Helper to store final END in empty program space
 */
static void stoend( void )
{
	ProgSize = 1;
	Prog[ 0 ] = ( OP_NIL | OP_END );
}


/*
 *  Clear the program space
 */
void clpall( void )
{
	clrretstk_pc();
	stoend();
}


/*
 *  Sanity checks for program (step) deletion
 */
static int check_delete_prog( unsigned int pc ) 
{
	if ( !isRAM( pc ) || ( pc == ProgSize && getprog( pc ) == ( OP_NIL | OP_END ) ) ) {
		report_warn(ERR_READ_ONLY);
	}
	else {
		return 0;
	}
	return 1;
}


/*
 *  Clear just the current program
 */
void clrprog( void )
{
	update_program_bounds( 1 );
	if ( nLIB( ProgBegin ) == REGION_LIBRARY ) {
		/*
		 *  Porgram is in flash
		 */
		flash_remove( ProgBegin, ProgEnd + 1 - ProgBegin );
	}
	else {
		if ( check_delete_prog( ProgBegin ) ) {
			return;
		}
		clrretstk();
		xcopy( Prog_1 + ProgBegin, Prog + ProgEnd, ( ProgSize - ProgEnd ) << 1 );
		ProgSize -= ( ProgEnd + 1 - ProgBegin );
		if ( ProgSize == 0 ) {
			stoend();
		}
	}
	set_pc( ProgBegin - 1 );
	update_program_bounds( 1 );
}
 

/*
 *  Clear all - programs and registers
 */
void clrall(void) 
{
	NumRegs = TOPREALREG;
	xeq_init_contexts();
	clrreg( OP_CLREG );
	clrstk( OP_CLSTK );
	clralpha( OP_CLRALPHA );
	clrflags( OP_CLFLAGS );
	clpall();

	reset_shift();
	State2.test = TST_NONE;

	DispMsg = NULL;
}


/*
 *  Clear everything
 */
void reset( void ) 
{
	xset( &PersistentRam, 0, sizeof( PersistentRam ) );
	clrall();
	init_state();
	UState.contrast = 6;
#ifdef INFRARED
	State.print_delay = 10;
#endif
	DispMsg = "Erased";
}


/*
 *  Store into program space.
 */
void stoprog( opcode c ) {
	const int off = isDBL( c ) ? 2 : 1;
	int i;
	unsigned int pc = state_pc();

	if ( pc == ProgSize && c != ( OP_NIL | OP_END ) )
		stoprog( OP_NIL | OP_END );

	if ( !isRAM( pc ) ) {
		report_warn( ERR_READ_ONLY );
		return;
	}
	clrretstk();
	xeq_init_contexts();
	if ( ProgFree < off ) {
		return;
	}
	ProgSize += off;
	ProgEnd += off;
	pc = do_inc( pc, 0 );	// Don't wrap on END
	for ( i = ProgSize + 1; i > (int) pc; --i ) {
		Prog_1[ i ] = Prog_1[ i - off ];
	}
	if (isDBL(c))
		Prog_1[pc + 1] = c >> 16;
	Prog_1[pc] = c;
	State.pc = pc;
}


/*
 *  Delete the current step in the program
 */
void delprog( void )
{
	int i;
	const unsigned int pc = state_pc();
	int off;

	if ( check_delete_prog( pc ) )
		return;
	if ( pc == 0 )
		return;

	clrretstk(); // ND change

	off = isDBL( Prog_1[ pc ]) ? 2 : 1;
	ProgSize -= off;
	ProgEnd -= off;
	for ( i = pc; i <= (int) ProgSize; ++i )
		Prog_1[ i ] = Prog_1[ i + off ];
	decpc();
}


/*
 *  Helper to append a program in RAM.
 *  Returns non zero in case of an error.
 */
int append_program( const s_opcode *source, int length )
{
	unsigned short pc;
	int space_needed = length - ProgFree;

	if ( ProgSize == 1 ) {
		/*
		 *  Only the default END statement is present
		 */
		--space_needed;
		--ProgSize;
	}
	if ( length > NUMPROG_LIMIT ) {
		return report_err( ERR_INVALID );
	}
	if ( length > NUMPROG_LIMIT - ProgSize ) {
		return report_err( ERR_RAM_FULL );
	}

	/*
	 *  Make room if needed
	 */
	clrretstk();
	if ( space_needed > 0 && SizeStatRegs != 0 ) {
		space_needed -= SizeStatRegs;
		sigmaDeallocate();
	}
	if ( space_needed > 0 ) {
		int regs;
		if (is_dblmode())
			regs = global_regs() - ( ( space_needed + 7 ) >> 3 );
		else
			regs = NumRegs - ( ( space_needed + 3 ) >> 2 );

		if ( regs < 0 ) {
			return report_err( ERR_RAM_FULL );
		}
		cmdregs( regs, RARG_REGS );
	}
	/*
	 *  Append data
	 */
	pc = ProgSize + 1;
	ProgSize += length;
	xcopy( Prog_1 + pc, source, length << 1 );
	set_pc( pc );
	return 0;
}


#ifdef REALBUILD
/*
 *  We do not copy any static data from flash to RAM at startup and
 *  thus can't use code in RAM. In order to program flash use the
 *  IAP feature in ROM instead
 */
#define IAP_FUNC ((int (*)(unsigned int)) (*(int *)0x400008))

/*
 *  Issue a command to the flash controller. Must be done from ROM.
 *  Returns zero if OK or non zero on error.
 */
static int flash_command( unsigned int cmd )
{
	SUPC_SetVoltageOutput( SUPC_VDD_180 );
	return IAP_FUNC( cmd ) >> 1;
}

/*
 *  Program the flash starting at destination.
 *  Returns 0 if OK or non zero on error.
 *  count is in pages, destination % PAGE_SIZE needs to be 0.
 */
static int program_flash( void *destination, void *source, int count )
{
	unsigned int *flash = (unsigned int *) destination;
	unsigned short int *sp = (unsigned short int *) source;

	lock();  // No interrupts, please!

	while ( count-- > 0 ) {
		/*
		 *  Setup the command for the controller by computing the page from the address
		 */
		const unsigned int cmd = 0x5A000003 | ( (unsigned int) flash & 0x1ff00 );
		int i;

		/*
		 *  Copy the source to the flash write buffer
		 */
		for ( i = 0; i < PAGE_SIZE / 4; ++i, sp += 2 ) {
			*flash++ = *sp | ( (unsigned int) ( sp[ 1 ] ) << 16 );
		}

		/*
		 *  Command the controller to erase and write the page.
		 */
		if ( flash_command( cmd ) ) {
			report_err( ERR_IO );
			break;
		}
	}
	unlock();
	return Error != 0;
}


/*
 *  Set the boot bit to ROM and turn off the device.
 *  Next power ON goes into SAM-BA mode.
 */
void sam_ba_boot(void)
{
	/*
	 *  Command the controller to clear GPNVM1
	 */
	lock();
	flash_command( 0x5A00010C );
	SUPC_Shutdown();
}


#else

/*
 *  Emulate the flash in a file wp34s-lib.dat or wp34s-backup.dat
 *  Page numbers are relative to the start of the user flash
 *  count is in pages, destination % PAGE_SIZE needs to be 0.
 */
#if defined(QTGUI) || defined(IOS)
extern char* get_region_path(int region);
#else
static char* get_region_path(int region)
{
	return region == REGION_BACKUP ? BACKUP_FILE : LIBRARY_FILE;
}
#endif

static int program_flash( void *destination, void *source, int count )
{
	char *name;
	char *dest = (char *) destination;
	FILE *f = NULL;
	int offset;

	/*
	 *  Copy the source to the destination memory
	 */
	memcpy( dest, source, count * PAGE_SIZE );

	/*
	 *  Update the correct region file
	 */
	if ( dest >= (char *) &BackupFlash && dest < (char *) &BackupFlash + sizeof( BackupFlash ) ) {
		name = get_region_path( REGION_BACKUP );
		offset = dest - (char *) &BackupFlash;
	}
	else if ( dest >= (char *) &UserFlash && dest < (char *) &UserFlash + sizeof( UserFlash ) ) {
		name = get_region_path( REGION_LIBRARY );
		offset = dest - (char *) &UserFlash;
	}
	else {
		// Bad address
		report_err( ERR_ILLEGAL );
		return 1;
	}
	f = fopen( name, "rb+" );
	if ( f == NULL ) {
		f = fopen( name, "wb+" );
	}
	if ( f == NULL ) {
		report_err( ERR_IO );
		return 1;
	}
	fseek( f, offset, SEEK_SET );
	if ( count != fwrite( dest, PAGE_SIZE, count, f ) ) {
		fclose( f );
		report_err( ERR_IO );
		return 1;
	}
	fclose( f );
	return 0;
}
#endif


/*
 *  Initialize the library to an empty state if it's not valid
 */
void init_library( void )
{
	if ( checksum_region( &UserFlash, &UserFlash ) ) {
		struct {
			unsigned short crc;
			unsigned short size;
			s_opcode prog[ 126 ];
		} lib;
		lib.size = 0;
		lib.crc = MAGIC_MARKER;
		xset( lib.prog, 0xff, sizeof( lib.prog ) );
		program_flash( &UserFlash, &lib, 1 );
	}
}


/*
 *  Add data at the end of user flash memory.
 *  Update crc and counter when done.
 *  All sizes are given in steps.
 */
static int flash_append( int destination_step, const s_opcode *source, int count, int size )
{
	char *dest = (char *) ( UserFlash.prog + destination_step );
	char *src = (char *) source;
#ifdef REALBUILD
	int offset_in_page = (int) dest & 0xff;
#else
	int offset_in_page = ( dest - (char *) &UserFlash ) & 0xff;
#endif
	char buffer[ PAGE_SIZE ];
	FLASH_REGION *fr = (FLASH_REGION *) buffer;
	count <<= 1;

	if ( offset_in_page != 0 ) {
		/*
		 *  We are not on a page boundary
		 *  Assemble a buffer from existing and new data
		 */
		const int bytes = PAGE_SIZE - offset_in_page;
		xcopy( buffer, dest - offset_in_page, offset_in_page );
		xcopy( buffer + offset_in_page, src, bytes );
		if ( program_flash( dest - offset_in_page, buffer, 1 ) ) {
			return 1;
		}
		src += bytes;
		dest += bytes;
		count -= bytes;
	}

	if ( count > 0 ) {
		/*
		 *  Move multiples of complete pages
		 */
		count = ( count + ( PAGE_SIZE - 1 ) ) >> 8;
		if ( program_flash( dest, src, count ) ) {
			return 1;
		}
	}

	/*
	 *  Update the library header to fix the crc and size fields.
	 */
	xcopy( fr, &UserFlash, PAGE_SIZE );
	fr->size = size;
	checksum_region( &UserFlash, fr );
	return program_flash( &UserFlash, fr, 1 );
}


/*
 *  Remove steps from user flash memory.
 */
int flash_remove( int step_no, int count )
{
	const int size = UserFlash.size - count;
	step_no = offsetLIB( step_no );
	return flash_append( step_no, UserFlash.prog + step_no + count,
			     size - step_no, size );
}


/*
 *  Simple backup / restore
 *  Started with ON+STO or ON+RCL or the SAVE/LOAD commands
 *  The backup area is the last 2KB of flash (pages 504 to 511)
 */
void flash_backup( enum nilop op )
{
	if ( not_running() ) {
		process_cmdline_set_lift();
		init_state();
		checksum_all();

		if ( program_flash( &BackupFlash, &PersistentRam, sizeof( BackupFlash ) / PAGE_SIZE ) ) {
			report_err( ERR_IO );
			DispMsg = "Error";
		}
		else {
			DispMsg = "Saved";
		}
	}
}


void flash_restore( enum nilop op )
{
	if ( not_running() ) {
		if ( checksum_backup() ) {
			report_err( ERR_INVALID );
		}
		else {
			xcopy( &PersistentRam, &BackupFlash, sizeof( PersistentRam ) );
			init_state();
			DispMsg = "Restored";
		}
	}
}


/*
 *  Load the user program area from the backup.
 *  Called by PLOAD.
 */
void load_program( enum nilop op )
{
	if ( not_running() ) {
		if ( checksum_backup() ) {
			/*
			 *  Not a valid backup
			 */
			report_err( ERR_INVALID );
			return;
		}
		clpall();
		append_program( BackupFlash._prog, BackupFlash._prog_size );
	}
}


/*
 *  Load registers from backup
 */
void load_registers( enum nilop op )
{
	int count;
	if ( checksum_backup() ) {
		/*
		 *  Not a valid backup region
		 */
		report_err( ERR_INVALID );
		return;
	}
	count = NumRegs;
	if ( is_dblmode() ) {
		// Don't clobber the stack in DP mode
		count -= EXTRA_REG + STACK_SIZE;
	}
	if ( count > BackupFlash._numregs ) {
		count = BackupFlash._numregs;
	}
	xcopy( get_reg_n(0), get_flash_reg_n(0), count << 3 );
}


/*
 *  Load the statistical summation registers from backup
 */
void load_sigma( enum nilop op )
{
	if ( checksum_backup() ) {
		/*
		 *  Not a valid backup region
		 */
		report_err( ERR_INVALID );
		return;
	}
	if ( ! BackupFlash._state.have_stats ) {
		/*
		 *  Backup has no data
		 */
		report_err( ERR_MORE_POINTS );
		return;
	}
	sigmaCopy( ( (char *)( BackupFlash._regs + TOPREALREG - BackupFlash._numregs ) - sizeof( STAT_DATA ) ) );
}


/*
 *  Load the configuration data from the backup
 */
void load_state( enum nilop op )
{
	if ( not_running() ) {
		if ( checksum_backup() ) {
			/*
			 *  Not a valid backup region
			 */
			report_err( ERR_INVALID );
			return;
		}
		xcopy( &RandS1, &BackupFlash._rand_s1, (char *) &Crc - (char *) &RandS1 );
		init_state();
		clrretstk_pc();
	}
}


/*
 *  Save a user program to the library region. Called by PSTO.
 */
void store_program( enum nilop op )
{
	opcode lbl; 
	unsigned int pc;
	int space_needed, count, free;

	if ( not_running() ) {
		/*
		 *  Don't copy from library or XROM
		 */
		pc = nLIB( state_pc() );
		if ( pc == REGION_LIBRARY || pc == REGION_XROM ) {
			report_err( ERR_ILLEGAL );
			return;
		}
		/*
		 *  Check if program is labeled
		 */
		update_program_bounds( 1 );
		lbl = getprog( ProgBegin );
		if ( !isDBL(lbl) || opDBL(lbl) != DBL_LBL ) {
			report_err( ERR_NO_LBL );
			return;
		}
		/*
		 *  Compute space needed
		 */
		count = space_needed = 1 + ProgEnd - ProgBegin;
		free = NUMPROG_FLASH_MAX - UserFlash.size;

		/*
		 *  Find a duplicate label in the library and delete the program
		 */
		pc = find_opcode_from( addrLIB( 0, REGION_LIBRARY ), lbl, 0 );
		if ( pc != 0 ) {
			/*
			 *  CLP in library
			 */
			unsigned int old_pc = state_pc();
			set_pc( pc );
			space_needed -= 1 + ProgEnd - ProgBegin;
			if ( space_needed <= free ) {
				clrprog();
			}
			set_pc( old_pc );
		}
		if ( space_needed > free ) {
			report_err( ERR_FLASH_FULL );
			return;
		}
		// 3. Append program
		flash_append( UserFlash.size, get_current_prog(), count, UserFlash.size + count );
	}
}


/*
 *  Load a user program from any region. Called by PRCL.
 */
void recall_program( enum nilop op )
{
	if ( not_running() ) {
		if ( state_pc() == 0 ) {
			State.pc = 1;
		}
		update_program_bounds( 1 );
		append_program( get_current_prog(), ProgEnd - ProgBegin + 1 );
	}
}


#if !defined(REALBUILD) && !defined(IOS)
/*
 *  Filesystem access for emulator
 */
#ifdef _WIN32
#define ASSEMBLER "..\\tools\\wp34s_asm.exe"
#else
#define ASSEMBLER "../tools/wp34s_asm.pl"
#endif
#define ASSEMBLER_OPTIONS ""
char CurrentDir[ FILENAME_MAX + 1 ];
char StateFile[ FILENAME_MAX + 1 ] = STATE_FILE;
char ComPort[ FILENAME_MAX + 1 ] = "COM1";
char Assembler[ FILENAME_MAX + 1 ] = ASSEMBLER;

/*
 *  Show (GUI) message
 */
#ifdef QTGUI
extern void showMessage(const char* title, const char* message);
#endif

static void ShowMessage( const char *title, const char *format, ... )
{
	va_list args;
#ifndef QTGUI
#ifdef WINGUI
	char msg[ 10000 ];
	va_start( args, format );
	vsprintf( msg, format, args );
	MessageBox( NULL, msg, title, MB_OK );
#else
	va_start( args, format );
	fprintf( stderr, "%s:\n", title );
	vfprintf( stderr, format, args );
	fputc( '\n', stderr );
#endif
#else
	char msg[ 10000 ];
	va_start( args, format );
	vsprintf( msg, format, args );
	showMessage(title, msg);
#endif
}

/*
 *  Save/Load state to a file
 */
void save_statefile( const char *filename )
{
	FILE *f;
	if ( filename != NULL && *filename != '\0' ) {
		strncpy( StateFile, filename, FILENAME_MAX );
	}
	f = fopen( StateFile, "wb" );
	if ( f == NULL ) {
		ShowMessage( "Save Error", strerror( errno ) );
		return;
	}
	process_cmdline_set_lift();
	init_state();
	checksum_all();
	fwrite( &PersistentRam, sizeof( PersistentRam ), 1, f );
	fclose( f );
#ifdef DEBUG
	printf( "sizeof struct _state = %d\n", (int)sizeof( struct _state ) );
	printf( "sizeof struct _ustate = %d\n", (int)sizeof( struct _ustate ) );
	printf( "sizeof RAM = %d (%d free)\n", (int)sizeof(PersistentRam), 2048 - (int)sizeof(PersistentRam));
	printf( "sizeof struct _state2 = %d\n", (int)sizeof( struct _state2 ) );
	printf( "sizeof while on = %d\n", (int)sizeof(TStateWhileOn));
	printf( "sizeof decNumber = %d\n", (int)sizeof(decNumber));
	printf( "sizeof decContext = %d\n", (int)sizeof(decContext));
#endif
}


/*
 *  Helper to expand filenames with startup directory
 */
#ifdef _WIN32
#include <direct.h>
#define getcwd _getcwd
#define SEPARATOR '\\'
#else
#define SEPARATOR '/'
#endif

static char *expand_filename( char *buffer, const char *filename )
{
	char *p;
	size_t l;

	if ( *CurrentDir == '\0' ) {
		// Determine current directory on first call
		getcwd( CurrentDir, FILENAME_MAX );
		p = CurrentDir + strlen( CurrentDir );
		if ( p != CurrentDir && p[ -1 ] != SEPARATOR ) {
			*p = SEPARATOR;
			p[ 1 ] = '\0';
		}
	}
	if ( *filename == SEPARATOR || filename[ 1 ] == ':' ) {
		// Absolute path left unchanged
		strncpy( buffer, filename, FILENAME_MAX );
	}
	else {
		// Prepend CurrentDir
		strncpy( buffer, CurrentDir, FILENAME_MAX );
		l = strlen( buffer );
		strncpy( buffer + l, filename, FILENAME_MAX - l );
	}
	return buffer;
}


/*
 *  Load both the RAM file and the flash emulation images
 */
void load_statefile(const char *filename )
{
	FILE *f;
	char buffer[ FILENAME_MAX + 1 ];
#if !defined(QTGUI) && !defined(IOS)
	char *p;
#endif

	if ( filename != NULL && *filename != '\0' ) {
		expand_filename( StateFile, filename );
	}
	f = fopen( StateFile, "rb" );
	if ( f != NULL ) {
		fread( &PersistentRam, sizeof( PersistentRam ), 1, f );
		fclose( f );
	}
	f = fopen( expand_filename( buffer, BACKUP_FILE ), "rb" );
	if ( f != NULL ) {
		fread( &BackupFlash, sizeof( BackupFlash ), 1, f );
		fclose( f );
	}
	else {
		// Emulate a backup
		BackupFlash = PersistentRam;
	}
	f = fopen( expand_filename( buffer, LIBRARY_FILE ), "rb" );
	if ( f != NULL ) {
		fread( &UserFlash, sizeof( UserFlash ), 1, f );
		fclose( f );
	}
	init_library();

#if !defined(QTGUI) && !defined(IOS)
	/*
	 *  Load the configuration
	 *  1st line: COM port
	 *  2nd line: Tools directory
	 */
	f = fopen( expand_filename( buffer, "wp34s.ini" ), "rt" );
	if ( f != NULL ) {
		// COM port
		p = fgets( buffer, FILENAME_MAX, f );
		if ( p != NULL ) {
			strtok( buffer, "#:\r\n\t " );
			if ( *buffer != '\0' ) {
				strncpy( ComPort, buffer, FILENAME_MAX );
			}
		}
		// Assembler
		p = fgets( buffer, FILENAME_MAX, f );
		if ( p != NULL ) {
			strtok( buffer, "#\r\n\t" );
			if ( *buffer != '\0' ) {
				p = buffer + strlen( buffer );
				while ( p != buffer && p[-1] == ' ' ) {
					*(--p) = '\0';
				}
				expand_filename( Assembler, buffer );
			}
		}
		fclose( f );
	}
#endif
}

/*
 *  Import text file
 */
static void show_log( char *logname, int rc )
{
	char msg[ 10000 ] = "";
	FILE *f = fopen( logname, "rt" );
	if ( f != NULL ) {
		int size = fread( msg, 1, sizeof( msg ) - 1, f );
		if ( size >= 0 ) {
			msg[ size ] = 0;
		}
		fclose( f );
	}
	remove( logname );
	if ( *msg == '\0' ) {
		sprintf( msg, "Cannot execute assembler %s, RC=%d", Assembler, rc );
	}
	ShowMessage( rc == 0 ? "Import Result" : "Import Failed", msg );
}

static char* mktmpname(char* name, const char* prefix)
{
#if defined(QTGUI) || ( defined(USECURSES) && ! defined(WIN32) )
	strcpy(name, "wp34s");
	strcat(name, prefix);
	strcat(name, "_XXXXXX");
	return mktemp(name);
#else
	return tmpnam(name);
#endif
}

void set_assembler(const char* assembler)
{
	strncpy(Assembler, assembler, FILENAME_MAX);
}

#ifdef QTGUI
static char* getTmpDir()
{
#ifdef _WIN32
	return getenv("TMP");
#else
	char *tmp = getenv("TMPDIR");
	if(tmp==NULL || *tmp==0)
	{
		tmp = P_tmpdir;
	}
	if(tmp==NULL || *tmp==0)
	{
		tmp="/tmp";
	}
	return tmp;
#endif
}
#endif

#define IMPORT_BUFFER_SIZE 10000
void import_textfile( const char *filename )
{
#ifdef QTGUI
	char previousDir[ IMPORT_BUFFER_SIZE ];
#endif
	char buffer[ IMPORT_BUFFER_SIZE ];
	char tempfile[ FILENAME_MAX ];
	char logfile[ FILENAME_MAX ];
	char *tempname, *logname;
	int rc = -1;
	FILE *f;

	tempname = mktmpname( tempfile, "tmp" );
	if ( *tempname == '\\' ) {
		++tempname;
	}
	logname = mktmpname( logfile, "log" );
	if ( *logname == '\\' ) {
		++logname;
	}

	sprintf( buffer, "%s %s -pp \"%s\" -o %s 1>%s 2>&1", Assembler, ASSEMBLER_OPTIONS, filename, tempname, logname );
#ifdef QTGUI
	getcwd(previousDir, IMPORT_BUFFER_SIZE);
	chdir(getTmpDir());
#endif
	rc = system( buffer );
	show_log( logname, rc );
	if ( rc == 0 ) {
		// Assembly successful
		int size, words = 0;
		f = fopen( tempname, "rb" );

		if ( f == NULL ) {
			ShowMessage( "Import Failed", "Assembler output file error: %s", strerror( errno ) );
		}
		else {
			size = (int) fread( buffer, 2, sizeof( buffer ) / 2, f );
			fclose( f );
			if ( size >= 2 ) {
				words = (unsigned char) buffer[ 3 ] * 256 + (unsigned char) buffer[ 2 ];
			}
			if ( words != size - 3 ) {
				// Bad file size
				ShowMessage( "Import Failed", "Bad assembler output file size %d", size );
			}
			else {
				append_program( (s_opcode *) ( buffer + 4 ), words );
				update_program_bounds( 1 );
			}
		}
	}
	remove( tempname );
	remove( "wp34s_pp.lst" );
#ifdef QTGUI
	chdir(previousDir);
#endif
}

/*
 *  Export: Print current program to text file
 */
#include "pretty.h"

static const char *pretty( unsigned char z ) {
	if ( z == 32 ) {
		return "space";
	}
	if ( z < 32 ) {
		return map32[ z & 0x1f ];
	}
	if (z >= 127) {
		return maptop[ z - 127 ];
	}
	return CNULL;
}


static void write_pretty( const char *in, FILE *f ) {
	const char *p;
	const char *delim;
	char c;

	delim = strchr( in, '\'' );
	if ( delim == NULL ) {
		delim = strchr( in, 0x06 );
	}
	while ( *in != '\0' ) {
		c = *in;
		p = NULL;
		if ( in++ == delim ) {
			if ( c == 0x06 ) {
				++in;
				c = ' ';
			}
		}
		else {
			p = pretty( c );
		}
		if ( p == CNULL ) {
			fputc( c, f );
		}
		else {
			fputc( '[', f );
			while ( *p != '\0' ) {
				fputc( *p++, f );
			}
			fputc( ']', f );
		}
	}
	fputc( '\n', f );
}


extern void export_textfile( const char *filename )
{
	FILE *f;
	unsigned int pc = state_pc();
	int runmode = State2.runmode;
	int numlen = isRAM( pc ) ? 3 : 4;

	f = fopen( filename, "wt" );
	if ( f == NULL ) return;

	if ( runmode ) {
		// current program
		pc = ProgBegin;
	}
	else {
		// complete program memory
		pc = 1;
		numlen = 3;
	}
	if ( pc == 0 ) {
		++pc;
	}

	PcWrapped = 0;
	while ( !PcWrapped ) {
		char buffer[ 16 ];
		const char *p;
		opcode op = getprog( pc );
		unsigned int upc = user_pc( pc );
		*num_arg_0( buffer, upc, numlen ) = '\0';
		fprintf( f, "%s ", buffer );
		p = prt( op, buffer );
		write_pretty( p, f );
		pc = do_inc( pc, runmode );
	}

	fclose( f );
}

#endif


