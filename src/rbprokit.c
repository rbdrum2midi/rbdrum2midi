//ssj71
//rbprokit.c
#include "rbprokit.h"

void init_rb_pro_kit(MIDIDRUM* MIDI_DRUM)
{
    MIDI_DRUM->buf_indx[RED] = 10;
    MIDI_DRUM->buf_mask[RED] = 0xFF;
    MIDI_DRUM->buf_indx[YELLOW] = 12;
    MIDI_DRUM->buf_mask[YELLOW] = 0xFF;
    MIDI_DRUM->buf_indx[BLUE] = 14;
    MIDI_DRUM->buf_mask[BLUE] = 0xFF;
    MIDI_DRUM->buf_indx[GREEN] = 16;
    MIDI_DRUM->buf_mask[GREEN] = 0xFF;
    MIDI_DRUM->buf_indx[CYMBAL_FLAG] = 8;
    MIDI_DRUM->buf_mask[CYMBAL_FLAG] = 0x02;
    MIDI_DRUM->buf_indx[YELLOW_CYMBAL] = 12;
    MIDI_DRUM->buf_mask[YELLOW_CYMBAL] = 0xFF;
    MIDI_DRUM->buf_indx[BLUE_CYMBAL] = 14;
    MIDI_DRUM->buf_mask[BLUE_CYMBAL] = 0xFF;
    MIDI_DRUM->buf_indx[GREEN_CYMBAL] = 16;
    MIDI_DRUM->buf_mask[GREEN_CYMBAL] = 0xFF;
    MIDI_DRUM->buf_indx[ORANGE_BASS] = 7;
    MIDI_DRUM->buf_mask[ORANGE_BASS] = 0x01;
    MIDI_DRUM->buf_indx[BLACK_BASS] = 6;
    MIDI_DRUM->buf_mask[BLACK_BASS] = 0x40;
}

static inline void calc_velocity(MIDIDRUM* MIDI_DRUM, unsigned char value)
{
    MIDI_DRUM->velocity = min(max((280-value) * 2, 0), 127);
}

static inline void handle_drum(MIDIDRUM* MIDI_DRUM, unsigned char drum)
{
   if (MIDI_DRUM->drum_state[drum] && !MIDI_DRUM->prev_state[drum]) {
       calc_velocity(MIDI_DRUM,MIDI_DRUM->drum_state[drum]);
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

//callback for rockband kit
void cb_irq_rb_pro(struct libusb_transfer *transfer)
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
    get_state(MIDI_DRUM,CYMBAL_FLAG);
    //cymbals are same data as pads
    MIDI_DRUM->drum_state[YELLOW_CYMBAL] = MIDI_DRUM->drum_state[YELLOW];
    MIDI_DRUM->drum_state[GREEN_CYMBAL] = MIDI_DRUM->drum_state[GREEN];
    MIDI_DRUM->drum_state[BLUE_CYMBAL] = MIDI_DRUM->drum_state[BLUE];
    get_state(MIDI_DRUM,ORANGE_BASS);
    get_state(MIDI_DRUM,BLACK_BASS);

    handle_drum(MIDI_DRUM,RED); 
    if(MIDI_DRUM->drum_state[CYMBAL_FLAG]){
    
       	handle_drum(MIDI_DRUM,YELLOW_CYMBAL); 
       	handle_drum(MIDI_DRUM,BLUE_CYMBAL); 
       	handle_drum(MIDI_DRUM,GREEN_CYMBAL); 
    }    
    else{
        handle_drum(MIDI_DRUM,YELLOW);
       	handle_drum(MIDI_DRUM,BLUE);
        handle_drum(MIDI_DRUM,GREEN);
    }   
    handle_bass(MIDI_DRUM,ORANGE_BASS);
    handle_bass(MIDI_DRUM,BLACK_BASS);
        
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
