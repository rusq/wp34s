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
 *  This is the console emulator part
 */
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <unistd.h>

#include "xeq.h" 
#include "keys.h"
#include "display.h"
#include "lcd.h"
#include "int.h"
#include "consts.h"
#include "storage.h"

#include "catalogues.h"


#define CH_QUIT		'Q'
#define CH_TRACE	'T'
#define CH_FLAGS	'F'
#define CH_ICOUNT	'C'
#define CH_REFRESH	12	/* ^L */
#define CH_COPY		'X'
#define CH_PASTE	'V'

unsigned long long int instruction_count = 0;
int view_instruction_counter = 0;

/*
 *  PC keys to calculator keys
 */
static int remap(const int c) {
	switch (c) {
	case 'F':	return K_F;

	case 'q':	return K00;
	case 'w':	return K01;
	case 'B':	return K01;
	case 'e':	return K02;
	case 'C':	return K02;
	case 'r':	return K03;
	case 'D':	return K03;
	case 't':	return K04;
	case 'y':	return K05;

	case 'a':	return K10;
	case 's':	return K11;
	case 'd':	return K12;
	case 'f':	return K13;
	case 'g':	return K14;
	case 'h':	return K15;

	case 'z':	return K20;
	case 'x':	return K20;
	case '\r':	return K20;
	case '\n':	return K20;
	case 'c':	return K21;
	case 'v':	return K22;
	case 'b':	return K23;
	case 'n':	return K24;	// Backspace
	case 127:	return K24;
	case 8:		return K24;

	case 'u':	return K30;
	case '7':	return K31;
	case '8':	return K32;
	case '9':	return K33;
	case '/':	return K34;

	case 'j':	return K40;
	case '4':	return K41;
	case '5':	return K42;
	case '6':	return K43;
	case '*':	return K44;

	case 'm':	return K_F;
	case '1':	return K51;
	case '2':	return K52;
	case '3':	return K53;
	case '-':	return K54;

	case ' ':	return K60;	// ON
	case '0':	return K61;
	case '.':	return K62;
	case ',':	return K63;
	case '+':	return K64;
	}
	return K_UNKNOWN;
}


#include "pretty.c"

static void rarg_values(const opcode c, int l) {
	char bf[100];

	if (isRARG(c)) {
		const unsigned int cmd = RARG_CMD(c);
		const unsigned int idx = c & 0x7f;

		if (cmd == RARG_CONV) {
			while (l-- > 0)
				putchar(' ');
			printf("%c %s", (idx & 1)?'*':'/', decimal64ToString(&CONSTANT_CONV(idx/2), bf));
		} else if (cmd == RARG_CONST || cmd == RARG_CONST_CMPLX) {
			while (l-- > 0)
				putchar(' ');
			printf("%s", decimal64ToString((decimal64 *) get_const(c & 0x7f, 0), bf));
		}
	}
}
	
static int dumpop(const opcode c, int pt) {
	char tracebuf[25];
	const char *s, *m;

	if (c == RARG(RARG_ALPHA, 0))
		return 0;
	xset(tracebuf, '\0', sizeof(tracebuf));
	s = prt(c, tracebuf);
	if (strcmp(s, "???") != 0) {
		char t[100], *q = t;
		int l = 35;

		if (c == RARG(RARG_ALPHA, ' '))
			strcpy(tracebuf+2, "[space]");

		q += sprintf(t, "%04x  ", (unsigned int)c);
		while (*s != '\0') {
			const unsigned char z = 0xff & *s++;

			m = pretty(z);
			if (m == NULL) {
				*q++ = z;
				l--;
			} else {
				l -= slen(m) + 2;
				q += sprintf(q, "[%s]", m);
			}
		}
		*q++ = '\0';
		if (pt) {
			printf("%s", t);
			rarg_values(c, l);
			putchar('\n');
		}
		return 1;
	}
	return 0;
}


static void dump_menu(const char *name, const char *prefix, const enum catalogues cata) {
	int i;
	char cmd[16];
	const char *p;
	const char *buf;
	const char *m;
	const int oldcata = State2.catalogue;
	int n;

	State2.catalogue = cata;
	n = current_catalogue_max();
	printf("%s catalogue:\n", name);
	for (i=0; i<n; i++) {
		int l = 35 - slen(prefix);
		const opcode cati = current_catalogue(i);
		buf = catcmd(cati, cmd);

		if (cati == RARG(RARG_ALPHA, ' '))
			strcpy(cmd+2, "[space]");

		printf("\t%d\t%s", i+1, prefix);
		for (p=buf; *p != '\0'; p++) {
			const unsigned char c = 0xff & *p;
			m = pretty(c);
			if (m == NULL) {
				printf("%c", c);
				l--;
			} else {
				printf("[%s]", m);
				l -= slen(m) + 2;
			}
		}
		rarg_values(cati, l);
		printf("\n");
	}
	printf("\n");
	State2.catalogue = oldcata;
}

#include "xrom.h"
#include "xrom_labels.h"
	

static void dump_registers(void) {
	char buf[100];
	int i;
	if (is_dblmode()) {
		for (i=0; i<100; i += 2) {
			decimal128ToString(&(get_reg_n(i)->d), buf);
			printf("register %02d: %s\n", i, buf);
		}
		return;
	}
	for (i=0; i<100; i++) {
		decimal64ToString(&(get_reg_n(i)->s), buf);
		printf("register %02d: %s\n", i, buf);
	}
}


static void dump_constants(void) {
	char buf[100];
	int i;

	for (i=0; i<NUM_CONSTS; i++) {
		decimal128ToString(&(get_const(i, 1)->d), buf);
		printf("\t%02d\t%s\n", i, buf);
	}
}


static void dump_cmd_op(unsigned int op, unsigned int *n, int silent) {
	char buf[16];
	char out[1000];
	const char *pre;
	const char *p = catcmd(op, buf);
	const unsigned int opk = opKIND(op);
	const unsigned int opa = argKIND(op);

	if (strcmp(p, "???") == 0)
		return;
	if (! isRARG(op)) {
		if (opk == KIND_CMON && isNULL(monfuncs[opa].mondcmplx))
			return;
		if (opk == KIND_CDYA && isNULL(dyfuncs[opa].dydcmplx))
			return;
		if (opk == KIND_MON && isNULL(monfuncs[opa].mondreal) && isNULL(monfuncs[opa].monint))
			return ;
		if (opk == KIND_DYA && isNULL(dyfuncs[opa].dydreal) && isNULL(dyfuncs[opa].dydint))
			return ;
	}
	++*n;
	if (silent)
		return;
	pre = "";
	if (opk == KIND_CMON || opk == KIND_CDYA)
		pre = "[cmplx]";
	else if (isRARG(op) && RARG_CMD(op) == RARG_CONST_CMPLX)
		pre = "[cmplx]# ";
	else if (isRARG(op) && RARG_CMD(op) == RARG_CONST)
		pre = "# ";
	else if (isRARG(op) && RARG_CMD(op) == RARG_ALPHA)
		pre = "Alpha ";
	prettify(p, out, 0);
	if (strncmp("[cmplx]", out, 7) == 0 && !(isRARG(op) && RARG_CMD(op) == RARG_ALPHA))
		pre = "";
	printf("%-5u  %04x   %s%s\n", *n, op, pre, out);
}

static unsigned int dump_commands(int silent) {
	unsigned int i, j, n=0;

	if (! silent)
		printf("Number Opcode Command\n");
	for (i=0; i<KIND_MAX; i++)
		for (j=0; j<opcode_breaks[i]; j++)
			dump_cmd_op((i<<KIND_SHIFT) + j, &n, silent);
	for (i=0; i<NUM_RARG; i++) {
		if (i == RARG_CONST || i == RARG_CONST_CMPLX) {
			for (j=0; j<NUM_CONSTS_CAT-1; j++)
				dump_cmd_op(RARG(i, j), &n, silent);
			dump_cmd_op(RARG(i, OP_PI), &n, silent);
		} else if (i == RARG_CONV) {
			for (j=0; j<NUM_CONSTS_CONV*2; j++)
				dump_cmd_op(RARG(i, j), &n, silent);
		} else if (i == RARG_ALPHA) {
			for (j=1; j<256; j++)
				dump_cmd_op(RARG(i, j), &n, silent);
		} else
			dump_cmd_op(RARG(i, 0), &n, silent);
	}
	return n;
}


void shutdown_calc( void )
{
	checksum_all();
	setuptty( 1 );
	save_statefile();
	exit( 0 );
}


/*
 *  Dummies
 */
int is_key_pressed(void) 
{
	return 0;
}

int get_key(void)
{
	return 0;
}

int put_key( int k )
{
	return k;
}

enum shifts shift_down(void)
{
	return SHIFT_N;
}

#ifndef WIN32  // Windows uses winserial.c
/*
 *  Open a COM port for transmission
 */
int open_port( int baud, int bits, int parity, int stopbits )
{
	return 0;
}


/*
 *  Close the COM port after transmission is complete
 */
extern void close_port( void )
{
}


/*
 *  Output a single byte to the serial
 */
void put_byte( unsigned char byte )
{
	report_err(ERR_PROG_BAD);
}


/*
 *  Force buffer flush
 */
void flush_comm( void )
{
}

#endif



/*
 *  Main loop
 */
int main(int argc, char *argv[]) {
	int c, n = 0;
	int warm = 0;

	xeq_init_contexts();
	load_statefile();
	if (argc > 1) {
		if (argc == 2) {
			if (strcmp(argv[1], "commands") == 0) {
				dump_commands(0);
				return 0;
			}
			if (strcmp(argv[1], "reg") == 0) {
				dump_registers();
				return 0;
			}
			if (strcmp(argv[1], "wake") == 0) {
				warm = 1;
				goto skipargs;
			}
			if (strcmp(argv[1], "opcodes") == 0) {
				dump_opcodes(stdout, 0);
				return 0;
			}
			if (strcmp(argv[1], "xref") == 0) {
				dump_opcodes(stdout, 1);
				return 0;
			}
			if (strcmp(argv[1], "constants") == 0) {
				dump_constants();
				return 0;
			}
			dump_menu("statistics", "", CATALOGUE_STATS);
			dump_menu("probability", "", CATALOGUE_PROB);
			dump_menu("clear", "", CATALOGUE_CLEAR);
			dump_menu("displ", "", CATALOGUE_DISPL);
			dump_menu("more", "", CATALOGUE_MORE);
			dump_menu("modes", "", CATALOGUE_MODE);
			dump_menu("conversions", "", CATALOGUE_CONV);
			dump_menu("constants", "# ", CATALOGUE_CONST);
		}
		for (c=0; c<65536; c++) {
			if (isDBL(c) && (c & 0xff))	/* Don't show all multi-word instructions */
				continue;
			n += dumpop(c, argc > 2);
		}
		printf("total number of opcodes %d\n", n);
		printf("\tniladic commands %d\n", NUM_NILADIC);

		for (n=c=0; c<NUM_MONADIC; c++) {
			if (monfuncs[c].mondreal != NULL) n++;
			if (monfuncs[c].mondcmplx != NULL) n++;
			if (monfuncs[c].monint != NULL) n++;
		}
		printf("\tmonadic commands %d with %d functions\n", NUM_MONADIC, n);

		for (n=c=0; c<NUM_DYADIC; c++) {
			if (dyfuncs[c].dydreal != NULL) n++;
			if (dyfuncs[c].dydcmplx != NULL) n++;
			if (dyfuncs[c].dydint != NULL) n++;
		}
		printf("\tdyadic commands %d with %d functions\n", NUM_DYADIC, n);

		for (n=c=0; c<NUM_TRIADIC; c++) {
			if (trifuncs[c].trireal != NULL) n++;
			if (trifuncs[c].triint != NULL) n++;
		}
		printf("\ttriadic commands %d with %d functions\n", NUM_TRIADIC, n);

		printf("\targument commands %d\n", NUM_RARG);
		printf("\tmultiword commands %d\n", NUM_MULTI);
		printf("\tspecial commands %d\n", NUM_SPECIAL);

		printf("\ttotal commands %u\n", dump_commands(1));
		return 0;
	}
skipargs:
	if (!warm)
		init_34s();
	State2.flags = 1;
	if (setuptty(0) == 0) {
		display();
		JustDisplayed = 0;
		while ((c = GETCHAR()) != GETCHAR_ERR && c != CH_QUIT) {
#ifdef USECURSES
			if (c == CH_TRACE) {
				State2.trace = 1 - State2.trace;
				display();
			} else if (c == CH_FLAGS) {
				State2.flags = 1 - State2.flags;
				display();
			} else if (c == CH_REFRESH) {
				clear();
				display();
			} else if (c == CH_ICOUNT) {
				instruction_count = 0;
				view_instruction_counter = 1 - view_instruction_counter;
				display();
			} else if (c == CH_PASTE) {
				paste_raw_x("123.14159265358979323846264338327950");
				c = K_UNKNOWN;
				clear();
				display();
			} else if (c == CH_COPY) {
				char buffer[66];
				const char *p = fill_buffer_from_raw_x(buffer);
				printf("%s\n", p);
				fflush(NULL);
				sleep(1);
				c = K_UNKNOWN;
			} else
#endif
			c=remap(c);
			if(c != K_UNKNOWN) {
				process_keycode(c);
				process_keycode(K_RELEASE);
			}
		}
		setuptty(1);
	}
	shutdown_calc();
	return 0;
}

