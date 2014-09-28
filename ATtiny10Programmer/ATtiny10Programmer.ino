// ATtiny10Programmer.ino
//
// Brian Doyle, brian@balance-software.com, 2014.09.27
//
// This is a complete rewrite of the ATtiny4/5/9/10 programmer I found here:
// http://junkplusarduino.blogspot.com/p/attiny10-resources.html
// which appears to be the blog of "feynman17'.
//
// Credits for the original code go at least to:
//
// feynman17
// pcm1723
// Keri DuPrey
// Nat Blundell
//
// I would like to expressly thank all of the above individuals for their efforts.
// This is my first program for the Arduino and I simply would not have been able
// to learn as much as I have in such a short amount of time had it not been
// for their substantial efforts.  Sincerely, thank you.
//
// While much of this code derives directly from their efforts, it is all new,
// so any errors are mine.  That said, if you use it you take all the responsibility.
// If it eats your hardware or sets your house on fire too bad.
//
// This code is expressly licensed under the MIT License.
//
// This code is both more and less functional than the original, owing
// to the fact I haven't yet ported a few things.
//
// The original code had a few problems.  First, it didn't correctly handle
// the address field of AVRStudio .hex files.  This prevents using .org directives
// to specify where certain chunks of code or data should go.
//
// Second, the original code tended to fail arbitrarily depending on
// how much code I was trying to send over the serial port.  I know now the
// reason it failed was because of serial buffer overruns, but *not* knowing that
// initially was enough for me to start rewriting the code.  You might ask why I
// chose that route instead of trying to modify the original.  I learn better when
// I write things from scratch, but I also had to understand it all before I knew
// for sure what the problem was.  Rewriting it was the fastest way for me to do that.
//
// Now that I have a handle on things I've changed the upload logic so that it
// writes all the serial data into a buffer first and then writes to the ATtiny.
//
// My implementation currently doesn't support ATtiny 20 and 40, at least insofar
// as those chips support 2KB and 4KB flash areas respectively.  Modifying
// this code to support those chips wouldn't be too hard, but as I only have
// ATtiny10's, there's no reason yet for me to do it.
//
// What else don't I have in here at the moment?
//
// High voltage programming
// Fuse setting/clearing
// Quick reset
// Probably some other stuff
//
// How to use this?
//
// Get an Arduino.  I have an Arduino Nano which works great.
// Get an ATtiny10 (get a few, I've fried one already).
// Hook up the Arduino to the ATtiny10 as shown in this diagram:
//
//
//    Arduino                ATtiny10
//    -----------+          +----------------
//    (SS#)  D10 |----------| 6 (RESET#/PB3)
//               |          |
//    (MOSI) D11 |--[R]--+--| 1 (TPIDATA/PB0)
//               |       |  |
//    (MISO) D12 |-------+  |
//               |          |
//    (SCK)  D13 |----------| 3 (TPICLK/PB1)
//    -----------+          +----------------
//
//    -----------+          +----------------
//    (HVP)   D9 |---       | 6 (RESET#/PB3)
//               |          |
//
//	-[R]-  =  2.2kΩ resistor
//
//	2011.12.08:		Original image by pcm1723
//
//	2014.09.26 bd:  The pcm1723 image had resistors on D10, D12, and D13.
//                  Both the Arduino Nano and the ATtiny10 operate at 5V and
//                  are rated at 40mA per pin (ATtiny reference §16.1 p. 115,
//                  Nano via online) so I'm getting rid of all the resistors
//                  except the one on D11 to prevent MISO->MOSI.
//
//
// Note the HVP connection is not necessary, and isn't even supported by this
// program (to do).
// Write some assembly code in AVRStudio for the ATtiny10.
// Build it, then open the .hex file generated by AVRStudio.
// Upload this sketch to the Arduino.
// I use Sublime Text 2 with Stino on my Mac (https://github.com/Robot-Will/Stino),
// and I run AVRStudio under Windows in a VM.  You should use whatever
// Arduino environment makes you happy.
// Once the sketch is uploaded, open the Serial Monitor.
// Set the baud rate to 115,200.
// Type in a question mark (?) and hit enter to see a list of commands.
// Use the 'u' command to start programming the ATtiny10.
// Next copy the entire contents of your .hex file.
// Paste the .hex file content into the Send box of the Serial Monitor and click send.
// Your Arduino should program your ATtiny.
// I've noticed if I do all of this from Windows (even under VMware), I never
// have to pull out the USB cable and reset the Arduino.  It just programs
// the ATtiny10 then my program starts to run.  I can write it over and over again,
// just repeatly using the 'u' command and uploading.  That's nice.
// Congratulations, if you're like me you've spend an absurd amount of time getting
// an LED to blink.
//
// Some other interesting code I wrote in support of my LED color cycling project
// can be found here:
//
// http://jsfiddle.net/16bmnzn8/
//
// The section references sprinkled throughout the comments refer to the
// ATtiny10 datasheet:
//
// http://www.atmel.com/Images/Atmel-8127-AVR-8-bit-Microcontroller-ATtiny4-ATtiny5-ATtiny9-ATtiny10_Datasheet.pdf


#include <SPI.h>

#include "pins_arduino.h"

enum ArduinoPortBBits {
	kPortBSS			=	1 << 2,
	kPortBMOSI			=	1 << 3,
	kPortBMISO			=	1 << 4,
	kPortBSCK			=	1 << 5,
};

enum ATtiny4_5_9_10AddressSpace {
	// §5.2
	kTiny4IO			=	0x0000,	// 0x0000 - 0x003f
	kTiny4SRAM			=	0x0040,	// 0x0040 - 0x005f
	kTiny4Reserved0		=	0x0060,	// 0x0060 - 0x3eff
	kTiny4NVMLock		=	0x3f00,	// 0x3f00 - 0x3f01
	kTiny4Reserved1		=	0x3f02,	// 0x3f02 - 0x3f3f
	kTiny4Configuration	=	0x3f40, // 0x3f40 - 0x3f41
	kTiny4Reserved2		=	0x3f42,	// 0x3f42 - 0x3f7f
	kTiny4Calibration	=	0x3f80,	// 0x3f80 - 0x3f81
	kTiny4Reserved3		=	0x3f82,	// 0x3f82 - 0x3fbf
	kTiny4DeviceId		=	0x3fc0, // 0x3fc0 - 0x3fc3
	kTiny4Reserved4		=	0x3fc4,	// 0x3fc4 - 0x3fff
	kTiny4Flash			=	0x4000, // 0x4000 - 0x41ff (tiny 4/5), 0x4000 - 0x43ff (tiny 9/10)
};

enum ATtinyType {
	kATtinyUnknown,

	kATtiny4			=	4,
	kATtiny5			=	5,
	kATtiny9			=	9,
	kATtiny10			=	10,
	kATtiny20			=	20,
	kATtiny40			=	40,
};

enum IntelHexFileType {
	kIHTypeData			=	0,
	kIHTypeEOF			=	1,
	kIHTypeExtSegAddr	=	2,
	kIHTypeStartSegAddr	=	3,
	kIHTypeExtLinAddr	=	4,
	kIHTypeStartLinAddr	=	5,
};

enum TPICSS {
	TPIIR				=	0x0f,	// §14.7.1
	TPIPCR				=	0x02,	// §14.7.2
	TPISR				=	0x00,	// §14.7.3

	GT0					=	0x01,	// §14.7.2 guard time bit 0
	GT1					=	0x02,	// §14.7.2 guard time bit 1
	GT2					=	0x04,	// §14.7.2 guard time bit 2
	NVMDisable			=	0x00,	// §14.7.3
	NVMEnable			=	0x02,	// §14.7.3
	TPIIdentification	=	0x80,	// §14.7.1
};

// §14.5
enum TPIInstruction {
	SIN					=	0x10,	// serial in from i/o space
	SKEY				=	0xe0,	// serial key signaling
	SLD					=	0x20,	// serial load from data space
	SLD_postIncrement	=	0x24,	// serial load from data space with post increment
	SLDCS				=	0x80,	// serial load from control and status space
	SOUT				=	0x90,	// serial out to i/o space
	SST					=	0x60,	// serial store to data space
	SST_postIncrement	=	0x64,	// serial store to data space with post increment
	SSTCS				=	0xc0,	// serial store to control and status space
	SSTPR_high			=	0x69,	// serial store to pointer register high byte
	SSTPR_low			=	0x68,	// serial store to pointer register low byte
};

enum TPINVM {
	NVMCMD				=	0x33,	// §15.7.2
	NVMCSR				=	0x32,	// §15.7.1

	NVMBusy				=	0x80,	// §15.7.1
	NVMChipErase		=	0x10,	// §15.7.2
	NVMNop				=	0x00,	// §15.7.2
	NVMSectionErase		=	0x14,	// §15.7.2
	NVMWordWrite		=	0x1d,	// §15.7.2
};

ATtinyType				gATtinyType;
uint16_t				gTPIPointer;

bool tpi_memory_read( uint8_t &data, bool postIncrement = true, bool updateGTPIPointer = true );
boolean tpi_memory_write( uint8_t data, bool postIncrement = true, bool updateGTPIPointer = true );


uint16_t get_free_memory() {
  uint16_t				v; 
  extern uint16_t		__heap_start, *__brkval;

  return (uint16_t) &v - ( __brkval ? (uint16_t) &__heap_start : (uint16_t) __brkval );
}

void setup() {
	tristate_arduino_spi();		// so we can test the tiny at power on

	Serial.begin( 115200 );
}

void loop() {
	Serial.print( F( "Command [d,e,i,m,u,v,?] > " ) );

	for ( ;; ) {
		switch ( serial_read() ) {
			case 'd': case 'D':		tpi_dump_memory();			break;
			case 'e': case 'E':		tpi_chip_erase();			break;
			case 'i': case 'I':		tpi_identify_device();		break;
			case 'm': case 'M':		print_free_memory();		break;
			case 'u': case 'U':		upload_program();			break;
			case 'v': case 'V':		print_version();			break;

			case ' ': case '\t': case '\r': case '\n':			continue;

			default: {
				Serial.println( F( "Commands:" ) );
				Serial.println( F( "D: Dump memory" ) );
				Serial.println( F( "E: Erase memory" ) );
				Serial.println( F( "I: Identify device" ) );
				Serial.println( F( "M: Print free memory" ) );
				Serial.println( F( "U: Upload program" ) );
				Serial.println( F( "V: Print version" ) );
				Serial.println( F( "?: Help" ) );
			} break;
		}

		break;
	}
}

void print_free_memory() {
	Serial.print( F( "Free memory: " ) );
	Serial.print( get_free_memory() );
	Serial.println( F( " bytes" ) );
}

void print_hex( uint16_t value, int8_t nibbles ) {
	while ( --nibbles > 0 ) {
		if ( value >> nibbles * 4 & 0x0f ) break;		// if msb is != 0

		Serial.print( F( "0" ) );
	}

	Serial.print( value, HEX );
}

void print_version() {
	Serial.println( F( "Version 1.0" ) );
}

void tristate_arduino_spi() {
	const uint8_t		kPortBTriState = ~( kPortBSS | kPortBMOSI | kPortBMISO | kPortBSCK );

	DDRB &= kPortBTriState;								// tri-state SPI so ATtiny can be tested
	PORTB &= kPortBTriState;
}

// I had tons of failures trying to write the ATtiny10 while
// reading a hex file from the Serial Monitor.  The problem is
// that the serial buffer gets data faster than the TPI code
// can write it, so the serial buffer overflows and the write
// fails.  To deal with this, I now read the hex file from the
// serial port quickly into a buffer, decoding it only as much
// as is necessary to keep track of the data, the address to
// write to, and the length of data to write.  After I've acquired
// the whole file I then write it to the ATtiny.
//
// Here's a little program I wrote to color cycle an RGB LED with
// an ATtiny10.  It's hooked up like so:
//
// LED_R ---[R]--- ATtiny10_PB0
// LED_G ---[R]--- ATtiny10_PB1
// LED_B ---[R]--- ATtiny10_PB2
//
// :020000020000FC
// :1000000000E00EBF0FE50DBF00E018ED1CBF06BFFE
// :1000100007E001B9000000E002B9B1E4A0E0D1E4DA
// :10002000C5E5F1E4E6EA0DD020E030E011D03395EB
// :10003000363419F4203009F4F6CF3030B9F723956F
// :10004000F5CF7C9188819081A395C395E395089520
// :100050000F931F9301E010E0701708F011608017F4
// :1000600008F01260901708F0146012B9039599F720
// :060070001F910F9108959D
// :100100007F8285888B8F9295989B9EA1A4A7AAAD8C
// :10011000B0B3B6B8BBBEC1C3C6C8CBCDD0D2D5D79D
// :10012000D9DBDDE0E2E4E5E7E9EBECEEEFF1F2F458
// :10013000F5F6F7F8F9FAFBFBFCFDFDFEFEFEFEFE10
// :10014000FFFEFEFEFEFEFDFDFCFBFBFAF9F8F7F6F6
// :10015000F5F4F2F1EFEEECEBE9E7E5E4E2E0DDDB0C
// :10016000D9D7D5D2D0CDCBC8C6C3C1BEBBB8B6B324
// :10017000B0ADAAA7A4A19E9B9895928F8B888582EB
// :100180007F7C7976736F6C696663605D5A575451F2
// :100190004E4B484643403D3B383633312E2C2927C1
// :1001A0002523211E1C1A1917151312100F0D0C0AE6
// :1001B000090807060504030302010100000000000E
// :1001C0000000000000000101020303040506070807
// :1001D000090A0C0D0F1012131517191A1C1E2123D2
// :1001E0002527292C2E313336383B3D404346484B9A
// :1001F0004E5154575A5D606366696C6F7376797CB3
// :00000001FF

void upload_program() {
	uint16_t			address, w;
	uint8_t *			buffer = NULL, length, *p, *q, type;
	bool				success = false;

	// The page size of ATtiny4,5,9,10 is 16 bytes (§15.3.2).
	// AVRStudio generates Intel .hex files with 16-byte data rows
	// in most cases, though they can be smaller.  The buffer allocation
	// here is wide enough to receive 64 rows of 16 bytes each, plus
	// a length byte and an address word, which would be what AVRStudio
	// should output if all 1024 bytes of the ATtiny9,10 memory is filled.
	// I don't expect this to overrun in most cases but it is possible if
	// somehow the .hex file contains lots of smaller-than-page-sized rows.
	// In that case I recommend writing a small python script or similar
	// to preprocess the .hex file and convert it into longer rows.
	// The Intel hex file format is here:
	// https://en.wikipedia.org/wiki/Intel_HEX

	if ( ! ( p = buffer = (uint8_t *) malloc( 64 * ( 1 + 2 + 16 ) ) ) ) {
		Serial.println( "Could not allocate enough memory" );
		goto done;
	}

	Serial.println( F( "Upload hex file content to serial monitor" ) );

	do {
		while ( serial_read() != ':' ) ;

		length = serial_decode_byte();
		address = (uint16_t) serial_decode_byte() << 8;
		address |= serial_decode_byte();
		type = serial_decode_byte();

		if ( length ) {
			if ( type == kIHTypeData ) {
				*p++ = length;
				*p++ = address >> 8 & 0xff;
				*p++ = address & 0xff;

				while ( length-- ) *p++ = serial_decode_byte();
			} else {
				while ( length-- ) serial_decode_byte();
			}
		}

		serial_decode_byte();			// consume checksum
	} while ( type != kIHTypeEOF );

	Serial.println( F( "Upload complete" ) );

	tpi_chip_erase();

	Serial.println( F( "Writing flash" ) );

	tpi_enable();

	for ( q = buffer; q < p; ) {
		length = *q++;
		address = (uint16_t) *q++ << 8;
		address |= *q++;

		print_hex( length, 2 );
		Serial.print( F( " ") );
		print_hex( address, 4 );
		Serial.print( F( " ") );

		address |= kTiny4Flash;

		if ( address & 1 ) {
			address &= ~1;
			print_hex( *q, 2 );
			w = 0xff00 | *q++;
			if ( ! tpi_memory_write_word( address, w ) ) goto done;
			address += 2; 
			--length;
		}
		for ( ; length > 1; address += 2, length -= 2 ) {
			print_hex( *q, 2 );
			w = (uint16_t) *q++ << 8;
			print_hex( *q, 2 );
			w |= *q++;
			if ( ! tpi_memory_write_word( address, w ) ) goto done;
		}
		if ( length ) {
			print_hex( *q, 2 );
			w = (uint16_t) *q++ << 8 | 0xffu;
			if ( ! tpi_memory_write_word( address, w ) ) goto done;
		}

		Serial.println( F( " OK" ) );
	}

	Serial.println( F( "Done" ) );

	success = true;

done:

	if ( ! success ) Serial.println( F( "Upload to ATtiny failed" ) );

	if ( buffer ) free( buffer );

	tpi_disable();
}

uint8_t serial_decode_byte() {
	uint8_t				b0, b1;

	b0 = serial_read();
	b1 = serial_read();

	return unhex( b0, b1 );
}

uint8_t serial_read() {
	while ( Serial.available() < 1 ) ;

	return Serial.read();
}

// converts two chars hexadecimal characters to the byte they represent
uint8_t unhex( uint8_t high, uint8_t low ) {
	uint8_t				b;

	b = ( high >= '0' && high <= '9' ? high - '0' : ( high & ~0x20 ) - 'A' + 0xa ) << 4;
	b |= low >= '0' && low <= '9' ? low - '0' : ( low & ~0x20 ) - 'A' + 0xa;

	return b;
}

// ATtiny TPI (Tiny Programming Interface) (§14 pp. 95+)

boolean tpi_chip_erase() {
	tpi_enable();

	// §15.4.3.1
	// 1. write the CHIP_ERASE command to the NVMCMD register:
	tpi_io_write( NVMCMD, NVMChipErase );

	// 2. Start the erase operation by writing a dummy byte to
	// the high byte of any word location inside the code section
	tpi_set_pointer( kTiny4Flash + 1 );
	tpi_memory_write( 0x1 );

	// 3. Wait until the NVMBSY bit has been cleared
	if ( tpi_nvm_wait() ) Serial.println( F( "Chip erased" ) );
	else Serial.println( F( "Chip erase failed" ) );

	tpi_disable();
}

bool tpi_css_read( uint8_t address, uint8_t &data ) {
	// §14.5.6
	tpi_serial_write( SLDCS | address );

	return tpi_serial_read( data );
}

void tpi_css_write( uint8_t address, uint8_t data ) {
	// §14.5.7
	tpi_serial_write( SSTCS | address );
	tpi_serial_write( data );
}

void tpi_disable() {
	tpi_css_write( TPISR, NVMDisable );

	digitalWrite( SS, HIGH );							// release RESET
	delay( 1 );											// tRST min = 400 ns @ Vcc = 5 V

	SPI.end();

	tristate_arduino_spi();
}

void tpi_dump_memory() {
	uint8_t				b, i;
	uint16_t			flashEnd;

	tpi_identify_device();

	tpi_enable();
	tpi_set_pointer( kTiny4IO );

	switch ( gATtinyType ) {
		case kATtiny4:
		case kATtiny5:		flashEnd = 0x0200;		break;
		case kATtiny9:
		case kATtiny10:		flashEnd = 0x0400;		break;
		case kATtiny20:		flashEnd = 0x0800;		break;
		case kATtiny40:		flashEnd = 0x1000;		break;
	}

	flashEnd += kTiny4Flash;

	while ( gTPIPointer < flashEnd ) {
		if ( ! tpi_memory_read( b, true, false ) ) {
			Serial.println( F( "Failed to read memory" ) );
			goto done;
		}

		// read all the memory, but only print
		// the register, SRAM, config and signature memory
		if (
			gTPIPointer >= kTiny4IO && gTPIPointer < kTiny4Reserved0 ||
			gTPIPointer >= kTiny4NVMLock && gTPIPointer < kTiny4Reserved1 ||
			gTPIPointer >= kTiny4Configuration && gTPIPointer < kTiny4Reserved2 ||
			gTPIPointer >= kTiny4Calibration && gTPIPointer < kTiny4Reserved3 ||
			gTPIPointer >= kTiny4DeviceId && gTPIPointer < kTiny4Reserved4 ||
			gTPIPointer >= kTiny4Flash && gTPIPointer < flashEnd )
		{
			if (
				gTPIPointer == kTiny4IO ||
				gTPIPointer == kTiny4NVMLock ||
				gTPIPointer == kTiny4Configuration ||
				gTPIPointer == kTiny4Calibration ||
				gTPIPointer == kTiny4DeviceId ||
				gTPIPointer == kTiny4Flash )
			{
				if ( gTPIPointer != kTiny4IO ) {
					Serial.println( F( "" ) );
					Serial.println( F( "" ) );
				}

				if ( gTPIPointer == kTiny4IO ) Serial.println( F( "Registers, SRAM:" ) );
				else if ( gTPIPointer == kTiny4NVMLock ) Serial.println( F( "NVM Lock:" ) );
				else if ( gTPIPointer == kTiny4Configuration ) Serial.println( F( "Configuration:" ) );
				else if ( gTPIPointer == kTiny4Calibration ) Serial.println( F( "Calibration:" ) );
				else if ( gTPIPointer == kTiny4DeviceId ) Serial.println( F( "Device ID:" ) );
				else if ( gTPIPointer == kTiny4Flash ) Serial.println( F( "Flash:" ) );

				Serial.println( F( "" ) );

				for ( i = 0; i < 5; ++i ) Serial.print( F( " " ) );
				for ( i = 0; i < 16; ++i ) {
					Serial.print( F( " +" ) );
					Serial.print( i, HEX );
				}
			}
			if ( ! ( gTPIPointer & 0x000f ) ) {
				Serial.println( F( "" ) );
				print_hex( gTPIPointer, 4 );
				Serial.print( F( ": " ) );
			}
			print_hex( b, 2 );
			Serial.print( F( " " ) );
		}

		++gTPIPointer;

		if ( gTPIPointer == kTiny4Reserved0 ) tpi_set_pointer( kTiny4NVMLock );
	}

	Serial.println( F( "" ) );

done:

	tpi_disable();
}

bool tpi_enable() {
	uint8_t				data;
	uint64_t			nvmKey = 0x1289ab45cdd888ffull;	// §14.6

	// §14.3.1, p. 96, enable TPI

	SPI.begin();
	SPI.setBitOrder( LSBFIRST );
	SPI.setDataMode( SPI_MODE0 );
	SPI.setClockDivider( SPI_CLOCK_DIV16 );				// §16.8 2MHz maximum serial programming frequency
														// I'm using Arduino Nano (16MHz), so set serial clock to 1MHz

	digitalWrite( SS, LOW );							// assert RESET on tiny
	delay( 1 );											// §16.5 tRST min = 400ns @ Vcc = 5 V

	SPI.transfer( 0xff );								// activate TPI by emitting
	SPI.transfer( 0xff );								// 16 or more pulses on TPICLK
														// while holding TPIDATA to "1"

	// §14.7.2
	tpi_css_write( TPIPCR, GT2 );						// TPIPCR, guard time = 8 idle bits (default=128)

	tpi_serial_write( SKEY );							// enable NVM
	do { tpi_serial_write( nvmKey & 0xff ); } while ( nvmKey >>= 8 );
	
	// §14.7.3 wait for NVM to enable
	do {
		if ( ! tpi_css_read( TPISR, data ) ) {
			Serial.println( F( "Failed to enable TPI" ) );
			return false;
		}
	} while ( ! data );

	return true;
}

bool tpi_identify_device() {
	uint8_t				b0, b1, b2;

	gATtinyType = kATtinyUnknown;

	tpi_enable();
	tpi_set_pointer( kTiny4DeviceId );

	if ( ! tpi_memory_read( b0 ) ) goto done;
	if ( ! tpi_memory_read( b1 ) ) goto done;
	if ( ! tpi_memory_read( b2 ) ) goto done;

	// §15.3.4
	if ( b0 == 0x1e ) {
		switch ( b1 ) {
			case 0x8f: {
				switch ( b2 ) {
					case 0x0a:		gATtinyType = kATtiny4;		break;
					case 0x09:		gATtinyType = kATtiny5;		break;
				}
			} break;

			case 0x90: {
				switch ( b2 ) {
					case 0x08:		gATtinyType = kATtiny9;		break;
					case 0x03:		gATtinyType = kATtiny10;	break;
				}
			} break;

			case 0x91: {
				switch ( b2 ) {
					case 0x0f:		gATtinyType = kATtiny20;	break;
				}
			} break;

			case 0x92: {
				switch ( b2 ) {
					case 0x0e:		gATtinyType = kATtiny40;	break;
				}
			} break;
		}
	}

	if ( gATtinyType ) {
		Serial.print( F( "ATtiny" ) );
		Serial.print( gATtinyType );
		Serial.println( F( " connected" ) );
	} else {
		Serial.println( F( "Unable to identify device" ) );
	}

done:

	tpi_disable();

	return gATtinyType != kATtinyUnknown;
}

boolean tpi_io_read( uint8_t address, uint8_t &data ) {
	// §14.5.4 SIN 0b0aa1aaaa replace a with 6 address bits
	tpi_serial_write( SIN | ( address & 0x30 ) << 1 | address & 0x0f );

	return tpi_serial_read( data );
}

void tpi_io_write( uint8_t address, uint8_t data ) {
	// §14.5.5 SOUT 0b1aa1aaaa replace a with 6 address bits
	tpi_serial_write( SOUT | ( address & 0x30 ) << 1 | address & 0x0f );
	tpi_serial_write( data );
}

// read memory indirectly.  must call tpi_set_pointer() first
bool tpi_memory_read( uint8_t &data, bool postIncrement, bool updateGTPIPointer ) {
	tpi_serial_write( postIncrement ? SLD_postIncrement : SLD );

	if ( tpi_serial_read( data ) ) {
		if ( postIncrement && updateGTPIPointer ) ++gTPIPointer;
		return true;
	}

	return false;
}

// write memory indirectly.  must call tpi_set_pointer() first
boolean tpi_memory_write( uint8_t data, bool postIncrement, bool updateGTPIPointer ) {
	tpi_serial_write( postIncrement ? SST_postIncrement : SST );
	tpi_serial_write( data );

	if ( postIncrement && updateGTPIPointer ) ++gTPIPointer;
}

bool tpi_memory_write_word( uint16_t address, uint16_t data ) {
	tpi_io_write( NVMCMD, NVMWordWrite );

	tpi_set_pointer( address );
	tpi_memory_write( data >> 8 & 0xff );
	tpi_memory_write( data & 0xff );

	if ( ! tpi_nvm_wait() ) {
		Serial.println( F( "tpi_memory_write_word() failed" ) );
		return false;
	}

	return true;
}

bool tpi_nvm_wait() {
	uint8_t				b;

	do {
		if ( ! tpi_io_read( NVMCSR, b ) ) return false;
	} while ( b & NVMBusy );

	return true;
}

// read TPI 12-bit format byte data via SPI
// 2 bytes (16 clocks) or 3 bytes (24 clocks)
bool tpi_serial_read( uint8_t &data ) {
	uint8_t				b0, b1;

	// keep transmitting high(idle) while waiting for a start bit
	while ( ( b0 = SPI.transfer( 0xff ) ) == 0xff ) ;

	// get (partial) data bits
	b1 = SPI.transfer( 0xff );

	// if the first byte(b0) contains less than 4 data bits
	// we need to get a third byte to get the parity and stop bits
	if ( ( b0 & 0x0f ) == 0x0f ) SPI.transfer( 0xff );

	// b0 should hold only idle and start bits = 0b01111111
	// if it doesn't, shift (b1|b0) left until it does.
	// bd 2014.09.26: b0 == 0xff will cause an infinite loop, so test for it
	if ( b0 == 0xff ) {
		Serial.println( F( "tpi_serial_read() got bad data" ) );
		return false;
	}
	while ( b0 != 0x7f ) {
		b1 <<= 1;
		if ( b0 & 0x80 ) b1 |= 1;
		b0 <<= 1;
		b0 |= 1;									// fill with idle bit
	}

	// now the data byte is stored in b1
	data = b1;

	return true;
}

// Send a byte in one TPI frame (12 bits: 1 start + 8 data + 1 parity + 2 stop)
// using 2 SPI data bytes (2 x 8 = 16 clocks) with 4 extra idle bits
void tpi_serial_write( uint8_t data ) {
	// compute parity bit
	uint8_t				parity = data;

	// §14.3.4 p. 97
	// even parity function is:
	// d0 ^ d1 ^ d2 ^ d3 ^ d4 ^ d5 ^ d6 ^ d7 ^ 0

	// due to commutativity, this becomes:
	// d0 ^ d4 ^ d1 ^ d5 ^ d2 ^ d6 ^ d3 ^ d7 ^ 0
	// due to associativity, this becomes:
	// ( d0 ^ d4 ) ^ ( d1 ^ d5 ) ^ ( d2 ^ d6 ) ^ ( d3 ^ d7 ) ^ 0
	parity ^= ( parity >> 4 );		// b[7:4] ^ b[3:0]
	// which yields:
	// d0 ^ d1 ^ d2 ^ d3 ^ 0
	// due to commutativity:
	// d0 ^ d2 ^ d1 ^ d3 ^ 0
	// due to associativity:
	// ( d0 ^ d2 ) ^ ( d1 ^ d3 ) ^ 0
	parity ^= ( parity >> 2 );		// b[3:2] ^ b[1:0]
	// which yields:
	// d0 ^ d1 ^ 0
	// due to associativity:
	// ( d0 ^ d1 ) ^ 0
	parity ^= ( parity >> 1 );		// b[1] ^ b[0]
	// which yields:
	// d0 ^ 0
	// which yields parity bit in d0

	// TPI transmits data in LSBfirst mode and idle is high
	// 2 idle(high) + 1 start bit(low) + data[4:0]
	SPI.transfer( 0x03 | data << 3 );

	// data[7:5] + 1 parity + 2 stop bits(high) + 2 idle(high)
	SPI.transfer( 0xf0 | parity << 3 | data >> 5 );
}

void tpi_set_pointer( unsigned short address ) {
	// §14.5.3
	tpi_serial_write( SSTPR_low );
	tpi_serial_write( address & 0xff );
	tpi_serial_write( SSTPR_high );
	tpi_serial_write( address >> 8 & 0xff );

	gTPIPointer = address;
}
