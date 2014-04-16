//ssj71
//rbkit.c
#include "rb1kit.h"

static void init_rb1_kit(MIDIDRUM* MIDI_DRUM)
{
	switch(MIDI_DRUM->kit)
    {
     case PS_ROCKBAND1:
	    MIDI_DRUM->buf_indx[RED] = 0;
	    MIDI_DRUM->buf_mask[RED] = 0x04;
	    MIDI_DRUM->buf_indx[YELLOW] = 0;
	    MIDI_DRUM->buf_mask[YELLOW] = 0x08;
	    MIDI_DRUM->buf_indx[BLUE] = 0;
	    MIDI_DRUM->buf_mask[BLUE] = 0x01;
	    MIDI_DRUM->buf_indx[GREEN] = 0;
	    MIDI_DRUM->buf_mask[GREEN] = 0x02;
	    MIDI_DRUM->buf_indx[ORANGE_BASS] = 0;
	    MIDI_DRUM->buf_mask[ORANGE_BASS] = 0x10;
	    break;
	 case XB_ROCKBAND1:
	    MIDI_DRUM->buf_indx[RED] = 3;
	    MIDI_DRUM->buf_mask[RED] = 0x20;
	    MIDI_DRUM->buf_indx[YELLOW] = 3;
	    MIDI_DRUM->buf_mask[YELLOW] = 0x80;
	    MIDI_DRUM->buf_indx[BLUE] = 3;
	    MIDI_DRUM->buf_mask[BLUE] = 0x40;
	    MIDI_DRUM->buf_indx[GREEN] = 3;
	    MIDI_DRUM->buf_mask[GREEN] = 0x10;
	    MIDI_DRUM->buf_indx[ORANGE_BASS] = 3;
	    MIDI_DRUM->buf_mask[ORANGE_BASS] = 0x01;
	    break;
    }
}

static inline void calc_velocity(MIDIDRUM* MIDI_DRUM, unsigned char value)
{
    MIDI_DRUM->velocity = 125;//TODO: allow velocity to be selected
}

static inline void handle_drum(MIDIDRUM* MIDI_DRUM, unsigned char drum)
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

//callback for rockband1 kit
static void cb_irq_rb1(struct libusb_transfer *transfer)
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
    get_state(ORANGE_BASS);

    handle_drum(RED);
    handle_drum(YELLOW);
    handle_drum(BLUE);
    handle_drum(GREEN);
    
    handle_bass(ORANGE_BASS);
        
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
