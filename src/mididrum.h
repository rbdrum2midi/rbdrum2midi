//ssj71
//mididrum.h


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
