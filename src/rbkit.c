//ssj71
//rbkit.c
#include "rbkit.h"

static void init_rb_kit(MIDIDRUM* MIDI_DRUM)
{
    MIDI_DRUM->buf_indx[RED] = 12;
    MIDI_DRUM->buf_mask[RED] = 0xFF;
    MIDI_DRUM->buf_indx[YELLOW] = 11;
    MIDI_DRUM->buf_mask[YELLOW] = 0xFF;
    MIDI_DRUM->buf_indx[BLUE] = 14;
    MIDI_DRUM->buf_mask[BLUE] = 0xFF;
    MIDI_DRUM->buf_indx[GREEN] = 13;
    MIDI_DRUM->buf_mask[GREEN] = 0xFF;
    MIDI_DRUM->buf_indx[CYMBAL_FLAG] = 1;
    MIDI_DRUM->buf_mask[CYMBAL_FLAG] = 0x08;
    MIDI_DRUM->buf_indx[YELLOW_CYMBAL] = 11;
    MIDI_DRUM->buf_mask[YELLOW_CYMBAL] = 0xFF;
    MIDI_DRUM->buf_indx[BLUE_CYMBAL] = 14;
    MIDI_DRUM->buf_mask[BLUE_CYMBAL] = 0xFF;
    MIDI_DRUM->buf_indx[GREEN_CYMBAL] = 13;
    MIDI_DRUM->buf_mask[GREEN_CYMBAL] = 0xFF;
    MIDI_DRUM->buf_indx[ORANGE_BASS] = 0;
    MIDI_DRUM->buf_mask[ORANGE_BASS] = 0x10;
    MIDI_DRUM->buf_indx[BLACK_BASS] = 0;
    MIDI_DRUM->buf_mask[BLACK_BASS] = 0x20;
}

static inline void calc_velocity(MIDIDRUM* MIDI_DRUM, unsigned char value)
{
    MIDI_DRUM->velocity = min(max((280-value) * 2, 0), 127);
}

inline void handle_drum(MIDIDRUM* MIDI_DRUM, unsigned char drum)
{
   if (MIDI_DRUM->drum_state[drum] && !MIDI_DRUM->prev_state[drum]) {
       calc_velocity(MIDI_DRUM->drum_state[drum]);
       noteup(MIDI_DRUM->g_seq, MIDI_DRUM->g_port, DEFAULT_CHANNEL, MIDI_DRUM->midi_note[drum], -1);
       notedown( MIDI_DRUM->g_seq,  MIDI_DRUM->g_port, DEFAULT_CHANNEL, MIDI_DRUM->midi_note[drum], MIDI_DRUM->velocity);
   }
}

static inline void handle_bass(MIDIDRUM* MIDI_DRUM, unsigned char drum)
{
    if (MIDI_DRUM->drum_state[drum] != MIDI_DRUM->prev_state[drum]) {
        if (MIDI_DRUM->drum_state[drum]) {
            MIDI_DRUM->velocity = 125;
            notedown( MIDI_DRUM->g_seq,  MIDI_DRUM->g_port, DEFAULT_CHANNEL, MIDI_DRUM->midi_note[drum],  MIDI_DRUM->velocity);
        }
        // Up
        else {
            noteup( MIDI_DRUM->g_seq,  MIDI_DRUM->g_port, DEFAULT_CHANNEL, MIDI_DRUM->midi_note[drum], 0);
        }
    }
}

//callback for rockband kit
static void cb_irq_rb(struct libusb_transfer *transfer)
{
    MIDIDRUM* MIDI_DRUM = (MIDIDRUM*)transfer->user_data; 
    if (transfer->status != LIBUSB_TRANSFER_COMPLETED) {
        fprintf(stderr, "irq transfer status %d? %d\n", transfer->status, LIBUSB_TRANSFER_ERROR);
        MIDI_DRUM->do_exit = 2;
        libusb_free_transfer(transfer);
        transfer = NULL;
        return;
    }

    //RockBand 3 Drumkit
    get_state(RED);
    get_state(YELLOW);
    get_state(GREEN);
    get_state(BLUE);
    get_state(CYMBAL_FLAG);
    get_state(YELLOW_CYMBAL);
    get_state(BLUE_CYMBAL);
    get_state(GREEN_CYMBAL);
    get_state(ORANGE_BASS);
    get_state(BLACK_BASS);

    handle_drum(RED); 
    if(MIDI_DRUM->drum_state[CYMBAL_FLAG]){
    
       	handle_drum(YELLOW_CYMBAL); 
       	handle_drum(BLUE_CYMBAL); 
       	handle_drum(GREEN_CYMBAL); 
    }    
    else{
        handle_drum(YELLOW);
       	handle_drum(BLUE);
        handle_drum(GREEN);
    }   
    handle_bass(ORANGE_BASS);
    handle_bass(BLACK_BASS);
        
    //now that the time-critical stuff is done, lets do the assignments 
    memcpy(MIDI_DRUM->prev_state,MIDI_DRUM->drum_state,NUM_DRUMS);
    
    if (MIDI_DRUM->verbose)
    {
        print_hits(MIDI_DRUM);
	print_buf(MIDI_DRUM);
    } 
    if (libusb_submit_transfer(transfer) < 0)
        MIDI_DRUM->do_exit = 2;
}
