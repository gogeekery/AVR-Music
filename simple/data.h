#include <avr/io.h>
#include <util/delay.h>
#include <avr/pgmspace.h>

#include <avr/pgmspace.h>
#include <util/atomic.h>

#include <stdint.h>


// Type Defines ======
// =====================

#define U8	uint8_t
#define U16	uint16_t
#define U32 uint32_t
#define U64 uint64_t

#define S8	sint8_t
#define S16	sint16_t
#define S32 sint32_t
#define S64 sint64_t

// -----------------------------


const U16 NoteData[] = {
	4545,4854,4065,4310,7692,6849,7246,6097,6493,5747,5154,5434,
	2272,2415,2032,3846,4065,3424,3623,3048,2873,3048,2564,2717,
	1136,1014,1072,1915,2032,1706,1519,1607,1432,1519,1278,
	568,602,506,536,956,851,902,758,803,716,638,676,
	284,301,253,478,506,425,451,379,358,379,319,338,
	142,126,134,238,253,212,189,200,179,189,159,
};

// -------- Note frequencies --------

#define A2		0
#define A2b		1
#define B2		2
#define B2b		3
#define C2		4
#define D2		5
#define D2b		6
#define E2		7
#define E2b		8
#define F2		9
#define G2		10
#define G2b		11
#define A3		12
#define A3b		13
#define B3		14
#define C3		15
#define C3b		16
#define D3		17
#define D3b		18
#define E3		19
#define F3		20
#define F3b		21
#define G3		22
#define G3b		23
#define A4		24
#define B4		25
#define B4b		26
#define C4		27
#define C4b		28
#define D4		29
#define E4		30
#define E4b		31
#define F4		32
#define F4b		33
#define G4		34
#define A5		35
#define A5b		36
#define B5		37
#define B5b		38
#define C5		39
#define D5		40
#define D5b		41
#define E5		42
#define E5b		43
#define F5		44
#define G5		45
#define G5b		46
#define A6		47
#define A6b		48
#define B6		49
#define C6		50
#define C6b		51
#define D6		52
#define D6b		53
#define E6		54
#define F6		55
#define F6b		56
#define G6		57
#define G6b		58
#define A7		59
#define B7		60
#define B7b		61
#define C7		62
#define C7b		63
#define D7		64
#define E7		65
#define E7b		66
#define F7		67
#define F7b		68
#define G7		69

#define PAUSE	1
#define Ps		PAUSE

#define END		0
