
/*
#include <avr/io.h>
#include <stdio.h>
#include <stdint.h>
*/

// Represent bool as uint8_t with 1 = true, 0 = false
typedef uint8_t bool;

enum {
	false, true
};

/*
typedef uint8_t		tU8;
typedef uint16_t	tU16;
typedef uint32_t	tU32;
typedef int8_t		tS8;
typedef int16_t		tS16;
typedef int32_t		tS32;
*/

// TODO: Cleanup/structure

// Defines for nested macros (used for pin macros below)
#define P(a, b) a ## b
#define P2(a, b) P(a, b)

// Set a pin as input or output
#define set_input(bit)              P2(DDR, bit ## _PORT) &= ~(1 << bit)
#define set_output(bit)             P2(DDR, bit ## _PORT) |= (1 << bit) 

// Enable or disable the pullup of a pin (input)
#define pullup_on(bit)              P2(PORT, bit ## _PORT) |= (1 << bit)
#define pullup_off(bit)             P2(PORT, bit ## _PORT) &= ~(1 << bit)

// Set the state of an output pin
#define set_high(bit)               P2(PORT, bit ## _PORT) |= (1 << bit)
#define set_low(bit)                P2(PORT, bit ## _PORT) &= ~(1 << bit)
#define toggle(bit)                 P2(PORT, bit ## _PORT) ^= (1 << bit)

// Check the state of an input pin
#define is_high(bit)                (P2(PIN, bit ## _PORT) & (1 << bit)) 
#define is_low(bit)                 (! (P2(PIN, bit ## _PORT) & (1 << bit))) 


// -----------------

// increment amount for different keys (piano key range) - see util/freqs.py
const uint16_t notes_add[100] = {
    75,  80,  84,  89,  95,  100,  106,  113,  119,  126,  134,  142,
    150,  159,  169,  179,  189,  200,  212,  225,  238,  253,  268,  284,
    300,  318,  337,  357,  378,  401,  425,  450,  477,  505,  535,  567,
    601,  636,  674,  714,  757,  802,  850,  900,  954,  1010,  1070,  1134,
    1201,  1273,  1349,  1429,  1514,  1604,  1699,  1800,  1907,  2021,  2141,  2268,
    2403,  2546,  2697,  2858,  3028,  3208,  3398,  3600,  3814,  4041,  4282,  4536,
    4806,  5092,  5395,  5715,  6055,  6415,  6797,  7201,  7629,  8083,  8563,  9072,
    9612,  10184,  10789,  11431,  12110,  12830,  13593,  14402,  15258,  16165,  17127,  18145,
    19224,  20367,  21578,  22861,
};

// one period of the note waveform - see util/sin.py
const uint8_t waveform[64] = {
    // sin^3
    /*128, 162, 189, 210, 225, 236, 244, 249, 252, 254, 254, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 254, 254, 252, 249, 244, 236, 225, 210, 189, 162,
    128, 94, 67, 46, 31, 20, 12, 7, 4, 2, 2, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 2, 2, 4, 7, 12, 20, 31, 46, 67, 94,*/

    // sin^2
    128, 152, 173, 191, 207, 220, 230, 238, 244, 248, 251, 253, 254, 255, 255, 255,
    255, 255, 255, 255, 254, 253, 251, 248, 244, 238, 230, 220, 207, 191, 173, 152,
    128, 104, 83, 65, 49, 36, 26, 18, 12, 8, 5, 3, 2, 1, 1, 1,
    1, 1, 1, 1, 2, 3, 5, 8, 12, 18, 26, 36, 49, 65, 83, 104,

    // sin
    /*128, 140, 153, 165, 177, 188, 199, 209, 218, 226, 234, 240, 245, 250, 253, 254,
    255, 254, 253, 250, 245, 240, 234, 226, 218, 209, 199, 188, 177, 165, 153, 140,
    128, 116, 103, 91, 79, 68, 57, 47, 38, 30, 22, 16, 11, 6, 3, 2,
    1, 2, 3, 6, 11, 16, 22, 30, 38, 47, 57, 68, 79, 91, 103, 116*/

    // square
    /*255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0*/
};


// envelope for note (scale amount)
const uint8_t envelope[128] = {
    0xFF, 0xFA, 0xF5, 0xF0, 0xEB, 0xE7, 0xE2, 0xDE, 0xD9, 0xD5, 0xD1, 0xCD, 0xC9, 0xC5, 0xC1, 0xBD, 
    0xB9, 0xB6, 0xB2, 0xAE, 0xAB, 0xA8, 0xA4, 0xA1, 0x9E, 0x9B, 0x98, 0x95, 0x92, 0x8F, 0x8C, 0x89, 
    0x86, 0x84, 0x81, 0x7F, 0x7C, 0x7A, 0x77, 0x75, 0x73, 0x70, 0x6E, 0x6C, 0x6A, 0x68, 0x66, 0x64, 
    0x62, 0x60, 0x5E, 0x5C, 0x5A, 0x58, 0x57, 0x55, 0x53, 0x52, 0x50, 0x4E, 0x4D, 0x4B, 0x4A, 0x48, 
    0x47, 0x45, 0x44, 0x43, 0x41, 0x40, 0x3F, 0x3E, 0x3C, 0x3B, 0x3A, 0x39, 0x38, 0x37, 0x36, 0x35, 
    0x33, 0x32, 0x31, 0x30, 0x30, 0x2F, 0x2E, 0x2D, 0x2C, 0x2B, 0x2A, 0x29, 0x28, 0x27, 0x26, 0x25, 
    0x23, 0x22, 0x21, 0x20, 0x1F, 0x1E, 0x1D, 0x1C, 0x1A, 0x19, 0x18, 0x17, 0x16, 0x15, 0x14, 0x13, 
    0x11, 0x10, 0x0F, 0x0E, 0x0D, 0x0C, 0x0B, 0x0A, 0x08, 0x07, 0x06, 0x05, 0x04, 0x03, 0x00, 0x00
};

