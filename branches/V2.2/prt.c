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

#include "xeq.h"
#include "display.h"
#include "consts.h"


static const char *make_complex(const char *str, char *instr) {
	char *p = instr;
	const char *s = str;

	*p++ = COMPLEX_PREFIX;
	while ((*p++ = *s++) != '\0');
	return instr;
}

static const char *prt_monadic(const unsigned int f, char *instr) {
	if (f < num_monfuncs && (! isNULL(monfuncs[f].mondreal) || ! isNULL(monfuncs[f].monint)))
		return sncopy(instr, monfuncs[f].fname, NAME_LEN);
	return "???";
}

static const char *prt_dyadic(const unsigned int f, char *instr) {
	if (f < num_dyfuncs && (! isNULL(dyfuncs[f].dydreal) || ! isNULL(dyfuncs[f].dydint)))
		return sncopy(instr, dyfuncs[f].fname, NAME_LEN);
	return "???";
}

static const char *prt_triadic(const unsigned int f, char *instr) {
	if (f < num_trifuncs && (! isNULL(trifuncs[f].trireal) || ! isNULL(trifuncs[f].triint)))
		return sncopy(instr, trifuncs[f].fname, NAME_LEN);
	return "???";
}

static const char *prt_monadic_cmplx(const unsigned int f, char *instr) {
	char buf[NAME_LEN + 1];

	if (f < num_monfuncs && ! isNULL(monfuncs[f].mondcmplx)) {
		if (isNULL(monfuncs[f].mondreal))
			return monfuncs[f].fname;
		
		return make_complex(sncopy(buf, monfuncs[f].fname, NAME_LEN), instr);
	}
	return "???";
}

static const char *prt_dyadic_cmplx(const unsigned int f, char *instr) {
	char buf[NAME_LEN + 1];

	if (f < num_dyfuncs && ! isNULL(dyfuncs[f].dydcmplx)) {
		if (isNULL(dyfuncs[f].dydreal))
			return dyfuncs[f].fname;
		return make_complex(sncopy(buf, dyfuncs[f].fname, NAME_LEN), instr);
	}
	return "???";
}

static const char *prt_niladic(const unsigned int idx, char *instr) {
	if (idx < num_niladics)
		return sncopy(instr, niladics[idx].nname, NAME_LEN);
	return "???";
}

static const char *prt_tst(const char *r, const enum tst_op op, char *instr, int cmplx) {
	char *p = instr;
	if (cmplx)
		*p++ = COMPLEX_PREFIX;
	*p++ = 'x';
	*p++ = "=\013\035<\011>\012"[op];
	*scopy(p, r) = '?';
	return instr;
}

static const char *prt_specials(const unsigned int opm, char *instr) {

	switch (opm) {
	case OP_0:	case OP_1:	case OP_2:
	case OP_3:	case OP_4:	case OP_5:
	case OP_6:	case OP_7:	case OP_8:
	case OP_9:
		instr[0] = opm - OP_0 + '0';
		return instr;

	case OP_A:	case OP_B:	case OP_C:
	case OP_D:	case OP_E:	case OP_F:
		instr[0] = opm - OP_A + 'A';
		return instr;

#ifdef COMPILE_CATALOGUES
	case OP_DOT:	return ".";
#else
	case OP_DOT:	return UState.fraccomma?",":".";
#endif
	case OP_CHS:	return "+/-";
	//case OP_CLX:	return "CLx";
	case OP_ENTER:	return "ENTER\020";
	case OP_EEX:	return "EEX";
	case OP_SIGMAPLUS:	return "\221+";
	case OP_SIGMAMINUS:	return "\221-";

	case OP_Xeq0:	case OP_Xlt0:	case OP_Xle0:
	case OP_Xne0:	case OP_Xgt0:	case OP_Xge0:
	case OP_Xapx0:
		return prt_tst("0", (enum tst_op)(opm - OP_Xeq0), instr, 0);
	case OP_Zeq0:	case OP_Zne0:
	//case OP_Zapx0:
		return prt_tst("0", (enum tst_op)(opm - OP_Zeq0), instr, 1);

	case OP_Xeq1:	case OP_Xlt1:	case OP_Xle1:
	case OP_Xne1:	case OP_Xgt1:	case OP_Xge1:
	case OP_Xapx1:
		return prt_tst("1", (enum tst_op)(opm - OP_Xeq1), instr, 0);
	case OP_Zeq1:	case OP_Zne1:
	//case OP_Zapx1:
		return prt_tst("1", (enum tst_op)(opm - OP_Zeq1), instr, 1);
	}
	return "???";
}


/* Metric <-> imperiam conversions */
static const char *prt_conv(unsigned int arg, char *instr) {
	const unsigned int conv = arg / 2;
	const unsigned int dirn = arg & 1;

	if (conv >= NUM_CONSTS_CONV)
		return "???";
	if (dirn == 0) {		// metric to imperial
		sncopy(sncopy_char(instr, cnsts_conv[conv].metric, METRIC_NAMELEN, '\015'),
				cnsts_conv[conv].imperial, IMPERIAL_NAMELEN);
	} else {			// imperial to metric
		sncopy(sncopy_char(instr, cnsts_conv[conv].imperial, IMPERIAL_NAMELEN, '\015'),
				cnsts_conv[conv].metric, METRIC_NAMELEN);
	}
	return instr;
}

/* Commands that take an argument */
static const char *prt_rargs(const opcode op, char *instr) {
	unsigned int arg = op & RARG_MASK;
	int ind = op & RARG_IND;
	const unsigned int cmd = RARG_CMD(op);
	char buf[CONST_NAMELEN + 1];

	if (cmd == RARG_ALPHA) {
		*scopy(instr, "\240 ") = arg + (ind?RARG_IND:0);
	} else if (cmd >= num_argcmds || argcmds[cmd].cmd == NULL)
		return "???";
	else if (!ind) {
		if (arg >= argcmds[cmd].lim || (argcmds[cmd].stos && arg > 96 && arg != regA_idx))
			return "???";
		if (cmd == RARG_CONST) {
			//return sncopy(instr, cnsts[arg].cname, CONST_NAMELEN);
			sncopy(scopy(instr, "# "), cnsts[arg].cname, CONST_NAMELEN);
			return instr;
		}
		if (cmd == RARG_CONST_CMPLX) {
			//return make_complex(sncopy(buf, cnsts[arg].cname, CONST_NAMELEN), instr);
			sncopy(scopy(buf, "# "), cnsts[arg].cname, CONST_NAMELEN);
			return make_complex(buf, instr);
		}
		if (cmd == RARG_CONV)
			return prt_conv(arg, instr);
		if (cmd == RARG_CONST_INT) {
			char buf[32];

			decimal64ToString(&CONSTANT_INT(arg), buf);
			buf[10] = '\0';
			scopy(scopy(instr, "iC "), buf);
		} else {
			if (arg >= regX_idx && arg <= regK_idx && argcmds[cmd].stckreg) {
				*sncopy_spc(instr, argcmds[cmd].cmd, NAME_LEN) = REGNAMES[arg-regX_idx];
			} else if (/*argcmds[cmd].label && */arg >= 100) {
				*sncopy_spc(instr, argcmds[cmd].cmd, NAME_LEN) = LBLNAMES[arg-100];
			} else {
				num_arg_0(sncopy_spc(instr, argcmds[cmd].cmd, NAME_LEN), arg, argcmds[cmd].lim < 10 ? 1 : 2);
			}
		}
	} else {
		if (!argcmds[cmd].indirectokay)
			return "???";
		if (arg >= regX_idx && arg <= regK_idx)
			*sncopy_char(instr, argcmds[cmd].cmd, NAME_LEN, '\015') = REGNAMES[arg-regX_idx];
		else if (arg < NUMREG)
			num_arg_0(sncopy_char(instr, argcmds[cmd].cmd, NAME_LEN, '\015'), arg, 2);
		else
			return "???";
	}
	return instr;
}

static const char *prt_multi(const opcode op, char *instr) {
	char *p, c;
	int cmd = opDBL(op);

	if (cmd >= num_multicmds)
		return "???";
	p = sncopy_char(instr, multicmds[cmd].cmd, NAME_LEN, '\'');
	*p++ = op & 0xff;
	c = (op >> 16) & 0xff;
	if (c != '\0') {
		*p++ = c;
		c = op>>24;
		if (c != '\0')
			*p++ = c;
	}
	*p = '\'';
	return instr;
}

const char *prt(opcode op, char *instr) {
	unsigned int arg;
	xset(instr, '\0', 16);
	if (isDBL(op))
		return prt_multi(op, instr);
	if (isRARG(op))
		return prt_rargs(op, instr);
	arg = argKIND(op);
	switch (opKIND(op)) {
	case KIND_SPEC:	return prt_specials(arg, instr);
	case KIND_NIL:	return prt_niladic(arg, instr);
	case KIND_MON:	return prt_monadic(arg, instr);
	case KIND_DYA:	return prt_dyadic(arg, instr);
	case KIND_TRI:	return prt_triadic(arg, instr);
	case KIND_CMON:	return prt_monadic_cmplx(arg, instr);
	case KIND_CDYA:	return prt_dyadic_cmplx(arg, instr);
	}
	return "???";
}

const char *catcmd(opcode op, char instr[16]) {
	unsigned int f;

	xset(instr, '\0', 16);
	if (isDBL(op)) {
		return prt_multi(op, instr);
	} else if (isRARG(op)) {
		f = RARG_CMD(op);
		if (f < num_argcmds) {
			if (f == RARG_CONST || f == RARG_CONST_CMPLX) {
				const unsigned int arg = op & RARG_MASK;
				if (arg < NUM_CONSTS)
					return sncopy(instr, cnsts[arg].cname, CONST_NAMELEN);
			} else if (f == RARG_CONV) {
				return prt_conv(op & RARG_MASK, instr);
			} else if (f == RARG_ALPHA) {
				*instr = op & 0xff;
				return instr;
			} else
				return sncopy(instr, argcmds[f].cmd, NAME_LEN);
		}
	} else {
		f = argKIND(op);
		switch (opKIND(op)) {
		default:
			break;
		case KIND_SPEC:
			return prt_specials(f, instr);
		case KIND_NIL:
			if (f < num_niladics)
				return sncopy(instr, niladics[f].nname, NAME_LEN);
			break;

		case KIND_MON:
		case KIND_CMON:
			if (f < num_monfuncs)
				return sncopy(instr, monfuncs[f].fname, NAME_LEN);
			break;

		case KIND_DYA:
		case KIND_CDYA:
			if (f < num_dyfuncs)
				return sncopy(instr, dyfuncs[f].fname, NAME_LEN);
			break;
		case KIND_TRI:
			if (f < num_trifuncs)
				return sncopy(instr, trifuncs[f].fname, NAME_LEN);
			break;
		}
	}
	return "???";
}
