##AVR Chiptune Player
A compact polyphonic chiptune player originally written for AVR microcontrollers and reproduced on Windows. Songs are stored as compact three byte events and synthesized in real time using a small waveform and envelope. Music can be generated froma midi file with an included tool (MIDItoC.c)

#Circuit
[circuit/ATMEGA328P.PNG]
 


#Key components

MCU: ATmega328P recommended
Speaker: 8 ohm or 16 ohm rated for audio power
Driver: small audio amplifier module or transistor per channel
Coupling capacitor: 100 microfarad electrolytic for DC blocking
Resistors: 220 ohm gate or base resistors for transistor drivers
Decoupling: 0.1 microfarad ceramic and 10 microfarad bulk capacitor near MCU VCC and AVCC
Optional: LEDs for status and push buttons for song and tempo control


#Wiring summary

MCU PWM pin to gate or base resistor to transistor to speaker to ground
Speaker other terminal to amplifier output or VCC depending on driver topology
AVCC tied to VCC with decoupling capacitors close to MCU pins
Common ground for MCU and driver


#Design notes

Ideally, use a transistor or amplifier to avoid excessive current draw on the MCU.

Tie AVCC to VCC and add a 10 microfarad electrolytic plus a 0.1 microfarad ceramic cap near the MCU.

For stereo separation use two independent driver stages and two speakers or two amplifier channels.



#Data formats

Event triplet: note data_lo data_hi where data = (delay << 3) | (channel & 0x3) and delay is in AVR note update units

Waveform: 64 entry table used to synthesize one period of the waveform

Envelope: 128 entry amplitude envelope applied per voice


#Build and Flash

avr-gcc -mmcu=atmega328p -Os -DF_CPU=16000000UL -o main.elf avr_main.c
avr-objcopy -O ihex main.elf main.hex

Flash the MCU:
avrdude -c <programmer> -p m328p -U flash:w:main.hex:i

Build the Windows player with MinGW or MSVC:
gcc -O2 -o win_player.exe win_player.c -lwinmm


Use the included generator to convert MIDI to AVR headers

MIDItoC input_song.mid song_name
