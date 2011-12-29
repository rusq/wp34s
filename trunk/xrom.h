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

#ifndef __XROM_H__
#define __XROM_H__

#include "xeq.h"
#include "xrom_labels.h"

/* Entry points */
#define ENTRY_SIGMA	99	/* Same order: as RARG_SUM, RARG_PROD, RARG_SOLVE, RARG_INTG */
#define ENTRY_PI	98
#define ENTRY_SOLVE	97
#define ENTRY_DERIV	96
#define ENTRY_2DERIV	95
#define ENTRY_INTEGRATE	94

#define ENTRY_QUAD	89	/* Same order as OP_QUAD, OP_NEXTPRIME */
#define ENTRY_NEXTPRIME	88
#define ENTRY_ZETA	87
#define ENTRY_Bn	86
#define ENTRY_Bn_star	85
#define ENTRY_W1	84
#define ENTRY_SETEUR	83
#define ENTRY_SETUK	82
#define ENTRY_SETUSA	81
#define ENTRY_SETIND	80
#define ENTRY_SETCHN	79
#define ENTRY_SETJAP	78
#define ENTRY_WHO	77



extern const s_opcode xrom[];
extern const unsigned short int xrom_size;

#endif
