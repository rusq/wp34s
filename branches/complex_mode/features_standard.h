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
#ifndef FEATURES_H__
#define FEATURES_H__

#if !defined(REALBUILD) && !defined(WINGUI) && !defined(QTGUI) && !defined(IOS)
#define CONSOLE
#endif

/*
 *  Select optional features here
 */

// Allow for any generic argument taking commands in XROM
// #define XROM_RARG_COMMANDS

// Permanently disable quartz crystal (cannot be enabled with ON+C)
//#define DISABLE_XTAL

#ifdef DISABLE_XTAL
#undef XTAL
#endif

// Define this to support a STOPWATCH function like the StopWatch on the HP-41C
// Time Module or the HP-55
#if !defined(REALBUILD) || (defined(XTAL) /* && !defined(INFRARED) */)
#define INCLUDE_STOPWATCH
#else
//#define INCLUDE_STOPWATCH
#endif

// Interrupt XROM code if the EXIT key is held down for at least the number
// of ticks (100 ms) specified below. Zero disables this feature. Without it
// the device needs to be reset if XROM code gets stuck in an infinite loop.
//#define INTERRUPT_XROM_TICKS 10

// Include the pixel plotting commands
// #define INCLUDE_PLOTTING

// Build a tiny version of the device
// #define TINY_BUILD

// Include a catalogue of the internal commands
// If not defined, these commands are put into P.FCN, TEST and CPX X.FCN instead.
// #define INCLUDE_INTERNAL_CATALOGUE
// #define INCLUDE_RELATIVE_CALLS

// Include a mechanism for a user defined catalogue
// 2-3 flash pages (512 - 768 bytes) in total.
// #define INCLUDE_USER_CATALOGUE

// Include the CNSTS command to access constants via indirection
// #define INCLUDE_INDIRECT_CONSTS

// Replace dispatch functions for calling niladic, monadic, dyadic and triadic
// functions and their complex variants with one universal dispatch function.
// It saves approximately 280 bytes in the firmware.
// This is an EXPERIMENTAL FEATURE that hasn't yet received adequate testing.
// #define UNIVERSAL_DISPATCH

// Code to allow access to caller's local data from xIN-code
// #define ENABLE_COPYLOCALS

#ifndef TINY_BUILD

// Include the Mantissa and exponent function
// Space cost is approximately 180 bytes
#define INCLUDE_MANTISSA

// Include the xroot function for reals, integers and complex numbers
// Space cost is approximately 400 bytes
#define INCLUDE_XROOT

// Include the user mode serial commands SOPEN, SCLOSE, RECV1, SEND1, aRECV, aSEND
// Space cost approximately 700 bytes.
// #define INCLUDE_USER_IO

// Include the SAVEM/RESTM user mode save and restore commands
#define INCLUDE_USER_MODE

// Include the Gudermannian functions and their inverses in the real
// and complex domain.
// #define INCLUDE_GUDERMANNIAN

// Include first and second order Bessel functions Jn, In, Yn and Kn for
// both real and complex arguments.  These are implemented in XROM.
// #define INCLUDE_XROM_BESSEL

// Inlcude real and complex flavours of the digamma function.  These are
// implemented in XROM.  The first setting is sufficient for accuracy for
// single precision, the second needs to be enabled as well to get good
// results for double precision..
// #define INCLUDE_XROM_DIGAMMA
// #define XROM_DIGAMMA_DOUBLE_PRECISION

// Include a fused multiply add instruction
// This isn't vital since this can be done using a complex addition.
// Space cost 108 bytes.
// #define INCLUDE_MULADD

// Include a date function to determine the date of Easter in a given year
// #define INCLUDE_EASTER

// Include code to use a Ridder's method step after a bisection in the solver.
// For some functions this seems to help a lot, for others there is limited
// benefit.
#define USE_RIDDERS

// Include code to find integer factors
// Space cost 480 bytes.
// #define INCLUDE_FACTOR

// Include matrix functions better implemented in user code
// #define SILLY_MATRIX_SUPPORT

// Include matrix row/row operations.
// M.R<->, M.R*, M.R+
// #define MATRIX_ROWOPS

// Include the LU decomposition as a user command
// M.LU
// #define MATRIX_LU_DECOMP

// Include fast path code to calculate factorials and gamma functions
// for positive integers using a string of multiplications.
#define GAMMA_FAST_INTEGERS

// Include the flash register recall routines RCF and their variants
// #define INCLUDE_FLASH_RECALL

// Include iBACK, etc. as user visible commands
// #define INCLUDE_INDIRECT_BRANCHES

// Include code to support the 41/42's MOD operation
#define INCLUDE_MOD41

// Include code to support integer mode multiplication and exponentation modulo
// operations.
#define INCLUDE_INT_MODULO_OPS

// Include code to support integer (truncated) division
#define INCLUDE_INTEGER_DIVIDE

// Make EEX key enter PI if pressed when command line empty
// Values:  0 or undefined:  feature disabled
//          1:               feature enabled if user flag L is set
//          2:               feature always enabled
//#define INCLUDE_EEX_PI 2

// Change the fraction separator to the old Casio form _|
//#define INCLUDE_CASIO_SEPARATOR

// Group digits when viewing the full X register.
#define FULL_NUMBER_GROUPING

// When viewing the full X register, don't group digits if thousands separator is turned off (only applies if FULL_NUMBER_GROUPING is enabled).
//#define FULL_NUMBER_GROUPING_TS

// Switch the seven-segment display to fraction mode as soon as two decimal marks are entered
#define PRETTY_FRACTION_ENTRY

// When entering fractions that don't fit into the 12 digit display, the first
// digits disappear to the left, just like during normal fraction display,
// instead of overflowing into the exponent field.
// This only applies if PRETTY_FRACTION_ENTRY is enabled.
#define FRACTION_ENTRY_OVERFLOW_LEFT

// Configure how to handle EEX in fraction mode and whether fraction mode is
// automatically entered or exited.
// Bits:    0 (LSB):  EEX enabled in fraction mode
//          1:        EEX exits fraction mode
//          2:        decimal fractions exit fraction mode
//          3:        fractions do NOT automatically switch to fraction mode
//        The following only applies if INCLUDE_DOUBLEDOT_FRACTIONS is enabled:
//          4:        mixed/proper fractions (a.b.c) enable proper fractions
//          5:        simple/improper fractions (a..b) enable improper fractions
#define FRACTION_MODE_INPUT 0

// Make two successive decimals a..b enter an improper fraction a/b, not a 0/b (also enables PRETTY_FRACTION_ENTRY)
//#define INCLUDE_DOUBLEDOT_FRACTIONS

// Ignore invalid fraction entry instead of treating it as an error.
// If the denominator is missing or is zero, only the integer part will
// be parsed. If a.b.0 is entered, it's interpreted as the integer a, and
// with INCLUDE_DOUBLEDOT_FRACTIONS enabled, a..0 is interpreted as zero.
// This saves approximately 252 bytes in the firmware.
//#define IGNORE_INVALID_FRACTIONS

// Show four-digit exponents instead of the HIG symbol in double precision mode
// and display only 10 digits of the mantissa if necessary.
// Values:  0 or undefined:  large exponents never displayed
//          1:               large exponents always displayed
//          2:               large exponents displayed if flag L is set
//          3:               large exponents displayed if flag L is cleared
//#define SHOW_LARGE_EXPONENT 1

// Allow entering four-digit exponents in double precision mode
//#define LARGE_EXPONENT_ENTRY

// Rules about negative exponents in single precision mode with flag D cleared
// Values: -1: Use value from register 0 (for debugging only!)
//          0: Fixed limit
//          1: Don't allow denormal numbers, without limit
//          2: Don't allow denormal numbers, with low limit
//          3: Don't allow denormal numbers, with high limit
//          4: Allow denormal numbers if all entered digits can be stored, without limit
//          5: Allow denormal numbers if all entered digits can be stored, with low limit
//          6: Allow denormal numbers if all entered digits can be stored, with high limit
//          7: Allow denormal numbers if at least one digit can be stored, without limit
//          8: Allow denormal numbers if at least one digit can be stored, with low limit
//          9: Allow denormal numbers if at least one digit can be stored, with high limit
#define SP_NEG_EXP_ENTRY_TYPE_DC 0
// Absolute value of negative exponent limit in single precision mode with flag D cleared
// -1: Use value from register 1 (for debugging only!)
#define SP_NEG_EXP_ENTRY_LIMIT_DC 383

// Rules about negative exponents in single precision mode with flag D set
// Values: -1: Use value from register 2 (for debugging only!)
//         Other values same as above.
#define SP_NEG_EXP_ENTRY_TYPE_DS 0
// Absolute value of negative exponent limit in single precision mode with flag D set
// -1: Use value from register 3 (for debugging only!)
#define SP_NEG_EXP_ENTRY_LIMIT_DS 383

// Rules about exponents when the mantissa is zero in single precision mode with flag D cleared
// Values: -1: Use value from flags 1 (MSB) and 0 (for debugging only!)
//          0: Treat a zero as if it was a number with one significant digit
//          1: Allow any exponent (within the above specified limits if any)
//          2: Allow any exponent (ignore any limit specified above)
#define SP_EXP_ENTRY_ZERO_DC 0

// Rules about exponents when the mantissa is zero in single precision mode with flag D set
// Values: -1: Use value from flag 3 (MSB) and 2 (for debugging only!)
//         Other values same as above.
#define SP_EXP_ENTRY_ZERO_DS 0

// Rules about positive exponents in single precision mode with flag D cleared
// Values: -1: Use value from register 4 (for debugging only!)
//          0: Fixed limit
//          1: Don't allow numbers that overflow to infinity, without limit
//          2: Don't allow numbers that overflow to infinity, with low limit
//          3: Don't allow numbers that overflow to infinity, with high limit
#define SP_POS_EXP_ENTRY_TYPE_DC 0
// Positive exponent limit in single precision mode with flag D cleared
// -1: Use value from register 5 (for debugging only!)
#define SP_POS_EXP_ENTRY_LIMIT_DC 384

// Rules about positive exponents in single precision mode with flag D set
// Values: -1: Use value from register 6 (for debugging only!)
//         Other values same as above.
#define SP_POS_EXP_ENTRY_TYPE_DS 0
// Positive exponent limit in single precision mode with flag D set
// -1: Use value from register 7 (for debugging only!)
#define SP_POS_EXP_ENTRY_LIMIT_DS 384

// How to handle changing the sign of exponents in single precision mode with flag D cleared
// Values: -1: Use value from register 8 (for debugging only!)
//          0: Limit exponents so that changing the sign is always legal
//          1: Changing the sign is not allowed if it would result in an illegal exponent
//          2: Changing the sign is always allowed
//          3: Extend the range of allowed exponents so changing the sign is always legal
#define SP_EXP_ENTRY_CHS_DC 2

// How to handle changing the sign of exponents in single precision mode with flag D set
// Values: -1: Use value from register 9 (for debugging only!)
//         Other values same as above.
#define SP_EXP_ENTRY_CHS_DS 2

// Rules about exponent entry in double precision mode.
// Only applies if LARGE_EXPONENT_ENTRY is enabled,
// without it the -999..999 range is unconditionally allowed.
// Values are the same as above, except -1 isn't available for debugging.
#define DP_NEG_EXP_ENTRY_TYPE_DC 0
#define DP_NEG_EXP_ENTRY_LIMIT_DC 6143
#define DP_NEG_EXP_ENTRY_TYPE_DS 0
#define DP_NEG_EXP_ENTRY_LIMIT_DS 6143

#define DP_EXP_ENTRY_ZERO_DC 0
#define DP_EXP_ENTRY_ZERO_DS 0

#define DP_POS_EXP_ENTRY_TYPE_DC 0
#define DP_POS_EXP_ENTRY_LIMIT_DC 6144
#define DP_POS_EXP_ENTRY_TYPE_DS 0
#define DP_POS_EXP_ENTRY_LIMIT_DS 6144

#define DP_EXP_ENTRY_CHS_DC 2
#define DP_EXP_ENTRY_CHS_DS 2

// Shift exponent on illegal entry instead of showing a warning
#define SHIFT_EXPONENT

// Pad exponents with spaces instead of zeros
//#define PAD_EXPONENTS_WITH_SPACES

// Do not pad exponent during number entry.
// You probably want padding if SHIFT_EXPONENT is enabled.
//#define DONT_PAD_EXPONENT_ENTRY

// Show warnings (bad digit, too long entry, too big or small number) in the
// upper line only so the number being entered is never hidden.
//#define WARNINGS_IN_UPPER_LINE_ONLY

// Chamge ALL display mode to limited significant figures mode
//#define INCLUDE_SIGFIG_MODE

// Enable Y-register display (not just for complex results)
#define INCLUDE_YREG_CODE

// Enable complex lock mode. Requires RP_PREFIX, INCLUDE_YREG_CODE, INCLUDE_YREG_HMS, and EXTRA_FLAGS
#define INCLUDE_C_LOCK

//Various complex lock mode defaults

#define DEFAULT_TO_SSIZE8 //Fix default to 8 deep stack, to allow seamless back and forth to Complex mode without losing stack content. Also see keys.c //JM3
//#define DEFAULT_TO_J // Still possible to change between I and J if this is selected
//#define DEFAULT_TO_C_LOCK // Complex Lock mode on by default
#define DEFAULT_TO_CPX_YES // No need to XEQ CPXYES to turn on complex lock 

// Enable Entry RPN (pressing Enter doesn't duplicate the x-register)
// #define ENTRY_RPN

// Y register is always displayed (cannot be turned off)
//#define YREG_ALWAYS_ON

#if !defined(YREG_ALWAYS_ON)
// Y register on by default (can be turned off)
#define YREG_ON_BY_DEFAULT
#endif

// Temporarily disable y-register display when a shift key or the CPX key is pressed 
//#define SHIFT_AND_CMPLX_SUPPRESS_YREG

// Use fractions in y-register display
#define INCLUDE_YREG_FRACT

// Don't show angles as fractions after a rectangular to polar coordinate conversion (also enables RP_PREFIX)
//#define ANGLES_NOT_SHOWN_AS_FRACTIONS

// Use HMS mode in y-register display
#define INCLUDE_YREG_HMS

// Show prefix for gradian mode when y-register is displayed (without this gradian mode is indicated by neither the RAD nor the 360 annunciators being shown)
#define SHOW_GRADIAN_PREFIX

// Show complex named registers as cX, cY, cZ, cT, cL and cJ (instead of X, Z, A, C, L and J)
//#define SHOW_COMPLEX_REGS

// Right-justify seven-segment exponent (007 or "  7" rather than "7  ")
//#define INCLUDE_RIGHT_EXP

// Rectangular - Polar y-reg prefix change:
#define RP_PREFIX

// Reduces RAM program steps by 2 and provides 4 bytes of extra flags
// in persistent RAM. About 10 of these are used by C_LOCK_MODE but there are others!
#define EXTRA_FLAGS

// h ./, in DECM mode switches E3 separator on/off (instead of chnaging radix symbol)
//#define MODIFY_K62_E3_SWITCH

// Indicate four-level stack by a '.' and eight-level stack by a ':'
//#define SHOW_STACK_SIZE

// BEG annunciators indicates BIG stack size rather than beginning of program
//#define MODIFY_BEG_SSIZE8

/*
 * This setting allows to change default mode to one of the other 2
 * possibilities. The date mode equal to DEFAULT_DATEMODE will not be
 * announced, the other 2 will be.
 * If left undefined, it defaults to DMY mode.
 * See enum date_modes for values of
 *	DATE_DMY=0,	DATE_YMD=1,	DATE_MDY=2
*/
#define DEFAULT_DATEMODE 0

/* This setting supresses the date mode display entirely if enabled.
 */
//#define NO_DATEMODE_INDICATION


//#define DEFAULT_DATE_YMD  //initialise UState.date_mode properly. No setting means DMY.
//#define DEFAULT_DATE_MDY  //initialise UState.date_mode properly. No setting means DMY.


//#define REVERSE_ARROW
// This is BIT's method, added from his repository, by JM
//Reverse angular conversions (345 only)
//The conversion arrow can be reversed by pressing it again, and the reversed arrow
//converts from the selected angular mode to the current angular mode. This is much
//faster than entering the conversion catalog, and the last remembered catalog position
//doesn't change.


/*******************************************************************/
/* Below here are the automatic defines depending on other defines */
/*******************************************************************/

#if defined(INCLUDE_C_LOCK)
#define RP_PREFIX
#define INCLUDE_YREG_CODE
#define INCLUDE_YREG_HMS
#define EXTRA_FLAGS
#endif

#if defined(INCLUDE_DOUBLEDOT_FRACTIONS)
#define PRETTY_FRACTION_ENTRY
#endif

#if defined(INCLUDE_COMPLEX_ZETA) && ! defined(INCLUDE_ZETA)
/* Complex zeta implies real zeta */
#define INCLUDE_ZETA
#endif

#if defined(INCLUDE_BERNOULLI) && ! defined(INCLUDE_ZETA)
/* Bernoulli numbers need real zeta */
#define INCLUDE_ZETA
#endif

#if defined(XROM_DIGAMMA_DOUBLE_PRECISION) && ! defined(INCLUDE_XROM_DIGAMMA)
/* Accurate digamma needs normal digamma */
#define INCLUDE_XROM_DIGAMMA
#endif

#if defined(INCLUDE_PLOTTING) || defined(INFRARED)
#define PAPER_WIDTH 166
#endif

#if defined(INCLUDE_YREG_CODE) && defined(INCLUDE_YREG_FRACT) && defined(ANGLES_NOT_SHOWN_AS_FRACTIONS)
#define RP_PREFIX
#endif

#if defined(INCLUDE_YREG_CODE) || defined(RP_PREFIX) || defined(SHOW_STACK_SIZE)
#define INCLUDE_FONT_ESCAPE
#endif

#endif  /* TINY_BUILD*/
#endif  /* FEATURES_H__ */
