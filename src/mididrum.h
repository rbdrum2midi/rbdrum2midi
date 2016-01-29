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

#define EP_INTR			(1 | LIBUSB_ENDPOINT_IN)
#define INTR_LENGTH		27

#define DEFAULT_CHANNEL 9

enum YVK{
    YVK_KICK = 36,
    YVK_SNARE,
    YVK_LO_TOM,
    YVK_MID_TOM,
    YVK_HI_TOM,
    YVK_CLOSED_HAT,
    YVK_OPEN_HAT,
    YVK_RIDE,
    YVK_CRASH 
};

enum GM{
    GM_KICK = 36,
    GM_SNARE = 38,
    GM_LO_TOM = 43,
    GM_MID_TOM = 47,
    GM_HI_TOM = 50,
    GM_CLOSED_HAT = 42,
    GM_OPEN_HAT = 46,
    GM_RIDE = 51,
    GM_CRASH = 49 
};

#define max(a, b) (((a) > (b)) ? (a) : (b))
#define min(a, b) (((a) < (b)) ? (a) : (b))

typedef enum {
    UP = 0,
    DOWN,
    LEFT,
    RIGHT,
    CONNECT,//4
    START,
    SELECT,//6

    PLUS = 5,//WII buttons
    MINUS,
    A_BUTTON,
    B_BUTTON,
    ONE,
    TWO,//10

    CROSS = 7,//PS3 buttons
    CIRCLE,
    SQUARE,
    TRIANGLE, //10

    GREEN = 7, //XBox buttons
    RED,
    YELLOW,
    BLUE,//10

    ORANGE = 11,
    PICK,//warning, don't mess with the order here, it affects guitar logic
    HINOTE,
    HI_GREEN,
    HI_RED,
    HI_YELLOW,
    HI_BLUE,
    HI_ORANGE,
    WHAMMY_LSB,
    WHAMMY_MSB,
    SELECTOR,


    KEYS0 = 11,
    KEYS1,
    KEYS2,
    KEYS3,
    EXPRESSION,
    EXPR_TOGGLE,
    
    YELLOW_CYMBAL = 11,
    BLUE_CYMBAL,
    GREEN_CYMBAL,
    ORANGE_CYMBAL,
    ORANGE_BASS,
    BLACK_BASS,
    CYMBAL_FLAG,
    NOTHING,
    OPEN_HAT,
    CLOSED_HAT,
    NUM_DRUMS,//NUM DRUMS determines array sizes!  

} drums;

typedef enum {
    UNKNOWN = 0,

    PS_ROCKBAND,
    XB_ROCKBAND,
    WII_ROCKBAND,
    XB_ROCKBAND1,
    PS_ROCKBAND1,
    GUITAR_HERO,
    DRUMS,

    XB_RB_GUITAR,
    PS_RB_GUITAR,
    GUITARS,

    WII_RB3_KEYBOARD
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
    unsigned long key_state;
    unsigned long prev_keystate;
    void* sequencer;

    unsigned char verbose;
    unsigned char dbg;
    unsigned char hat_mode;
    unsigned char hat;
    unsigned char bass_down;
    int velocity;
    char octave;
    unsigned char prog;
    unsigned char default_velocity;
    unsigned char irqbuf[INTR_LENGTH];
    unsigned char oldbuf[INTR_LENGTH];
    void (*noteup)(void* seq, unsigned char chan, unsigned char note, unsigned char vel);
    void (*notedown)(void* seq, unsigned char chan, unsigned char note, unsigned char vel);
    void (*pitchbend)(void* seq, unsigned char chan, short val);//expect val in range [-8192, 8191]
    void (*control)(void* seq, unsigned char chan, unsigned char indx, unsigned char val);//expect val in range [0,127]
    void (*program)(void* seq, unsigned char chan, unsigned char indx);//expect val in range [0,127]
}MIDIDRUM;

static inline void get_state(MIDIDRUM* MIDI_DRUM, unsigned char drum){MIDI_DRUM->drum_state[drum] = MIDI_DRUM->buf[MIDI_DRUM->buf_indx[drum]] & MIDI_DRUM->buf_mask[drum];}
static inline unsigned char changed(MIDIDRUM* MIDI_DRUM, unsigned char drum){return MIDI_DRUM->drum_state[drum] != MIDI_DRUM->prev_state[drum];}
//inline void notedown(snd_seq_t *seq, int port, int chan, int pitch, int vol);
//inline void noteup(snd_seq_t *seq, int port, int chan, int pitch, int vol);
void print_hits(MIDIDRUM* MIDI_DRUM);
void print_buf(MIDIDRUM* MIDI_DRUM);
void print_keys(MIDIDRUM* MIDI_DRUM);
void print_guitar(MIDIDRUM* MIDI_DRUM);

//other globals
int do_exit;
#endif
