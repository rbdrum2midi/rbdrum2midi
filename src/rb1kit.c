//ssj71
//rbkit.c
#include "rb1kit.h"
//#include "constants.h"

void init_rb1_kit(MIDIDRUM* MIDI_DRUM)
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

static inline void calc_velocity(MIDIDRUM* MIDI_DRUM)
{
    MIDI_DRUM->velocity = MIDI_DRUM->default_velocity;
}

static inline void handle_drum(MIDIDRUM* MIDI_DRUM, unsigned char drum)
{
   if (MIDI_DRUM->drum_state[drum] && !MIDI_DRUM->prev_state[drum]) {
       calc_velocity(MIDI_DRUM);
       MIDI_DRUM->noteup(MIDI_DRUM->sequencer, MIDI_DRUM->channel, MIDI_DRUM->midi_note[drum], 0);
       MIDI_DRUM->notedown( MIDI_DRUM->sequencer, MIDI_DRUM->channel, MIDI_DRUM->midi_note[drum], MIDI_DRUM->velocity);
   }
}

static inline void handle_bass(MIDIDRUM* MIDI_DRUM, unsigned char drum)
{
    if (MIDI_DRUM->drum_state[drum] != MIDI_DRUM->prev_state[drum]) {
        if (MIDI_DRUM->drum_state[drum]) {
            MIDI_DRUM->velocity = MIDI_DRUM->default_velocity;
            MIDI_DRUM->notedown( MIDI_DRUM->sequencer, MIDI_DRUM->channel, MIDI_DRUM->midi_note[drum], MIDI_DRUM->velocity);
        if(MIDI_DRUM->hat_mode == drum)
	    {
		MIDI_DRUM->midi_note[MIDI_DRUM->hat] = MIDI_DRUM->midi_note[CLOSED_HAT];
	    }
        }
        // Up
        else {
            MIDI_DRUM->noteup(MIDI_DRUM->sequencer, MIDI_DRUM->channel, MIDI_DRUM->midi_note[drum], 0);
	    if(MIDI_DRUM->hat_mode == drum)
	    {
		MIDI_DRUM->midi_note[MIDI_DRUM->hat] = MIDI_DRUM->midi_note[OPEN_HAT];
	    }
        }
    }
}

//callback for rockband1 kit
void cb_irq_rb1(struct libusb_transfer *transfer)
{
    MIDIDRUM* MIDI_DRUM = (MIDIDRUM*)transfer->user_data; 
    if (transfer->status != LIBUSB_TRANSFER_COMPLETED) {
        fprintf(stderr, "irq transfer status %d? %d\n", transfer->status, LIBUSB_TRANSFER_ERROR);
        do_exit = 2;
        libusb_free_transfer(transfer); 
        transfer = NULL;
        return;
    }

    //RockBand 3 Drumkit
    get_state(MIDI_DRUM,RED);
    get_state(MIDI_DRUM,YELLOW);
    get_state(MIDI_DRUM,GREEN);
    get_state(MIDI_DRUM,BLUE);
    get_state(MIDI_DRUM,ORANGE_BASS);

    handle_drum(MIDI_DRUM,RED);
    handle_drum(MIDI_DRUM,YELLOW);
    handle_drum(MIDI_DRUM,BLUE);
    handle_drum(MIDI_DRUM,GREEN);
    
    handle_bass(MIDI_DRUM,ORANGE_BASS);
        
    //now that the time-critical stuff is done, lets do the assignments 
    memcpy(MIDI_DRUM->prev_state,MIDI_DRUM->drum_state,NUM_DRUMS);
    
    if (MIDI_DRUM->verbose)
    {
        print_hits(MIDI_DRUM);
		print_buf(MIDI_DRUM);
    } 
    if (libusb_submit_transfer(transfer) < 0)
        do_exit = 2;
}
