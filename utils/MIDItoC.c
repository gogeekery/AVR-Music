/*  github.com/gogeekery

  MIDItoC.c
  Read a standard MIDI file and write a C header file designed for AVR import:
    const uint8_t <name>[][3] PROGMEM = { {note, lo, hi}, ... };

  Features:
  - Parses MIDI tracks, note-on/note-off, and tempo meta events.
  - Allocates up to 4 AVR voices (polyphony) and emits one triplet per note-on,
    assigning each note to an AVR channel (0..3).
  - Emits chord notes with a small spacing (CHORD_SPACING) in AVR delay units so
    chords sound simultaneous on the AVR player.
  - Command-line options: pitch shift, chord spacing, output directory.
  - Produces the same data packing: data = (delay << 3) | (channel & 0x3).

  Compile:
    gcc -O2 -o midi2avr midi2avr.c

  Usage:
    midi2avr <input.mid> <outname> [tempo] [pitch_shift] [chord_spacing] [outdir]

    - input.mid     : path to MIDI file
    - outname       : base name for header (e.g., spiritedaway_itsumo)
    - tempo         : forces the song to play at a fixed tempo
    - pitch_shift   : semitone shift (default 0)
    - chord_spacing : AVR delay units between chord notes (default 1)
    - outdir        : output directory (default .)
*/

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <math.h>

#define MAX_EVENTS 200000
#define MAX_STARTS 200000
#define MAX_STREAM (MAX_STARTS * 2)
#define MAX_VOICES 4
#define MAX_DELAY_UNITS 8191

typedef struct {
    uint32_t time_ticks;
    int type; // 0 = note_on, 1 = note_off
    uint8_t channel;
    uint8_t note;
    uint8_t velocity;
    int track;
} MidiEvent;

typedef struct {
    double time_seconds;
    int note;    // MIDI note (0..127)
    int midi_channel;
    double off_time_seconds;
} NoteStart;

typedef struct {
    double time_seconds; // adjusted time (seconds) used for spacing, later converted to delay units
    int note;            // 0..127
    int avr_channel;     // 0..3
} StreamEvent;

static MidiEvent events[MAX_EVENTS];
static int events_count = 0;

/* Read big-endian 32-bit */
static uint32_t read_be32(FILE *f) {
    int a = fgetc(f);
    int b = fgetc(f);
    int c = fgetc(f);
    int d = fgetc(f);
    if (a==EOF||b==EOF||c==EOF||d==EOF) return 0;
    return ((uint32_t)a<<24)|((uint32_t)b<<16)|((uint32_t)c<<8)|((uint32_t)d);
}

/* Read big-endian 16-bit */
static uint16_t read_be16(FILE *f) {
    int a = fgetc(f);
    int b = fgetc(f);
    if (a==EOF||b==EOF) return 0;
    return ((uint16_t)a<<8)|((uint16_t)b);
}

/* Read variable-length quantity */
static uint32_t read_vlq(FILE *f) {
    uint32_t value = 0;
    int c;
    do {
        c = fgetc(f);
        if (c == EOF) return value;
        value = (value << 7) | (c & 0x7F);
    } while (c & 0x80);
    return value;
}

/* Add MIDI event */
static void add_event(uint32_t time_ticks, int type, uint8_t channel, uint8_t note, uint8_t vel, int track) {
    if (events_count >= MAX_EVENTS) {
        fprintf(stderr, "Too many events\n");
        exit(1);
    }
    events[events_count].time_ticks = time_ticks;
    events[events_count].type = type;
    events[events_count].channel = channel;
    events[events_count].note = note;
    events[events_count].velocity = vel;
    events[events_count].track = track;
    events_count++;
}

/* Comparator for qsort: by time_ticks, note_off before note_on */
static int cmp_midi_events(const void *a, const void *b) {
    const MidiEvent *A = (const MidiEvent*)a;
    const MidiEvent *B = (const MidiEvent*)b;
    if (A->time_ticks < B->time_ticks) return -1;
    if (A->time_ticks > B->time_ticks) return 1;
    if (A->type != B->type) return (A->type - B->type);
    if (A->note != B->note) return (A->note - B->note);
    return (A->channel - B->channel);
}

/* Comparator for NoteStart by time then note */
static int cmp_notestart(const void *a, const void *b) {
    const NoteStart *A = (const NoteStart*)a;
    const NoteStart *B = (const NoteStart*)b;
    if (A->time_seconds < B->time_seconds) return -1;
    if (A->time_seconds > B->time_seconds) return 1;
    return (A->note - B->note);
}

/* Comparator for StreamEvent by time then channel */
static int cmp_streamevent(const void *a, const void *b) {
    const StreamEvent *A = (const StreamEvent*)a;
    const StreamEvent *B = (const StreamEvent*)b;
    if (A->time_seconds < B->time_seconds) return -1;
    if (A->time_seconds > B->time_seconds) return 1;
    return (A->avr_channel - B->avr_channel);
}

/* Parse MIDI file (simple parser) */
static void parse_midi(const char *path, uint16_t *out_ppq, uint32_t *out_tempo_micro) {
    FILE *f = fopen(path, "rb");
    if (!f) { perror(path); exit(1); }

    char hdr[5] = {0};
    if (fread(hdr,1,4,f) != 4) { fprintf(stderr,"Invalid MIDI\n"); exit(1); }
    if (strncmp(hdr,"MThd",4) != 0) { fprintf(stderr,"Not a MIDI file\n"); exit(1); }
    uint32_t hdr_len = read_be32(f);
    uint16_t format = read_be16(f);
    uint16_t ntrks = read_be16(f);
    uint16_t division = read_be16(f);
    *out_ppq = division;
    if (hdr_len > 6) fseek(f, hdr_len - 6, SEEK_CUR);

    uint32_t tempo = 500000; // default microseconds per quarter
    *out_tempo_micro = tempo;

    for (int t = 0; t < ntrks; ++t) {
        if (fread(hdr,1,4,f) != 4) break;
        if (strncmp(hdr,"MTrk",4) != 0) { fprintf(stderr,"Expected MTrk\n"); exit(1); }
        uint32_t trk_len = read_be32(f);
        long track_end = ftell(f) + trk_len;
        uint32_t abs_ticks = 0;
        int running_status = 0;
        while (ftell(f) < track_end) {
            uint32_t delta = read_vlq(f);
            abs_ticks += delta;
            int c = fgetc(f);
            if (c == 0xFF) {
                int meta = fgetc(f);
                uint32_t len = read_vlq(f);
                if (meta == 0x51 && len == 3) {
                    int b1 = fgetc(f);
                    int b2 = fgetc(f);
                    int b3 = fgetc(f);
                    tempo = (b1<<16)|(b2<<8)|b3;
                    *out_tempo_micro = tempo;
                } else {
                    for (uint32_t i=0;i<len;i++) fgetc(f);
                }
                running_status = 0;
            } else if (c == 0xF0 || c == 0xF7) {
                uint32_t len = read_vlq(f);
                for (uint32_t i=0;i<len;i++) fgetc(f);
                running_status = 0;
            } else {
                int status;
                if (c & 0x80) {
                    status = c;
                    running_status = status;
                } else {
                    if (!running_status) { fprintf(stderr,"Running status error\n"); exit(1); }
                    status = running_status;
                    ungetc(c, f);
                }
                int type = status & 0xF0;
                int channel = status & 0x0F;
                if (type == 0x80) {
                    int note = fgetc(f);
                    int vel = fgetc(f);
                    add_event(abs_ticks, 1, (uint8_t)channel, (uint8_t)note, (uint8_t)vel, t);
                } else if (type == 0x90) {
                    int note = fgetc(f);
                    int vel = fgetc(f);
                    if (vel == 0) add_event(abs_ticks, 1, (uint8_t)channel, (uint8_t)note, (uint8_t)vel, t);
                    else add_event(abs_ticks, 0, (uint8_t)channel, (uint8_t)note, (uint8_t)vel, t);
                } else if (type == 0xA0 || type == 0xB0 || type == 0xE0) {
                    fgetc(f); fgetc(f);
                } else if (type == 0xC0 || type == 0xD0) {
                    fgetc(f);
                } else {
                    fgetc(f); fgetc(f);
                }
            }
        }
    }

    fclose(f);
}

/* Build polyphonic stream events with voice allocation and chord spacing */
static StreamEvent *build_polyphonic_stream(uint16_t ppq, uint32_t tempo_micro, int pitch_shift, int chord_spacing, int *out_count) {
    if (events_count == 0) { *out_count = 0; return NULL; }
    qsort(events, events_count, sizeof(MidiEvent), cmp_midi_events);

    double seconds_per_tick = (double)tempo_micro / 1000000.0 / (double)ppq;
    const double update_rate = 23952.0 / 92.0; // AVR note-update rate

    // Build list of note-on starts with their off times
    NoteStart *starts = malloc(sizeof(NoteStart) * events_count);
    int starts_count = 0;

    for (int i = 0; i < events_count; ++i) {
        if (events[i].type == 0) { // note_on
            double t = events[i].time_ticks * seconds_per_tick;
            double off_t = t + 2.0; // default long sustain
            // find matching note_off
            for (int j = i+1; j < events_count; ++j) {
                if (events[j].type == 1 && events[j].note == events[i].note && events[j].channel == events[i].channel) {
                    off_t = events[j].time_ticks * seconds_per_tick;
                    break;
                }
            }
            int midi_note = events[i].note + pitch_shift;
            if (midi_note < 0) midi_note = 0;
            if (midi_note > 127) midi_note = 127;
            starts[starts_count].time_seconds = t;
            starts[starts_count].note = midi_note;
            starts[starts_count].midi_channel = events[i].channel;
            starts[starts_count].off_time_seconds = off_t;
            starts_count++;
        }
    }

    if (starts_count == 0) { free(starts); *out_count = 0; return NULL; }

    qsort(starts, starts_count, sizeof(NoteStart), cmp_notestart);

    // Voice state
    typedef struct { int active_note; double start_time; } VoiceState;
    VoiceState voices[MAX_VOICES];
    for (int v = 0; v < MAX_VOICES; ++v) { voices[v].active_note = -1; voices[v].start_time = 0.0; }

    // Build stream events
    StreamEvent *stream = malloc(sizeof(StreamEvent) * MAX_STREAM);
    int stream_idx = 0;

    int i = 0;
    while (i < starts_count) {
        double t0 = starts[i].time_seconds;
        int j = i;
        while (j < starts_count && fabs(starts[j].time_seconds - t0) < 1e-9) j++;
        int group_count = j - i;

        // For each note in the group, allocate a voice and emit a stream event
        for (int k = 0; k < group_count; ++k) {
            int note = starts[i + k].note;
            int midi_ch = starts[i + k].midi_channel;

            // find free voice
            int chosen = -1;
            for (int v = 0; v < MAX_VOICES; ++v) {
                if (voices[v].active_note == -1) { chosen = v; break; }
            }
            // if none free, steal oldest voice
            if (chosen == -1) {
                double oldest = voices[0].start_time;
                chosen = 0;
                for (int v = 1; v < MAX_VOICES; ++v) {
                    if (voices[v].start_time < oldest) { oldest = voices[v].start_time; chosen = v; }
                }
            }

            // assign voice
            voices[chosen].active_note = note;
            voices[chosen].start_time = t0;

            // emit stream event (time_seconds will be adjusted for spacing later)
            stream[stream_idx].time_seconds = t0;
            stream[stream_idx].note = note;
            stream[stream_idx].avr_channel = chosen & 0x3;
            stream_idx++;
        }

        i = j;
    }

    // Add a small offset for events that share the same original time to create chord spacing
    int s = 0;
    while (s < stream_idx) {
        double t = stream[s].time_seconds;
        int e = s;
        while (e < stream_idx && fabs(stream[e].time_seconds - t) < 1e-9) e++;
        for (int k = 0; k < (e - s); ++k) {
            stream[s + k].time_seconds = t + ((double)k * (double)chord_spacing) / update_rate;
        }
        s = e;
    }

    // Sort by adjusted time
    qsort(stream, stream_idx, sizeof(StreamEvent), cmp_streamevent);

    // Convert adjusted times to delays (AVR delay units) between successive stream events
    for (int x = 0; x < stream_idx; ++x) {
        double dt;
        if (x + 1 < stream_idx) dt = stream[x+1].time_seconds - stream[x].time_seconds;
        else dt = 2.0; // tail
        int delay_units = (int)(dt * update_rate + 0.5);
        if (delay_units < 0) delay_units = 0;
        if (delay_units > MAX_DELAY_UNITS) delay_units = MAX_DELAY_UNITS;
        // store delay in time_seconds field for convenience
        stream[x].time_seconds = (double)delay_units;
    }

    *out_count = stream_idx;
    free(starts);
    return stream;
}

/* Write header file */
static void write_header(const char *outdir, const char *name, StreamEvent *stream, int count) {
    char path[1024];
    snprintf(path, sizeof(path), "%s/%s.h", outdir, name);
    FILE *f = fopen(path, "wb");
    if (!f) { perror(path); exit(1); }
    fprintf(f, "#include <stdint.h>\n\n");
    fprintf(f, "#ifndef PROGMEM\n#define PROGMEM\n#endif\n\n");
    fprintf(f, "const uint8_t %s[][3] PROGMEM = {\n", name);
    for (int i = 0; i < count; ++i) {
        int note = stream[i].note;
        int delay = (int)(stream[i].time_seconds + 0.5);
        int channel = stream[i].avr_channel & 0x3;
        uint16_t data = (uint16_t)((delay << 3) | (channel & 0x3));
        uint8_t lo = data & 0xFF;
        uint8_t hi = (data >> 8) & 0xFF;
        fprintf(f, "    {%d, %d, %d},\n", note, lo, hi);
    }
    // final rest with delay 0
    fprintf(f, "    {0, 0, 0}\n};\n");
    fclose(f);
    printf("Wrote %s (%d events)\n", path, count);
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <input.mid> <outname> [bpm] [pitch_shift] [chord_spacing] [outdir]\n", argv[0]);
        return 1;
    }
    const char *infile = argv[1];
    char *outname = NULL;
    int pitch_shift = 0;
    int chord_spacing = 1;
    int user_bpm = 0;
    const char *outdir = ".";

    // User didn't provide a name (or maybe they dragged and dropped a mid file into the exe on Windows)
    if (argc >= 3) {
        outname = strdup(argv[2]);
    } else {
        // Find the start of the actual filename (after any slashes)
        const char *filename_start = strrchr(infile, '/');
        if (!filename_start) filename_start = strrchr(infile, '\\');
        filename_start = (filename_start == NULL) ? infile : filename_start + 1;

        char *dot = strrchr(filename_start, '.');
        size_t base_len = dot ? (size_t)(dot - filename_start) : strlen(filename_start);

        outname = malloc(base_len + 1); 
        if (outname) {
            memcpy(outname, filename_start, base_len);
            outname[base_len] = '\0';
        }
    }


    if (argc >= 4) user_bpm = atoi(argv[3]);
    if (argc >= 5) pitch_shift = atoi(argv[4]);
    if (argc >= 6) chord_spacing = atoi(argv[5]);
    if (argc >= 7) outdir = argv[6];
    if (chord_spacing < 0) chord_spacing = 0;

    uint16_t ppq = 480;
    uint32_t tempo_micro = 500000;
    parse_midi(infile, &ppq, &tempo_micro);

    if (user_bpm) {
        tempo_micro = 60000000 / user_bpm;
    }

    if (events_count == 0) {
        fprintf(stderr, "No MIDI events found.\n");
        return 1;
    }

    int stream_count = 0;
    StreamEvent *stream = build_polyphonic_stream(ppq, tempo_micro, pitch_shift, chord_spacing, &stream_count);
    if (!stream || stream_count == 0) {
        fprintf(stderr, "No stream events generated.\n");
        return 1;
    }

    write_header(outdir, outname, stream, stream_count);

    // printf("\nSuccess! Generated %s from %s\n", outname, infile);

    // A modern OS just does this for us anyways...
    // if (argc < 3) {
    //     free(outname);
    // }
    // free(stream);

    return 0;
}
