//ssj71
//rbkit.c
#include "rbkit.h"
//#include "constants.h"

void init_rb_kit(MIDIDRUM* MIDI_DRUM)
{
    MIDI_DRUM->buf_indx[GREEN] = 3;
    MIDI_DRUM->buf_mask[GREEN] = 0x10;
    MIDI_DRUM->buf_indx[RED] = 3;
    MIDI_DRUM->buf_mask[RED] = 0x20;
    MIDI_DRUM->buf_indx[YELLOW] = 3;
    MIDI_DRUM->buf_mask[YELLOW] = 0x80;
    MIDI_DRUM->buf_indx[BLUE] = 3;
    MIDI_DRUM->buf_mask[BLUE] = 0x40;
    MIDI_DRUM->buf_indx[ORANGE] = 3;
    MIDI_DRUM->buf_mask[ORANGE] = 0x01;

    MIDI_DRUM->buf_indx[PICK] = 2;
    MIDI_DRUM->buf_mask[PICK] = 0x03;
    MIDI_DRUM->buf_indx[HINOTE] = 2;
    MIDI_DRUM->buf_mask[HINOTE] = 0x40;

}

static inline void calc_velocity(MIDIDRUM* MIDI_DRUM, unsigned char value)
{
    MIDI_DRUM->velocity = MIDI_DRUM->default_velocity;
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
void cb_irq_rb(struct libusb_transfer *transfer)
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
    get_state(MIDI_DRUM,ORANGE);
    get_state(MIDI_DRUM,PICK);
    get_state(MIDI_DRUM,HINOTE);
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
