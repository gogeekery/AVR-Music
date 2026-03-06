
#include <avr/io.h>
#include <avr/pgmspace.h>
#include <avr/power.h>
#include <avr/interrupt.h>
#include <util/delay.h> 

#include "data.h"

#include "songs/spiritedaway_itsumo.h"
#include "songs/totoro.h"


#define SPKR1       PD6
#define SPKR1_PORT  D
#define SPKR2       PD5
#define SPKR2_PORT  D


#define NUM_SONGS       (sizeof(songs) / sizeof(songs[0]))
#define NUM_CHANNELS    4
#define DEBOUNCE_COUNT  3


typedef struct {
    uint16_t count;
    uint16_t add;
    uint16_t env_count;
} Channel;

typedef struct {
    volatile uint8_t *port;
    volatile uint8_t *pin;
    uint8_t bit;
    uint16_t note;
    int8_t count;
} Key;


Channel channels[NUM_CHANNELS];

const uint8_t (*songs[])[][3] = {

    &spiritedaway_itsumo//,

};


bool autoMode = true;

int16_t note_index = 0;
uint16_t delay = 1;
int16_t song_index = 0;

uint8_t key_index = 0;
uint8_t channel_index = 0;
uint8_t octave = 5;

uint8_t update_count = 0;

uint8_t led_delay = 0;
uint8_t led_pwm_count = 0;
uint8_t led_intensity = 0;

int main(void) {
    // power stuff
    power_adc_disable();
    power_spi_disable();
    power_twi_disable();
    power_usart0_disable();
    power_timer2_disable();

    // set outputs
    set_output(SPKR1);
    set_output(SPKR2);

    // initialize channel variables
    for (int i = 0; i < NUM_CHANNELS; i++) {
        channels[i].count = 0;
        channels[i].add = 0;
        channels[i].env_count = 0;
    }

    note_index = -1;
    delay = 1;

    // Timer 0 for PWM of speaker pins (to simulate amplitudes)
    // Fast PWM, 16 MHz tick
    TCCR0A |= _BV(COM0A1) | _BV(COM0B1);
    TCCR0A |= _BV(WGM00) | _BV(WGM01) | _BV(WGM02);
    TCCR0B |= _BV(CS00);

    // Timer 1 for amplitude (waveform and envelope) updates
    // CTC, 16 MHz
    // 18 KHz interrupt (16 Mhz / 889)
    TCCR1B |= _BV(WGM12);
    TCCR1B |= _BV(CS10);
    TIMSK1 |= _BV(OCIE1A);
    OCR1A = 667;

    // turn on interrupts
    sei();

    while (true);

    return 0;
}

ISR(TIMER1_COMPA_vect) {
    uint16_t out = 0;
    for (int i = 0; i < NUM_CHANNELS; i++) {
        channels[i].count += channels[i].add;
        uint8_t env_index = channels[i].env_count >> 8;

        if (env_index < 128) {
            channels[i].env_count++;        
            uint16_t wave_val = waveform[channels[i].count >> 10];
            uint8_t env_val = envelope[env_index];
            out += ((wave_val * env_val) >> 8);
        }

        if (i == NUM_CHANNELS / 2 - 1) {
            OCR0A = out >> 1;
            out = 0;
        }
    }
    OCR0B = out >> 1;

    // note updating and key handler
    update_count++;
    if (update_count == 92) {  // 138 at 36khz
        update_count = 0;

        if (autoMode) {
            delay--;
            if (delay == 0) {
                note_index++;
                uint8_t note = pgm_read_byte(&(*songs[song_index])[note_index][0]);
                uint16_t data = pgm_read_word(&(*songs[song_index])[note_index][1]);
                uint8_t c_index = data & 0x3;
                delay = data >> 3;
                if (note != 0) {
                    channels[c_index].add = notes_add[note + 12 - 21];
                    channels[c_index].env_count = 0;

                    led_delay = delay / 3;

                } else {
                    if (delay == 0) {
                        // end of song
                        delay = 500;
                        note_index = 0;//-1;
                        //song_index++;
                        //if (song_index == NUM_SONGS) song_index = 0;
                    }
                }
                channels[c_index].count = 0;            
            }
        }

    }

}

// NOTE: this could be done manually in the Timer 1 interrupt (with counter)
/*ISR(TIMER2_COMPA_vect) {
}*/