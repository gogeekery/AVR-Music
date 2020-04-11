
#include "data.h"


U8 Volume = 100;	// Speaker volume

U8 BPM;				// Current song's Beats per Minute;
U16 Note;			// Current position in song


volatile unsigned long timer1_millis;

ISR (TIMER1_COMPA_vect)
{
    timer1_millis++;
}
 
unsigned long millis(void) {

    unsigned long millis_return;
 
    // Ensure this cannot be disrupted
    ATOMIC_BLOCK(ATOMIC_FORCEON) {
        millis_return = timer1_millis;
    }
 
    return millis_return;
}

// X Multichannel
const U8 sngTetris[3][1024] = {
// Will be a single demensional array:

{
	160,	// BPM
	2		// Channel num
},

{
	100,	// Volume
			// Song notes (NOTE-LEN)
	E5, 4,B4, 8,C5, 8,D5, 4,C5, 8,B4, 8,
	A4, 4,A4, 8,C5, 8,E5, 4,D5, 8,C5, 8,
	B4, 4,C5, 8,D5, 4,E5, 4,
	C5, 4,A4, 4,A4, 8,A4, 8,B4, 8,C5, 8,

	D5, 4,F5, 8,A5, 4,G5, 8,F5, 8,
	E5, 4,C5, 8,E5, 4,D5, 8,C5, 8,
	B4, 4,B4, 8,C5, 8,D5, 4,E5, 4,
	C5, 4,A4, 4,A4, 4,Ps, 4,

	E5, 2,C5, 2,D5, 2,B4, 2,C5, 2,A4, 2,
	Ab4, 2,B4, 4,Ps, 4,E5, 2,C5, 2,D5, 2,
	B4, 2,C5, 4,E5, 4,A5, 2,A5b, 2,

	END		// EoS

},

{	// Metrenome effect...
	25,	// Volume

			// Song notes
	A4, 8,A4, 8,A4, 8,A4, 8,A4, 8,A4, 8,
	A4, 8,A4, 8,A4, 8,A4, 8,A4, 8,A4, 8,
	A4, 8,A4, 8,A4, 8,A4, 8,A4, 8,A4, 8,
	A4, 8,A4, 8,A4, 8,A4, 8,A4, 8,A4, 8,
	A4, 8,A4, 8,A4, 8,A4, 8,A4, 8,A4, 8,

	END		// EoS

}

};



void HoldNote(U8 BeatFrac) {
	
	// BPM is in quarter notes
	U32 Cnt = (60000L/BPM)/BeatFrac;//(32/BeatFrac)
	while (Cnt--) {
		_delay_ms(4);	// MBPS/32
	}

}


void PlayNote(U8 Note, U8 Len) {

	// Disable timer at 0Hz
	if (Note == Ps)
	{
		return;
	}

	OCR1B = VOLUME;

	ICR1H = (NoteData[Note] >> 8);
	ICR1L = NoteData[Note];

}


void PlaySong(U8 **Data) {

	BPM = Data[0][0];
	ChnlNum = Data[0][1];
	Note = 1;

	// Use tccr0 for 8 bit timer register
	// count in super small fracs of a second
	// 60000L is for 1 min to milli

	while (Data[Note]) {
		StartTime = mills();
		NoteTime = ((60000L/BPM)/(Data[Note+1]));
		while (millsecs-StartTime >= NoteTime) {
			for (Chnl = 0; Chnl < ChnlNum; ++Chnl) {
				PlayNote(Data[Chnl+1][Note]);
				_delay_ms(4);
			}
		}
		Note += 2;
	}

}


int main(void) {

	// Initialization
	DDRB = 0xFF;

	TCCR1A |= _BV(COM1B1);
	TCCR1B |= _BV(WGM13)|_BV(CS11);


	TCCR0B = (1 << WGM12) | (1 << CS11);

	OCR0AH = ((F_CPU/1000)/8) >> 8;
	ICR0AL = ((F_CPU/1000)/8);

	TIMSK0 |= (1 << OCIE1A);

	sei();

	// Loop
	for (;;) {

		PlaySong(sngTetris);
		HoldNote(1);

	}
	
	return 0;

}
