// win_player.c
// Compile: cl /O2 /MD /Fe:avr_player.exe win_main.c winmm.lib
// or: gcc -O2 -o avr_player.exe win_main.c -lwinmm


#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmsystem.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#pragma comment(lib, "winmm.lib")

#include "data.h"


// Include songs:
#include "songs/spiritedaway_itsumo.h"
#include "songs/castle.h"

const uint8_t (*songs[])[][3] = {
    //&spiritedaway_itsumo
    &castle
};



// Audio constants matching AVR Timer1 ISR tick
#define SAMPLE_RATE 23952   // 16MHz / 668 ~= 23952 Hz
#define CHANNELS 2
#define BITS_PER_SAMPLE 16
#define BLOCK_ALIGN (CHANNELS * (BITS_PER_SAMPLE/8))

// AVR constants
#define NUM_CHANNELS 4

typedef struct {
    uint32_t count;      // phase (use 32-bit for safety)
    uint16_t add;        // increment (from notes_add)
    uint32_t env_count;  // envelope counter
} ChannelState;

static ChannelState ch[NUM_CHANNELS];
static int32_t update_count = 0;
static int32_t note_index = -1;
static uint16_t delay_ticks = 1;
static int32_t song_index = 0;
static int running = 1;

// Forward declarations for song data from your headers:
// - notes_add[] (uint16_t array)
// - waveform[64] (uint8_t array)
// - envelope[128] (uint8_t array)
// - songs[] (pointer to arrays of triplets)
// These are provided by data.h and the song header you included.

// Helper: read 16-bit little-endian word from song triplet bytes 1..2
static inline uint16_t read_song_word(const uint8_t triplet[3]) {
    return (uint16_t)triplet[1] | ((uint16_t)triplet[2] << 8);
}

// Audio generation: fill buffer with frames (stereo 16-bit)
static void generate_audio(int16_t *outBuf, int frames) {
    for (int f = 0; f < frames; ++f) {
        uint32_t sum_left = 0;
        uint32_t sum_right = 0;

        // For each AVR channel
        for (int i = 0; i < NUM_CHANNELS; ++i) {
            ch[i].count += ch[i].add; // fixed-point phase increment
            uint8_t wave_val = waveform[(ch[i].count >> 10) & 63];
            uint32_t env_index = ch[i].env_count >> 8;
            uint8_t env_val = 0;
            if (env_index < 128) {
                env_val = envelope[env_index];
                ch[i].env_count++;
            } else {
                env_val = 0;
            }
            uint32_t contrib = ((uint32_t)wave_val * (uint32_t)env_val) >> 8; // 0..255

            if (i < NUM_CHANNELS / 2) {
                sum_left += contrib;
            } else {
                sum_right += contrib;
            }

            // After two channels AVR does: OCR0A = out >> 1; out = 0;
            // We'll apply the same >>1 later when converting to 8-bit range.
        }

        // Match AVR mixing: sum of two channels then >>1 to bring back to 0..255
        uint32_t left8 = (sum_left >> 1) & 0xFF;   // 0..255
        uint32_t right8 = (sum_right >> 1) & 0xFF; // 0..255

        // Convert 0..255 to signed 16-bit PCM roughly matching AVR PWM center:
        // map 128 -> 0, scale by 256: (value - 128) * 256
        int32_t sampleL = ((int32_t)left8 - 128) * 256;
        int32_t sampleR = ((int32_t)right8 - 128) * 256;

        // Clamp to int16 range
        if (sampleL > 32767) sampleL = 32767;
        if (sampleL < -32768) sampleL = -32768;
        if (sampleR > 32767) sampleR = 32767;
        if (sampleR < -32768) sampleR = -32768;

        outBuf[2 * f] = (int16_t)sampleL;
        outBuf[2 * f + 1] = (int16_t)sampleR;

        // AVR note update logic runs every 92 ticks
        update_count++;
        if (update_count == 92) {
            update_count = 0;
            // autoMode is true in AVR; replicate behavior
            if (1) {
                if (delay_ticks > 0) delay_ticks--;
                if (delay_ticks == 0) {
                    note_index++;
                    // read triplet from song
                    const uint8_t (*songptr)[3] = *songs[song_index];
                    const uint8_t *trip = songptr[note_index];
                    uint8_t note = trip[0];
                    uint16_t data = read_song_word(trip);
                    uint8_t c_index = data & 0x3;
                    delay_ticks = data >> 3;
                    if (note != 0) {
                        // replicate AVR index: notes_add[note + 12 - 21]
                        int idx = note + 12 - 21;
                        if (idx < 0) idx = 0;
                        if (idx >= (int)(sizeof(notes_add)/sizeof(notes_add[0]))) idx = (int)(sizeof(notes_add)/sizeof(notes_add[0])) - 1;
                        ch[c_index].add = notes_add[idx];
                        ch[c_index].env_count = 0;
                        ch[c_index].count = 0;
                    } else {
                        if (delay_ticks == 0) {
                            // end of song
                            delay_ticks = 500;
                            note_index = 0;
                        }
                    }
                }
            }
        }
    }
}

// WaveOut playback thread
DWORD WINAPI audio_thread(LPVOID param) {
    HWAVEOUT hWaveOut = NULL;
    WAVEFORMATEX wfx = {0};
    wfx.wFormatTag = WAVE_FORMAT_PCM;
    wfx.nChannels = CHANNELS;
    wfx.nSamplesPerSec = SAMPLE_RATE;
    wfx.wBitsPerSample = BITS_PER_SAMPLE;
    wfx.nBlockAlign = BLOCK_ALIGN;
    wfx.nAvgBytesPerSec = SAMPLE_RATE * wfx.nBlockAlign;
    wfx.cbSize = 0;

    MMRESULT res = waveOutOpen(&hWaveOut, WAVE_MAPPER, &wfx, 0, 0, CALLBACK_NULL);
    if (res != MMSYSERR_NOERROR) {
        fprintf(stderr, "waveOutOpen failed: %u\n", (unsigned)res);
        return 1;
    }

    const int FRAMES_PER_BUF = 512; // small buffer for low latency
    const int BUF_COUNT = 4;
    WAVEHDR headers[BUF_COUNT];
    int16_t *buffers[BUF_COUNT];

    for (int i = 0; i < BUF_COUNT; ++i) {
        buffers[i] = (int16_t*)malloc(FRAMES_PER_BUF * CHANNELS * sizeof(int16_t));
        ZeroMemory(&headers[i], sizeof(WAVEHDR));
        headers[i].lpData = (LPSTR)buffers[i];
        headers[i].dwBufferLength = FRAMES_PER_BUF * CHANNELS * sizeof(int16_t);
        headers[i].dwFlags = 0;
        waveOutPrepareHeader(hWaveOut, &headers[i], sizeof(WAVEHDR));
    }

    // Prime buffers
    for (int i = 0; i < BUF_COUNT; ++i) {
        generate_audio(buffers[i], FRAMES_PER_BUF);
        waveOutWrite(hWaveOut, &headers[i], sizeof(WAVEHDR));
    }

    // Playback loop
    while (running) {
        // Wait for any buffer to finish
        int idx = -1;
        for (int i = 0; i < BUF_COUNT; ++i) {
            if (headers[i].dwFlags & WHDR_DONE) {
                idx = i;
                break;
            }
        }
        if (idx == -1) {
            // Sleep briefly to avoid busy loop
            Sleep(1);
            continue;
        }
        // Refill and write
        headers[idx].dwFlags &= ~WHDR_DONE;
        generate_audio(buffers[idx], FRAMES_PER_BUF);
        waveOutWrite(hWaveOut, &headers[idx], sizeof(WAVEHDR));
    }

    // Drain and cleanup
    waveOutReset(hWaveOut);
    for (int i = 0; i < BUF_COUNT; ++i) {
        waveOutUnprepareHeader(hWaveOut, &headers[i], sizeof(WAVEHDR));
        free(buffers[i]);
    }
    waveOutClose(hWaveOut);
    return 0;
}

int main(void) {
    printf("AVR audio player starting. Press Enter to stop.\n");

    // Initialize channel states like AVR main()
    for (int i = 0; i < NUM_CHANNELS; ++i) {
        ch[i].count = 0;
        ch[i].add = 0;
        ch[i].env_count = 0;
    }
    note_index = -1;
    delay_ticks = 1;
    update_count = 0;

    // Start audio thread
    HANDLE hThread = CreateThread(NULL, 0, audio_thread, NULL, 0, NULL);
    if (!hThread) {
        fprintf(stderr, "Failed to create audio thread\n");
        return 1;
    }

    // Wait for user to press Enter
    getchar();
    running = 0;

    // Wait for thread to exit
    WaitForSingleObject(hThread, INFINITE);
    CloseHandle(hThread);

    printf("Stopped.\n");
    return 0;
}
