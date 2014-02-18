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

#ifndef __KEYS_H__
#define __KEYS_H__

#if 0
enum {
	KEY_11=0, KEY_12, KEY_13, KEY_14, KEY_15,
		KEY_16, KEY_17, KEY_18, KEY_19, KEY_10,
	KEY_21, KEY_22, KEY_23, KEY_24, KEY_25,
		KEY_26, KEY_27, KEY_28, KEY_29, KEY_20,
	KEY_31, KEY_32, KEY_33, KEY_34, KEY_35,
		KEY_36, KEY_37, KEY_38, KEY_39, KEY_30,
	KEY_41, KEY_42, KEY_43, KEY_44, KEY_45,
		KEY_46, KEY_47, KEY_48, KEY_49, KEY_40,
	KEY_01, KEY_02,

	KEY_SHIFT_null = 0x00,
	KEY_SHIFT_f = 0x40,
	KEY_SHIFT_g = 0x80,
	KEY_SHIFT_m = 0xc0
};
#endif

// MvC: changed keycodes for rows with just 5 keys to match HP SDK

typedef enum {
	K00 = 0,  K01 = 1,  K02 = 2,  K03 = 3,  K04 = 4,  K05 = 5,  // Row 1
	K10 = 6,  K11 = 7,  K12 = 8,  K13 = 9,  K14 = 10, K15 = 11, // Row 2
	K20 = 12, K21 = 13, K22 = 14, K23 = 15, K24 = 16,           // Row 3
	K30 = 18, K31 = 19, K32 = 20, K33 = 21, K34 = 22,           // Row 4
	K40 = 24, K41 = 25, K42 = 26, K43 = 27, K44 = 28,           // Row 5
#define K50 30													// F Shift
			  K51 = 31, K52 = 32, K53 = 33, K54 = 34,           // Row 6
	K60 = 36, K61 = 37, K62 = 38, K63 = 39, K64 = 40,           // Row 7 
	K70 = 41, K71 = 42, K72 = 43, K73 = 44,                     // move keys not used out of the way
} keycode;

#define K_UNKNOWN	-1
#define K_F		K50												// K_F is the Primary Shift Key in this Layout
#define K_G		K71												// K_G is unused in this layout
#define K_H		K72												// K_H is unused in this layout
#define K_ARROW		K40
#define K_CMPLX		K73											// K_CMPLX is unused in this layout

extern unsigned int keycode_to_digit_or_register(const keycode);

#endif
