//ssj71
//rbkit.c
#include "rbguitar.h"

void init_rb_guitar(MIDIDRUM* MIDI_DRUM)
{
    switch(MIDI_DRUM->kit)
    {
     case XB_RB_GUITAR:
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
        MIDI_DRUM->buf_indx[WHAMMY_MSB] = 11;
        MIDI_DRUM->buf_mask[WHAMMY_MSB] = 0xFF;
        MIDI_DRUM->buf_indx[WHAMMY_LSB] = 10;
        MIDI_DRUM->buf_mask[WHAMMY_LSB] = 0xF0;

        MIDI_DRUM->buf_indx[SELECTOR] = 4;
        MIDI_DRUM->buf_mask[SELECTOR] = 0xFF;
        MIDI_DRUM->buf_indx[LEFT] = 2;
        MIDI_DRUM->buf_mask[LEFT] = 0x04;
        MIDI_DRUM->buf_indx[RIGHT] = 2;
        MIDI_DRUM->buf_mask[RIGHT] = 0x08;

        MIDI_DRUM->buf_indx[START] = 2;
        MIDI_DRUM->buf_mask[START] = 0x10;
        MIDI_DRUM->buf_indx[SELECT] = 2;
        MIDI_DRUM->buf_mask[SELECT] = 0x20;

        MIDI_DRUM->midi_note[PICK] = 0x00;//this is a misuse of the midi note array, but it is a convenient place to keep what the "unpressed" state of the pick should be
        MIDI_DRUM->midi_note[WHAMMY_MSB] = 0x80;//similarly, this is an offset for the pitchbend
        break;
     case XB_RB3_GUITAR:
        MIDI_DRUM->buf_indx[GREEN] = 7;
        MIDI_DRUM->buf_mask[GREEN] = 0x10;
        MIDI_DRUM->buf_indx[RED] = 7;
        MIDI_DRUM->buf_mask[RED] = 0x20;
        MIDI_DRUM->buf_indx[YELLOW] = 7;
        MIDI_DRUM->buf_mask[YELLOW] = 0x80;
        MIDI_DRUM->buf_indx[BLUE] = 7;
        MIDI_DRUM->buf_mask[BLUE] = 0x40;
        MIDI_DRUM->buf_indx[ORANGE] = 7;
        MIDI_DRUM->buf_mask[ORANGE] = 0x01;

        MIDI_DRUM->buf_indx[PICK] = 6;
        MIDI_DRUM->buf_mask[PICK] = 0x03;
        MIDI_DRUM->buf_indx[HINOTE] = 2;
        MIDI_DRUM->buf_mask[HINOTE] = 0x40;
        MIDI_DRUM->buf_indx[WHAMMY_MSB] = 15;
        MIDI_DRUM->buf_mask[WHAMMY_MSB] = 0xFF;
        MIDI_DRUM->buf_indx[WHAMMY_LSB] = 14;
        MIDI_DRUM->buf_mask[WHAMMY_LSB] = 0xFF;

        MIDI_DRUM->buf_indx[SELECTOR] = 4;
        MIDI_DRUM->buf_mask[SELECTOR] = 0xFF;
        MIDI_DRUM->buf_indx[LEFT] = 2;
        MIDI_DRUM->buf_mask[LEFT] = 0x04;
        MIDI_DRUM->buf_indx[RIGHT] = 2;
        MIDI_DRUM->buf_mask[RIGHT] = 0x08;

        MIDI_DRUM->buf_indx[START] = 2;
        MIDI_DRUM->buf_mask[START] = 0x10;
        MIDI_DRUM->buf_indx[SELECT] = 2;
        MIDI_DRUM->buf_mask[SELECT] = 0x20;

        MIDI_DRUM->midi_note[PICK] = 0x00;//this is a misuse of the midi note array, but it is a convenient place to keep what the "unpressed" state of the pick should be
        MIDI_DRUM->midi_note[WHAMMY_MSB] = 0x80;//similarly, this is an offset for the pitchbend
        break;
     case PS_RB_GUITAR:
        MIDI_DRUM->buf_indx[GREEN] = 0;
        MIDI_DRUM->buf_mask[GREEN] = 0x02;
        MIDI_DRUM->buf_indx[RED] = 0;
        MIDI_DRUM->buf_mask[RED] = 0x04;
        MIDI_DRUM->buf_indx[YELLOW] = 0;
        MIDI_DRUM->buf_mask[YELLOW] = 0x08;
        MIDI_DRUM->buf_indx[BLUE] = 0;
        MIDI_DRUM->buf_mask[BLUE] = 0x01;
        MIDI_DRUM->buf_indx[ORANGE] = 0;
        MIDI_DRUM->buf_mask[ORANGE] = 0x10;

        MIDI_DRUM->buf_indx[PICK] = 2;
        MIDI_DRUM->buf_mask[PICK] = 0x0c;
        MIDI_DRUM->buf_indx[HINOTE] = 0;
        MIDI_DRUM->buf_mask[HINOTE] = 0x40;
        MIDI_DRUM->buf_indx[WHAMMY_MSB] = 5;
        MIDI_DRUM->buf_mask[WHAMMY_MSB] = 0xFF;
        MIDI_DRUM->buf_indx[WHAMMY_LSB] = 7;
        MIDI_DRUM->buf_mask[WHAMMY_LSB] = 0x00;

        MIDI_DRUM->buf_indx[SELECTOR] = 4; //TODO: CURRENTLY THESE ARE GUESSES
        MIDI_DRUM->buf_mask[SELECTOR] = 0xFF;
        MIDI_DRUM->buf_indx[LEFT] = 2;
        MIDI_DRUM->buf_mask[LEFT] = 0x04;
        MIDI_DRUM->buf_indx[RIGHT] = 2;
        MIDI_DRUM->buf_mask[RIGHT] = 0x08;

        MIDI_DRUM->buf_indx[START] = 2;
        MIDI_DRUM->buf_mask[START] = 0x10;
        MIDI_DRUM->buf_indx[SELECT] = 2;
        MIDI_DRUM->buf_mask[SELECT] = 0x20;

        MIDI_DRUM->midi_note[PICK] = 0x08;//this is a misuse of the midi note array, but it is a convenient place to keep what the "unpressed" state of the pick should be
        MIDI_DRUM->midi_note[WHAMMY_MSB] = 0x00;//similarly, this is an offset for the pitchbend
        break;
    }

}


static inline void old_off(MIDIDRUM* MIDI_DRUM)
{
    int i;
    MIDI_DRUM->velocity = MIDI_DRUM->default_velocity;
    for(i=GREEN;i<=ORANGE;i++)
    {
        if (MIDI_DRUM->prev_state[i])
            MIDI_DRUM->noteup(MIDI_DRUM->sequencer, MIDI_DRUM->channel, MIDI_DRUM->midi_note[i], 0);
        if (MIDI_DRUM->prev_state[i+HI_GREEN-GREEN])
            MIDI_DRUM->noteup(MIDI_DRUM->sequencer, MIDI_DRUM->channel, MIDI_DRUM->midi_note[i+HI_GREEN-GREEN], 0);
    }
}

//callback for rockband guitar
void cb_irq_rb_guitar(struct libusb_transfer *transfer)
{
    MIDIDRUM* MIDI_DRUM = (MIDIDRUM*)transfer->user_data; 
    if (transfer->status != LIBUSB_TRANSFER_COMPLETED) {
        fprintf(stderr, "irq transfer status %d? %d\n", transfer->status, LIBUSB_TRANSFER_ERROR);
        do_exit = 2;
        libusb_free_transfer(transfer);
        transfer = NULL;
        return;
    }
    int i,j,k;

    //RockBand 3 Guitar
    get_state(MIDI_DRUM,PICK);
    get_state(MIDI_DRUM,WHAMMY_LSB);
    get_state(MIDI_DRUM,WHAMMY_MSB);

    //TODO: add tapping mode when start pressed
    if(changed(MIDI_DRUM,PICK) && MIDI_DRUM->drum_state[PICK] != MIDI_DRUM->midi_note[PICK]) //PICK note isn't actually a midi note
    {
        //new notes
        get_state(MIDI_DRUM,RED);
        get_state(MIDI_DRUM,YELLOW);
        get_state(MIDI_DRUM,GREEN);
        get_state(MIDI_DRUM,BLUE);
        get_state(MIDI_DRUM,ORANGE);
        get_state(MIDI_DRUM,HINOTE);
        //first kill all old ones
        old_off(MIDI_DRUM); 
        //then send the new notes
        i=0;
        if(MIDI_DRUM->drum_state[HINOTE])
            i = HI_GREEN-GREEN;
        for(j=GREEN;j<=ORANGE;j++)
            if(MIDI_DRUM->drum_state[j])
            {
                MIDI_DRUM->notedown( MIDI_DRUM->sequencer, MIDI_DRUM->channel, MIDI_DRUM->midi_note[j+i], MIDI_DRUM->velocity);
                //swap the 2 so low notes and high notes are correct
                k = MIDI_DRUM->drum_state[i+j];
                MIDI_DRUM->drum_state[i+j] = MIDI_DRUM->drum_state[j];
                MIDI_DRUM->drum_state[j] = k;
                
            }
    }
    else
    {
        //stop any sounding notes that are let go
        for(j=GREEN;j<=ORANGE;j++)
        {
            if(MIDI_DRUM->prev_state[j]) 
            {
                get_state(MIDI_DRUM,j);
                if(!MIDI_DRUM->drum_state[j])
                    MIDI_DRUM->noteup(MIDI_DRUM->sequencer, MIDI_DRUM->channel, MIDI_DRUM->midi_note[j], 0);
            }
            if(MIDI_DRUM->prev_state[j+HI_GREEN-GREEN])
            {
                k = MIDI_DRUM->drum_state[j];
                get_state(MIDI_DRUM,j);
                i = HI_GREEN-GREEN;
                if(!MIDI_DRUM->drum_state[j])
                    MIDI_DRUM->noteup(MIDI_DRUM->sequencer, MIDI_DRUM->channel, MIDI_DRUM->midi_note[j+i], 0);
                MIDI_DRUM->drum_state[i+j] = MIDI_DRUM->drum_state[j];
                MIDI_DRUM->drum_state[j] = k;
            }
        }
        
    }
    
    //other controls
    if(changed(MIDI_DRUM,WHAMMY_LSB) || changed(MIDI_DRUM,WHAMMY_MSB))
    {
        short val = (MIDI_DRUM->drum_state[WHAMMY_MSB]+MIDI_DRUM->midi_note[WHAMMY_MSB])&0xff;
        val = (val<<5) + (MIDI_DRUM->drum_state[WHAMMY_LSB]>>3);
        if(MIDI_DRUM->drum_state[WHAMMY_MSB] == 0x7f && MIDI_DRUM->prev_state[WHAMMY_MSB] == 0x00)
            val = 0;//some set the whammy to 0x7f when it is released
        //val = 0x2000-val;
        val = -val;
        //printf("pitch %i %x\n",val,val);
        MIDI_DRUM->pitchbend(MIDI_DRUM->sequencer, MIDI_DRUM->channel, val);
    }

    get_state(MIDI_DRUM,SELECTOR);
    get_state(MIDI_DRUM,LEFT);
    get_state(MIDI_DRUM,RIGHT);

    if(changed(MIDI_DRUM,SELECTOR))
        MIDI_DRUM->control(MIDI_DRUM->sequencer, MIDI_DRUM->channel, 1, MIDI_DRUM->drum_state[SELECTOR]);//MOD WHEEL
    if(changed(MIDI_DRUM,LEFT))
    {
        if(!MIDI_DRUM->prog--)
            MIDI_DRUM->prog = 0;
        MIDI_DRUM->program(MIDI_DRUM->sequencer, MIDI_DRUM->channel, MIDI_DRUM->prog); 
    }
    if(changed(MIDI_DRUM,RIGHT))
    {
        if(MIDI_DRUM->prog++ > 126)
            MIDI_DRUM->prog = 127;
        MIDI_DRUM->program(MIDI_DRUM->sequencer, MIDI_DRUM->channel, MIDI_DRUM->prog); 
    }
        
    //now that the time-critical stuff is done, lets do the assignments 
    memcpy(MIDI_DRUM->prev_state,MIDI_DRUM->drum_state,NUM_DRUMS);
    
    if (MIDI_DRUM->verbose)
    {
    	print_guitar(MIDI_DRUM);
	//printf("\n");
    	print_buf(MIDI_DRUM);
    } 
    if (libusb_submit_transfer(transfer) < 0)
        do_exit = 2;
}
