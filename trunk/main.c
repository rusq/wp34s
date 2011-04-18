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
 * This is the main module for the real hardware
 * Module written by MvC
 */
__attribute__((section(".revision"),externally_visible)) const char SvnRevision[ 12 ] = "$Rev::     $";

#include "xeq.h"
#include "display.h"
#include "lcd.h"
#include "keys.h"

#include "board.h"
#include "aic.h"
#include "supc.h"
#include "slcdc.h"

#define BACKUP_SRAM  __attribute__((section(".backup")))
#define RAM_FUNCTION __attribute__((section(".ramfunc")))
#define NO_INLINE    __attribute__((noinline))

// This helps saving precious stack space in IRQs
#define IRQ_STATIC // static

/*
 *  CPU speed settings
 */
#define SPEED_IDLE    0
#define SPEED_MEDIUM  1
#define SPEED_H_LOW_V 2
#define SPEED_HIGH    3
#define SPEED_ANNUNCIATOR LIT_EQ

/*
 *  SUPC Voltages
 */
#define SUPC_VDD_155 (0x2 <<9)
#define SUPC_VDD_165 (0x3 <<9)
#define SUPC_VDD_175 (0x4 <<9)
#define SUPC_VDD_180 (0x5 <<9)

/*
 *  BOD detector voltages
 */
#define SUPC_BOD_1_9V 0
#define SUPC_BOD_2_0V 1
#define SUPC_BOD_2_1V 2
#define SUPC_BOD_2_2V 3
#define SUPC_BOD_2_3V 4
#define SUPC_BOD_2_4V 5
#define SUPC_BOD_2_5V 6
#define SUPC_BOD_2_6V 7
#define SUPC_BOD_2_7V 8
#define SUPC_BOD_2_8V 9
#define SUPC_BOD_2_9V 10
#define SUPC_BOD_3_0V 11
#define SUPC_BOD_3_1V 12
#define SUPC_BOD_3_2V 13
#define SUPC_BOD_3_3V 14
#define SUPC_BOD_3_4V 15
#define SUPC_BOD_MAX  15

/// PLL frequency range.
#define CKGR_PLL          AT91C_CKGR_OUT_2
/// PLL startup time (in number of slow clock ticks).
#define PLLCOUNT          AT91C_CKGR_PLLCOUNT
/// PLL MUL values.
#define PLLMUL_10         304
//#define PLLMUL         999
#define PLLMUL            1144
/// PLL DIV value.
#define PLLDIV            1

/*
 *  Heart beat frequency in ms
 *  And Auto Power Down threshold
 */
#define APD_TICKS 1800 // 3 minutes
#define APD_VOLTAGE SUPC_BOD_2_1V
#define LOW_VOLTAGE SUPC_BOD_2_4V
#define DEEP_SLEEP_MARKER 0xA53C

/*
 *  Setup the persistent RAM
 */
BACKUP_SRAM TPersistentRam PersistentRam;

/*
 *  Local data
 */
volatile unsigned int ClockSpeed;
volatile unsigned short FlashWakeupTime;
volatile unsigned char SpeedSetting;
volatile unsigned char Heartbeats;
unsigned char UserHeartbeatCountDown;
volatile unsigned short int StartupTicks;
unsigned char Contrast;
volatile unsigned char InIrq;
volatile unsigned char Voltage;

/*
 *  Definitions for the keyboard scan
 */
#define KEY_ROWS_MASK 0x0000007f
#define KEY_COLS_MASK 0x0400fC00
#define KEY_COLS_SHIFT 11
#define KEY_COL5_SHIFT 10
#define KEY_ON_MASK   0x00000400
#define KEY_REPEAT_MASK 0x0000010100000000LL   // only Up and Down repeat
#define KEY_SHIFT_F_MASK 0x0000000000000800LL
#define KEY_SHIFT_G_MASK 0x0000000000001000LL
#define KEY_SHIFT_H_MASK 0x0000000000002000LL
#define KEY_REPEAT_START 20
#define KEY_REPEAT_NEXT 28
#define KEY_BUFF_SHIFT 4
#define KEY_BUFF_LEN ( 1 << KEY_BUFF_SHIFT )
#define KEY_BUFF_MASK ( KEY_BUFF_LEN - 1 )

signed char KeyBuffer[ KEY_BUFF_LEN ];
volatile char KbRead, KbWrite, KbCount;
volatile char OnKeyPressed;
short int KbRepeatCount;
volatile unsigned short Keyticks;
long long KbData, KbDebounce, KbRepeatKey;
short int BodThreshold;
short int BodTimer;

/*
 *  Tell the revision number (must not be optimised out!)
 */
const char *get_revision( void )
{
	return SvnRevision + 7;
}



/*
 *  Short wait to allow output lines to settle
 */
void short_wait( int count )
{
	if ( SpeedSetting > SPEED_MEDIUM ) {
		count *= 10;
	}
	while ( count-- ) {
		// Exclude from optimisation
		asm("");
	}
}


/*
 *  Scan the keyboard
 */
NO_INLINE void scan_keyboard( void )
{
	IRQ_STATIC int i, k;
	IRQ_STATIC unsigned char m;
	IRQ_STATIC union _ll {
		unsigned char c[ 8 ];
		unsigned long long ll;
	} keys;
	IRQ_STATIC long long last_keys;

	/*
	 *  Assume no key is down
	 */
	keys.ll = 0LL;
	OnKeyPressed = 0;

	/*
	 *  Program the PIO pins in PIOC
	 */
	// Enable clock
	AT91C_BASE_PMC->PMC_PCER = 1 << AT91C_ID_PIOC;
	// Rows as open drain output
	AT91C_BASE_PIOC->PIO_OER = KEY_ROWS_MASK;
	AT91C_BASE_PIOC->PIO_MDER = KEY_ROWS_MASK;
	
	/*
	 *  Quick check
	 *  Set all rows to low and test if any key input is low
	 */
	AT91C_BASE_PIOC->PIO_CODR = KEY_ROWS_MASK;
	short_wait( 1 );
	k = ~AT91C_BASE_PIOC->PIO_PDSR;
	AT91C_BASE_PIOC->PIO_SODR = KEY_ROWS_MASK;

	if ( 0 != ( k & KEY_COLS_MASK ) ) {
		/*
		 *  Some keys may be pressed: Assemble the input
		 */
		int r;
		for ( r = 1, i = 0; i < 7; r <<= 1, ++i ) {
			/*
			 *  Set each column to zero and read the row image.
			 *  The input is inverted on read. 1s are pressed keys now.
			 */
			AT91C_BASE_PIOC->PIO_CODR = r;
			short_wait( 1 );
			k = ~AT91C_BASE_PIOC->PIO_PDSR & KEY_COLS_MASK; 
			AT91C_BASE_PIOC->PIO_SODR = r;

			/*
			 *  Adjust the result
			 */
			OnKeyPressed = 0 != ( k & KEY_ON_MASK );
			if ( i == 6 && OnKeyPressed && StartupTicks > 5 ) {
				/*
				 *  Add ON key to bit image.
				 *  Avoid registering ON directly on power up.
				 */
				k |= 1 << KEY_COLS_SHIFT;
			}

			/*
			 *  Some bit shuffling required: Columns are on bits 11-15 & 26.
			 *  These are configurable as wake up pins, a feature we don't use here.
			 */
			k >>= KEY_COLS_SHIFT;
			k = ( k >> KEY_COL5_SHIFT ) | k;
			keys.c[ i ] = (unsigned char) k;
		}
	}

	/*
	 *  Store new images
	 */
	last_keys = KbDebounce;
	KbDebounce = KbData;
	KbData = keys.ll;

	/*
	 *  A key is newly pressed, if
	 *  a) it wasn't pressed last time we checked
	 *  b) it has the same value as the debounce value
	 */
	keys.ll &= ( ~last_keys & KbDebounce );
	
	/*
	 *  Program PIO
	 */
	// All as input
	AT91C_BASE_PIOC->PIO_ODR = KEY_ROWS_MASK | KEY_COLS_MASK;
	// Disable clock
	AT91C_BASE_PMC->PMC_PCDR = 1 << AT91C_ID_PIOC;

	/*
	 *  Handle repeating keys (arrows)
	 */
	if ( keys.ll == 0 && KbData == KbRepeatKey ) {
		/*
		 *  One of the repeating keys is still down
		 */
		++KbRepeatCount;
		if ( KbRepeatCount == KEY_REPEAT_START ) {
			// Start repeat
			keys.ll = KbRepeatKey;
		}
		else if ( KbRepeatCount == KEY_REPEAT_NEXT ) {
			// Continue repeat
			keys.ll = KbRepeatKey;
			KbRepeatCount = KEY_REPEAT_START;
		}
	}
	else {
		/*
		 *  Restart the repeat for new key
		 */
		KbRepeatCount = 0;
		KbRepeatKey = keys.ll & KEY_REPEAT_MASK;
	}

	if ( keys.ll != 0 ) {
		/*
		 *  Decode
		 */
		k = 0;
		for ( i = 0; i < 7; ++i ) {
			/*
			 *  Handle each row
			 */
			for ( m = 1; m != 0x40; m <<= 1 ) {
				/*
				 *  Handle each column
				 */
				if ( keys.c[ i ] & m ) {
					/*
					 *  First key found exits loop;
					 */
					i = 7;

					if ( State.shifts == SHIFT_N ) {
						/*
						 *  Insert Shift key if f,g,h still down
						 */
						if ( k != K_F && ( KbData & KEY_SHIFT_F_MASK ) ) {
							put_key( K_F );
						}
						else if ( k != K_G && ( KbData & KEY_SHIFT_G_MASK ) ) {
							put_key( K_G );
						}
						else if ( k != K_H && ( KbData & KEY_SHIFT_H_MASK ) ) {
							put_key( K_H );
						}
					}

					/*
					 *  Add key to buffer
					 */
					put_key( k );
					break;
				}
				/*
				 *  Try next code
				 */
				++k;
			}
		}
	}
}


/*
 *  Set the contrast
 */
void set_contrast( unsigned int contrast )
{
	if ( contrast == Contrast ) {
		/*
		 *  No change
		 */
		return;
	}

	/*
	 *  Update supply controller settings
	 */
	SUPC_SetSlcdVoltage( ( contrast - 1 ) & 0x0f );

	Contrast = contrast;
}


/*
 *  Setup the LCD controller
 */
void enable_lcd( void )
{
	if ( Contrast != 0 ) {
		/*
		 *  LCD is running
		 */
		return;
	}

	/*
	 *  Switch LCD on in supply controller and clear the RAM
	 */
	SUPC_EnableSlcd( 1 );
	SLCDC_Clear();

	/*
	 *  Configure it for 10 commons and 40 segments, non blinking
	 */
	SLCDC_Configure( 10, 40, AT91C_SLCDC_BIAS_1_3, AT91C_SLCDC_BUFTIME_8_Tsclk );
	SLCDC_SetFrameFreq( AT91C_SLCDC_PRESC_SCLK_16, AT91C_SLCDC_DIV_2 );
	SLCDC_SetDisplayMode( AT91C_SLCDC_DISPMODE_NORMAL );

	/*
	 *  Set contrast and enable the display
	 */
	set_contrast( State.contrast + 1 );
	SLCDC_Enable();
}


/*
 *  Stop the LCD controller
 */
void disable_lcd( void )
{
	/*
	 *  Disable the controller itself
	 */
	SLCDC_Disable();

	/*
	 *  Turn it off in supply controller
	 */
	SUPC_DisableSlcd();
	Contrast = 0;
}


/*
 *  Go to idle mode
 *  // Runs from SRAM to be able to turn of flash
 */
RAM_FUNCTION void go_idle( void )
{
	/*
	 *  Voltage regulator to deep mode
	 */
	SUPC_EnableDeepMode();

	/*
	 *  Disable flash memory in order to save power
	 */
	if ( 1 && SpeedSetting == SPEED_IDLE ) {
		FlashWakeupTime = (unsigned short) 2; // ( 2 + ( 3 * ClockSpeed ) / 100000 );
		SUPC_DisableFlash();
	}

	/*
	 *  Disable the processor clock and go to sleep
	 */
	AT91C_BASE_PMC->PMC_SCDR = AT91C_PMC_PCK;
	while ( ( AT91C_BASE_PMC->PMC_SCSR & AT91C_PMC_PCK ) != AT91C_PMC_PCK );
}


/*
 *  Brown out detection
 */
void detect_voltage( void )
{
	/*
	 *  Don't be active all the time
	 */
	if ( BodTimer > 0 ) {
		--BodTimer;
		return;
	}

	/*
	 *  Check if brownout state has changed
	 *  while cycling through all possible values in descending order.
	 *  Called every 25 ms.
	 */
	if ( BodThreshold == -1 ) {
		/*
		 *  First call
		 */
		BodThreshold = Voltage = SUPC_BOD_3_0V;
	}
	else {
		/*
		 *  Get current status and decrease Threshold value
		 */
		unsigned char brownout =
			0 != ( AT91C_BASE_SUPC->SUPC_SR & AT91C_SUPC_BROWNOUT );

		if ( !brownout ) {
			/*
			 *  Voltage must be above the current threshold
			 */
			Voltage = BodThreshold;

			/*
			 *  Start over
			 */
			BodThreshold = SUPC_BOD_MAX;

			/*
			 *  But wait a second before next loop with BOD disabled
			 */
			BodTimer = 400;
			AT91C_BASE_SUPC->SUPC_BOMR = 0;
		}
		else if ( BodThreshold == 0 ) {
			/*
			 *  Start over with highest threshold
			 */
			BodThreshold = SUPC_BOD_MAX;
		}
		else {
			/*
			 *  Decrease threshold and wait for next measurement
			 */
			--BodThreshold;
		}
	}
	/*
	 *  Set detector to new threshold
	 */
	AT91C_BASE_SUPC->SUPC_BOMR = AT91C_SUPC_BODSMPL_CONTINUOUS | BodThreshold;
}



/*
 *  Helpers for set_speed
 */
#define wait_for_clock() while ( ( AT91C_BASE_PMC->PMC_SR & AT91C_PMC_MCKRDY ) \
				 != AT91C_PMC_MCKRDY )
#define disable_pll()  ( AT91C_BASE_PMC->PMC_PLLR = 0x8000 )
#define disable_mclk() ( AT91C_BASE_PMC->PMC_MOR = 0x370000 )
#define enable_mclk()  ( AT91C_BASE_PMC->PMC_MOR = 0x370001 )

/*
 *  Set the master clock register in two steps
 */
static void set_mckr( int pres, int css )
{
	int mckr = AT91C_BASE_PMC->PMC_MCKR;
	if ( ( mckr & AT91C_PMC_PRES ) != pres ) {
		AT91C_BASE_PMC->PMC_MCKR = ( mckr & ~AT91C_PMC_PRES ) | pres;
		wait_for_clock();
	}
	if ( ( mckr & AT91C_PMC_CSS ) != css ) {
		AT91C_BASE_PMC->PMC_MCKR = ( mckr & ~AT91C_PMC_CSS ) | css;
		wait_for_clock();
	}
}


/*
 *  Set clock speed to one of 4 fixed values:
 *  SPEED_IDLE, SPEED_MEDIUM, SPEED_H_LOW_V, SPEED_HIGH
 *  In the idle modes, the CPU clock will be turned off.
 *
 *  Physical speeds are 2 MHZ / 64, 2 MHz, 10 MHz, 32.768 MHz
 *  Using the slow clock doesn't seem to work for unknown reasons.
 *  Therefore, the main clock with a divider is used as slowest clock.
 */
void set_speed( unsigned int speed )
{
	/*
	 *  Table of supported speeds
	 */
	static const int speeds[ SPEED_HIGH + 1 ] =
		{ 2000000 / 64 , 2000000,
		  32768 * ( 1 + PLLMUL_10 ), 32768 * ( 1 + PLLMUL ) };

	if ( speed < SPEED_MEDIUM && ( is_debug() || StartupTicks < 10 ) ) {
		/*
		 *  Allow JTAG debugging
		 */
		speed = SPEED_MEDIUM;
	}

	/*
	 *  If low voltage reduce maximum speed to 10 MHz
	 */
	if ( speed == SPEED_HIGH && Voltage <= LOW_VOLTAGE ) {
		speed = SPEED_H_LOW_V;
	}

	/*
	 *  Set new speed, called from timer interrupt
	 */
#ifdef SPEED_ANNUNCIATOR
	if ( SpeedSetting >= SPEED_MEDIUM ) {
		clr_dot( SPEED_ANNUNCIATOR );
	}
#endif
	SpeedSetting = speed;
	if ( speeds[ speed ] == ClockSpeed ) {
		/*
		 *  Invalid or no change.
		 */
		return;
	}
	ClockSpeed = speeds[ speed ];

	switch ( speed ) {

	case SPEED_IDLE:
		/*
		 *  Turn the clock almost off
		 *  System will go idle from main loop
		 */
		enable_mclk();
		set_mckr( AT91C_PMC_PRES_CLK_64, AT91C_PMC_CSS_MAIN_CLK );

		// No wait states for flash read
		AT91C_BASE_MC->MC_FMR = AT91C_MC_FWS_0FWS;

		// Turn off the unused oscillators
		disable_pll();

		// Save power
		SUPC_SetVoltageOutput( SUPC_VDD_155 );
		break;

	case SPEED_MEDIUM:
		/*
		 *  2 MHz internal RC clock
		 */
		enable_mclk();
		set_mckr( AT91C_PMC_PRES_CLK, AT91C_PMC_CSS_MAIN_CLK );

		// No wait states needed for flash read
		AT91C_BASE_MC->MC_FMR = AT91C_MC_FWS_0FWS;

		// Turn off the PLL
		disable_pll();

		// save a little power
		SUPC_SetVoltageOutput( SUPC_VDD_165 );

		break;

	case SPEED_H_LOW_V:
		/*
		 *  10 MHz PLL, used in case of low battery
		 */

	case SPEED_HIGH:
		/*
		 *  32.768 MHz PLL, derived from 32 KHz slow clock
		 */
#ifdef SPEED_ANNUNCIATOR
		set_dot( SPEED_ANNUNCIATOR );
#endif
		SUPC_SetVoltageOutput( SUPC_VDD_180 );

		if ( speed == SPEED_H_LOW_V ) {
			// No wait state necessary at 10 MHz
			AT91C_BASE_MC->MC_FMR = AT91C_MC_FWS_0FWS;
		}
		else {
			// With VDD=1.8, 1 wait state for flash reads is enough.
			AT91C_BASE_MC->MC_FMR = AT91C_MC_FWS_1FWS;
		}

		if ( ( AT91C_BASE_PMC->PMC_MCKR & AT91C_PMC_CSS )
			== AT91C_PMC_CSS_PLL_CLK )
		{
			// Intermediate switch to main clock
			enable_mclk();
			set_mckr( AT91C_PMC_PRES_CLK, AT91C_PMC_CSS_MAIN_CLK );
		}
		if ( speed == SPEED_H_LOW_V ) {
			// Initialise PLL at 10MHz
			AT91C_BASE_PMC->PMC_PLLR = CKGR_PLL | PLLCOUNT \
						| ( PLLMUL_10 << 16 ) | PLLDIV;
		}
		else {
			// Initialise PLL at 32MHz
			AT91C_BASE_PMC->PMC_PLLR = CKGR_PLL | PLLCOUNT \
						| ( PLLMUL << 16 ) | PLLDIV;
		}
		while ( ( AT91C_BASE_PMC->PMC_SR & AT91C_PMC_LOCK ) == 0 );

		// Switch to PLL
		set_mckr( AT91C_PMC_PRES_CLK, AT91C_PMC_CSS_PLL_CLK );

		// Turn off main clock
		disable_mclk();

		if ( speed == SPEED_H_LOW_V ) {
			// Save battery, we are already low
			SUPC_SetVoltageOutput( SUPC_VDD_165 );
		}
		break;
	}

	if ( speed == SPEED_IDLE ) {
		/*
		 *  Save power
		 */
		go_idle();
	}
}


/*
 *  The 100ms heartbeat, called from the PIT interrupt
 */
void user_heartbeat( void )
{
	/*
	 *  Disallow power saving for one second after reset
	 *  This is for JTAG debugging.
	 *  ON key is disabled while this value is below 5.
	 */
	if ( StartupTicks < 10 ) {
		++StartupTicks;
	}

	/*
	 *  Application ticker in 10th of seconds
	 */
	++Ticker;

	if ( State.pause ) {
		/*
		 *  The PSE handler checks this value
		 */
	        --State.pause;
	}

	/*
	 *  Allow keyboard timeout handling.
	 *  Handles Auto Power Down (APD)
	 */
	if ( Keyticks < APD_TICKS ) {
		++Keyticks;
	}

	/*
	 *  Put a dummy key code in buffer to wake up application
	 */
        put_key( K_HEARTBEAT );
}

/*
 *  The SLCDC ENDFRAME interrupt
 *  Called every 640 slow clocks (about 51 Hz)
 *  It starts at a slow clock when called in idle mode
 */
NO_INLINE void LCD_interrupt( void )
{
	InIrq = 1;

	Heartbeats = ( Heartbeats + 1 ) & 0x7f;
	if ( Heartbeats & 1 ) {
		/*
		 *  The keyboard is scanned every 25ms for debounce and repeat
		 */
		scan_keyboard();
	}

	/*
	 *  Voltage detection state machine
	 */
	detect_voltage();

	/*
	 *  Count down to next 100ms user heart beat
	 */
	if ( --UserHeartbeatCountDown == 0 ) {
		/*
		 *  Service the 100ms user heart beat
		 */
		user_heartbeat();

		/*
		 *  Schedule the next one so that there are 50 calls in 5 seconds
		 *  We need to skip 3 ticks in 128 cycles
		 */
		if ( Heartbeats ==  40 || Heartbeats ==  81 || Heartbeats == 122 ) {
			UserHeartbeatCountDown = 6;
		}
		else {
			UserHeartbeatCountDown = 5;
		}
	}
	InIrq = 0;
}


/*
 *  Common functionality for all interrupt handling
 */
RAM_FUNCTION void irq_common( void )
{
	/*
	 *  Flash memory might be disabled, turn it on again
	 */
	if ( FlashWakeupTime != 0 ) {
		SUPC_EnableFlash( FlashWakeupTime );  // minimum 60 microseconds wake up time
		FlashWakeupTime = 0;
	}

	/*
	 *  Voltage regulator to normal mode
	 */
	SUPC_DisableDeepMode();

	/*
	 *  Set speed to a minimum of 2 MHz for all irq handling.
	 */
	if ( SpeedSetting < SPEED_MEDIUM ) {
		set_speed( SPEED_MEDIUM );
	}
}


#ifdef USE_SYSTEM_IRQ
/*
 *  The system interrupt handler
 */
RAM_FUNCTION void system_irq( void )
{
	irq_common();

	/*
	 *  Since all system peripherals are tied to the same IRQ source 1
	 *  we need to check, for the source of the interrupt
	 */
	if ( PIT_GetStatus() ) {
		/*
		 *  PIT
		 */
		PIT_interrupt();
	}
	else {
		/*
		 *  Add other sources here
		 */
		// ...
	}
}
#endif


/*
 *  The SLCDC interrupt handler
 */
RAM_FUNCTION void SLCD_irq( void )
{
	irq_common();

	if ( SLCDC_GetInterruptStatus() & AT91C_SLCDC_ENDFRAME ) {
		/*
		 *  SLCDC ENDFRAME
		 */
		LCD_interrupt();
	}
}


/*
 *  Enable the interrupt sources
 */
void enable_interrupts()
{
	InIrq = 0;
	int prio = 7;

#ifdef USE_SYSTEM_IRQ
	/*
	 *  Tell the interrupt controller where to go
	 *  This is for all system peripherals
	 */
	AIC_ConfigureIT( AT91C_ID_SYS, 
		         AT91C_AIC_SRCTYPE_INT_HIGH_LEVEL | prio,
		         system_irq );
	/*
	 *  Enable IRQ 1
	 */
	AIC_EnableIT( AT91C_ID_SYS );
	--prio;
#endif

	/*
	 *  Tell the interrupt controller where to go
	 *  This is for the SLCDC
	 */
	AIC_ConfigureIT( AT91C_ID_SLCD,
		         AT91C_AIC_SRCTYPE_INT_HIGH_LEVEL | prio,
		         SLCD_irq );
	/*
	 *  Enable IRQ 11
	 */
	AIC_EnableIT( AT91C_ID_SLCD );

	/*
	 *  Use the LCD frame rate for speed independent interrupt timing
	 */
	SLCDC_EnableInterrupts( AT91C_SLCDC_ENDFRAME );
	UserHeartbeatCountDown = 5;
	// --prio;
}


/*
 *  Lock / unlock by disabling / enabling the PIT interrupt.
 *  Do nothing if called inside the interrupt handler
 */
void lock( void )
{
	if ( !InIrq ) {
		AIC_DisableIT( AT91C_ID_SYS );
	}
}

void unlock( void )
{
	if ( !InIrq ) {
		AIC_EnableIT( AT91C_ID_SYS );
	}
}


/*************************************************************************
 *  Entry points called from the application
 *************************************************************************/

/*
 *  Check if something is waiting for attention
 *  Returns the number of keys in the buffer
 */
int is_key_pressed( void )
{
        return KbCount;
}


/*
 *  Get a key
 */
int get_key( void )
{
	int k;
	if ( KbCount == 0 ) {
		/*
		 *  No key in buffer
		 */
		return -1;
	}
	lock();
	k =  KeyBuffer[ (int) KbRead ];
	KbRead = ( KbRead + 1 ) & KEY_BUFF_MASK;
	--KbCount;
	unlock();
	return k;
}


/*
 *  Add a key to the buffer
 *  Returns 0 in case of failure or number of keys in buffer
 */
int put_key( int k )
{
	if ( KbCount == KEY_BUFF_LEN ) {
		/*
		 *  Sorry, no room
		 */
		return 0;
	}

	if ( k == K_HEARTBEAT && KbCount != 0 ) {
		/*
		 *  Don't fill the buffer with heartbeats
		 */
		return 0;
	}
	lock();
	KeyBuffer[ (int) KbWrite ] = (unsigned char) k;
	KbWrite = ( KbWrite + 1 ) & KEY_BUFF_MASK;
	++KbCount;
	unlock();
	return KbCount;
}


/*
 *  Turn everything except the backup sram off
 */
void shutdown( void )
{
	/*
	 *  Ensure the RAM checksum is correct
	 */
	checksum_all();

	/*
	 *  Wait for key release
	 */
	while ( OnKeyPressed ) {
		go_idle();
	}

	/*
	 *  Turn off display gracefully
	 */
	disable_lcd();

	/*
	 *  Off we go...
	 */
	SUPC_DisableVoltageRegulator();
}


/*
 *  Serve the watch dog
 */
void watchdog( void )
{
	AT91C_BASE_WDTC->WDTC_WDCR=0xA5000001;
}


/*
 *  Is debugger active ?
 *  The flag is set via the JTAG probe
 */
#define DEBUG_FLAG ((char *)(&PersistentRam))[ 0x7ff ]

int is_debug( void )
{
	return DEBUG_FLAG == 0xA5;
}



/*
 *  Main program as seen from C
 */
int main(void)
{
#ifdef STACK_DEBUG
	/*
	 *  Fill RAM with 0x5A for debugging
	 */
	xset( (void *) 0x200200, 0x5A, 0x800 );
#endif
        /*
         * Initialise the hardware (clock, backup RAM, LCD, RTC, timer)
	 */
	set_speed( SPEED_MEDIUM );
	SUPC_EnableSram();
	Keyticks = 0;
	BodThreshold = -1;
	BodTimer = 0;
	enable_lcd();
	SUPC_EnableRtc();
	detect_voltage();
	enable_interrupts();

	/*
	 *  Force DEBUG mode for JTAG debugging
	 */
	DEBUG_FLAG = 0; // 0xA5;

	/*
	 *  Allow wake up on ON key
	 */
	AT91C_BASE_SUPC->SUPC_WUMR = (AT91C_BASE_SUPC->SUPC_WUMR)& ~(0x7u <<12);
	SUPC_SetWakeUpSources( AT91C_SUPC_FWUPEN );

	if ( DeepSleepMarker == DEEP_SLEEP_MARKER ) {
		/*
		 *  Return from deep sleep
		 */
		DeepSleepMarker = 0;
		// TODO: Check if APD time reached, update Ticker and friends
	}
	else {
		/*
		 *  Initialise the software on power on.
		 *  CRC checking the RAM is a bit slow so we speed up.
		 *  Idling will slow us down again.
		 */
		set_speed( SPEED_HIGH );
		init_34s();
	}

	/*
	 *  Wait for event and execute it
	 */
	while( 1 ) {
                int k;

		while ( !is_key_pressed() ) {
			/*
			 *  Save power if nothing in queue
			 */
			set_speed( SPEED_IDLE );
		}

		/*
		 *  Read out the keyboard queue
		 */
		k = get_key();

		if ( k != K_HEARTBEAT ) {
			/*
			 *  A real key was pressed
			 */
			if ( OnKeyPressed && k != K60 && !running() ) {
				/*
				 *  Check for special key combinations
				 */
				switch( k ) {

				case K64:
					// ON+ Increase contrast
					if ( State.contrast != 15 ) {
						++State.contrast;
					}
					break;

				case K54:
					// ON- Decrease contrast
					if ( State.contrast != 0 ) {
						--State.contrast;
					}
					break;
				}
				// No further processing
				k = -1;
			}
		}
		if ( ( k != K_HEARTBEAT && k != -1 ) || running() ) {
			/*
			 *  Increase the speed of operation
			 */
			set_speed( SPEED_HIGH );
		}

		/*
		 *  Take care of the low battery indicator
		 */
		dot( BATTERY, Voltage <= LOW_VOLTAGE );

		/*
		 *  Handle the input
		 */
		if ( k != -1 ) {
			process_keycode( k );
			if ( k != K_HEARTBEAT ) {
				Keyticks = 0;
			}
		}

		if ( Voltage <= APD_VOLTAGE || ( !running() && Keyticks >= APD_TICKS ) ) {
			/*
			 *  We have a reason to power the device off
			 */
			if ( !is_debug() && StartupTicks >= 10 ) {
				shutdown();
			}
		}

		/*
		 *  Adjust the display contrast if it has been changed
		 */
		if ( State.contrast != Contrast ) {
			/*
			 *  The saved value ranges from 0 to 15.
			 *  We use values 1 to 16 or 0 which means off
			 */
			set_contrast( State.contrast + 1 );
		}
	}
        return 0;
}


#ifdef __GNUC__
/*
 *  Get rid of any exception handler code
 */
#define VISIBLE extern __attribute__((externally_visible))
VISIBLE void __aeabi_unwind_cpp_pr0(void) {};
VISIBLE void __aeabi_unwind_cpp_pr1(void) {};
VISIBLE void __aeabi_unwind_cpp_pr2(void) {};
#endif

