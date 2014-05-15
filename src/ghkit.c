//ssj71
//ghkit.c
#include "ghkit.h"
//#include "constants.h"

void init_gh_kit(MIDIDRUM* MIDI_DRUM)
{
	    MIDI_DRUM->buf_indx[RED] = 12;
	    MIDI_DRUM->buf_mask[RED] = 0xFF;	
	    MIDI_DRUM->buf_indx[YELLOW_CYMBAL] = 11;
	    MIDI_DRUM->buf_mask[YELLOW_CYMBAL] = 0xff;
	    MIDI_DRUM->buf_indx[BLUE] = 14;
	    MIDI_DRUM->buf_mask[BLUE] = 0xff;
	    MIDI_DRUM->buf_indx[GREEN] = 13;
	    MIDI_DRUM->buf_mask[GREEN] = 0xff;
	    MIDI_DRUM->buf_indx[ORANGE_CYMBAL] = 16;
	    MIDI_DRUM->buf_mask[ORANGE_CYMBAL] = 0xff;
	    MIDI_DRUM->buf_indx[ORANGE_BASS] = 15;
	    MIDI_DRUM->buf_mask[ORANGE_BASS] = 0xff;
}

static inline void calc_velocity(MIDIDRUM* MIDI_DRUM, unsigned char value)
{
    MIDI_DRUM->velocity = min(max(value * 2, 0), 127);
}

static inline void handle_drum(MIDIDRUM* MIDI_DRUM, unsigned char drum)
{
   if (MIDI_DRUM->drum_state[drum] && !MIDI_DRUM->prev_state[drum]) {
       calc_velocity(MIDI_DRUM,MIDI_DRUM->drum_state[drum]);
       noteup(MIDI_DRUM->g_seq, MIDI_DRUM->g_port, MIDI_DRUM->channel, MIDI_DRUM->midi_note[drum], -1);
       notedown( MIDI_DRUM->g_seq,  MIDI_DRUM->g_port, MIDI_DRUM->channel, MIDI_DRUM->midi_note[drum], MIDI_DRUM->velocity);
   }
}

static inline void handle_bass(MIDIDRUM* MIDI_DRUM, unsigned char drum)
{
   if (MIDI_DRUM->drum_state[drum] && !MIDI_DRUM->prev_state[drum]) {
       if(MIDI_DRUM->drum_state[drum]){
           calc_velocity(MIDI_DRUM,MIDI_DRUM->drum_state[drum]); 
           notedown( MIDI_DRUM->g_seq,  MIDI_DRUM->g_port, MIDI_DRUM->channel, MIDI_DRUM->midi_note[drum], MIDI_DRUM->velocity);
	   if(MIDI_DRUM->hat_mode = drum)
	   {
	       MIDI_DRUM->midi_note[MIDI_DRUM->hat] = MIDI_DRUM->midi_note[CLOSED_HAT];
	   }
       }else{ 
           noteup(MIDI_DRUM->g_seq, MIDI_DRUM->g_port, MIDI_DRUM->channel, MIDI_DRUM->midi_note[drum], -1); 
	   if(MIDI_DRUM->hat_mode = drum)
	   {
	       MIDI_DRUM->midi_note[MIDI_DRUM->hat] = MIDI_DRUM->midi_note[OPEN_HAT];
	   }
       }
   }
}

//callback for guitar hero kit
void cb_irq_gh(struct libusb_transfer *transfer)
{
    MIDIDRUM* MIDI_DRUM = (MIDIDRUM*)transfer->user_data; 
    if (transfer->status != LIBUSB_TRANSFER_COMPLETED) {
        fprintf(stderr, "irq transfer status %d? %d\n", transfer->status, LIBUSB_TRANSFER_ERROR);
        do_exit = 2;
        libusb_free_transfer(transfer);
        transfer = NULL;
        return;
    }

    //Guitar Hero Drumkit
    get_state(MIDI_DRUM,RED);
    get_state(MIDI_DRUM,YELLOW_CYMBAL);
    get_state(MIDI_DRUM,GREEN);
    get_state(MIDI_DRUM,BLUE);
    get_state(MIDI_DRUM,ORANGE_CYMBAL);
    get_state(MIDI_DRUM,ORANGE_BASS);

    handle_drum(MIDI_DRUM,RED);
    handle_drum(MIDI_DRUM,YELLOW_CYMBAL);
    handle_drum(MIDI_DRUM,GREEN);
    handle_drum(MIDI_DRUM,BLUE);
    handle_drum(MIDI_DRUM,ORANGE_CYMBAL); 
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
