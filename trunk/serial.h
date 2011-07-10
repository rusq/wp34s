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

#ifndef __SERIAL_H__
#define __SERIAL_H__

#include "xeq.h"

// User visible routines
extern void send_program(decimal64 *nul1, decimal64 *nul2);
extern void recv_program(decimal64 *nul1, decimal64 *nul2);
extern void send_registers(decimal64 *nul1, decimal64 *nul2);
extern void recv_registers(decimal64 *nul1, decimal64 *nul2);
extern void send_all(decimal64 *nul1, decimal64 *nul2);
extern void recv_all(decimal64 *nul1, decimal64 *nul2);
extern void send_byte(decimal64 *nul1, decimal64 *nul2);
extern int recv_byte(int timeout);

// Call-back for a received byte with error information
extern void received_byte( short byte );

// Implemented by the hardware
extern int open_port( int baud, int bits, int stopbits, int parity );
extern void close_port( void );
extern void put_byte( unsigned char byte );

#endif
