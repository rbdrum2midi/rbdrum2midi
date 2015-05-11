//ssj71
//mididrum.h
#ifndef MIDIDRUM_H
#define MIDIDRUM_H

#include <errno.h>
#include <signal.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include <libusb-1.0/libusb.h>

//#include <jackdriver.h>
#include "alsadriver.h"


#define EP_INTR			(1 | LIBUSB_ENDPOINT_IN)
#define INTR_LENGTH		27

#define DEFAULT_CHANNEL 9

#define YVK_KICK        36
#define YVK_SNARE       37
#define YVK_LO_TOM      38
#define YVK_MID_TOM     39
#define YVK_HI_TOM      40
#define YVK_CLOSED_HAT  41
#define YVK_OPEN_HAT    42
#define YVK_RIDE        43
#define YVK_CRASH       45

#define max(a, b) (((a) > (b)) ? (a) : (b))
#define min(a, b) (((a) < (b)) ? (a) : (b))

typedef enum {
    RED = 0,
    YELLOW,
    BLUE,
    GREEN,
    YELLOW_CYMBAL,
    BLUE_CYMBAL,
    GREEN_CYMBAL,
    ORANGE_CYMBAL,
    ORANGE_BASS,
    BLACK_BASS,
    CYMBAL_FLAG,
    OPEN_HAT,
    CLOSED_HAT,
    NUM_DRUMS
} drums;

typedef enum {
    UNKNOWN = 0,
    PS_ROCKBAND,
    XB_ROCKBAND,
    WII_ROCKBAND,
    XB_ROCKBAND1,
    PS_ROCKBAND1,
    GUITAR_HERO,
    ION
}kit_types;

//primary object for the system
typedef struct drum_midi
{
    unsigned char kit;
    unsigned char channel;
    unsigned char midi_note[NUM_DRUMS];
    unsigned char buf_indx[NUM_DRUMS];
    unsigned char buf_mask[NUM_DRUMS];
    unsigned char *buf;
    unsigned char drum_state[NUM_DRUMS];
    unsigned char prev_state[NUM_DRUMS];
    void* sequencer;//want to move to generic sequencer, but currently more worried about latency
//    snd_seq_t *g_seq;
//    int g_port;

    unsigned char verbose;
    unsigned char dbg;
//    int do_exit;
    unsigned char hat_mode;
    unsigned char hat;
    unsigned char bass_down;
    int velocity;
    unsigned char default_velocity;
    unsigned char irqbuf[INTR_LENGTH];
    unsigned char oldbuf[INTR_LENGTH];
//    struct libusb_device_handle *devh;
//    struct libusb_transfer *irq_transfer;

//function pointers //getting rid of these because they seem to introduce latency
//    void (*calc_velocity)(unsigned char);
//    void (*handle_bass)(unsigned char);
    void (*noteup)(void* seq, unsigned char chan, unsigned char note, unsigned char vel);
    void (*notedown)(void* seq, unsigned char chan, unsigned char note, unsigned char vel);
}MIDIDRUM;

static inline void get_state(MIDIDRUM* MIDI_DRUM, unsigned char drum){MIDI_DRUM->drum_state[drum] = MIDI_DRUM->buf[MIDI_DRUM->buf_indx[drum]] & MIDI_DRUM->buf_mask[drum];}
//inline void notedown(snd_seq_t *seq, int port, int chan, int pitch, int vol);
//inline void noteup(snd_seq_t *seq, int port, int chan, int pitch, int vol);
void print_hits(MIDIDRUM* MIDI_DRUM);
void print_buf(MIDIDRUM* MIDI_DRUM);

//other globals
int do_exit;
#endif
