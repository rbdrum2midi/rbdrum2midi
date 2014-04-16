//ssj71
//mididrum.h

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

enum drums{
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
    NUM_DRUMS
};

enum kit_types{
    UNKNOWN = 0,
    PS_ROCKBAND,
    XB_ROCKBAND,
    WII_ROCKBAND,
    XB_ROCKBAND1,
    PS_ROCKBAND1,
    GUITAR_HERO
};

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
//    void* sequencer;//want to move to generic sequencer, but currently more worried about latency
    snd_seq_t *g_seq;
    int g_port;

    unsigned char verbose;
    unsigned char dbg;
    int do_exit;
    unsigned char bass_down;
    int velocity;
    unsigned char irqbuf[INTR_LENGTH];
    unsigned char oldbuf[INTR_LENGTH];
//    struct libusb_device_handle *devh;
//    struct libusb_transfer *irq_transfer;

//function pointers //getting rid of these because they seem to introduce latency
//    void (*calc_velocity)(unsigned char);
//    void (*handle_bass)(unsigned char);
//    void (*noteup)(void* seq, unsigned char note, unsigned char vel);
//    void (*notedown)(void* seq, unsigned char note, unsigned char vel);
}MIDIDRUM;

inline void get_state(MIDIDRUM* MIDI_DRUM, unsigned char drum){MIDI_DRUM->drum_state[drum] = MIDI_DRUM->buf[MIDI_DRUM->buf_indx[drum]] & MIDI_DRUM->buf_mask[drum];}
static void print_hits(MIDIDRUM* MIDI_DRUM);
static void print_buf(MIDIDRUM* MIDI_DRUM);
