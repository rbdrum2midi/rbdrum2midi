//janifr
//based on rbkit.c by ssj71
//rbkit.c
#include "rb3keyboard.h"
//#include "constants.h"

void init_rb3_keyboard(MIDIDRUM* MIDI_DRUM)
{
    MIDI_DRUM->key_state=0;
    MIDI_DRUM->prev_keystate=0;
    MIDI_DRUM->velocity=0;
    MIDI_DRUM->channel=0;
}

static inline void get_velocity(MIDIDRUM* MIDI_DRUM)
{
    if (MIDI_DRUM->buf[12]!=0)
	MIDI_DRUM->velocity=MIDI_DRUM->buf[12];
    else if (MIDI_DRUM->buf[11]!=0)
	MIDI_DRUM->velocity=MIDI_DRUM->buf[11];
    else if (MIDI_DRUM->buf[10]!=0)
        MIDI_DRUM->velocity=MIDI_DRUM->buf[10];
    else if (MIDI_DRUM->buf[9]!=0)
        MIDI_DRUM->velocity=MIDI_DRUM->buf[9];
    else 
	MIDI_DRUM->velocity=MIDI_DRUM->buf[8]&0x7f;
}

static inline void handle_key(MIDIDRUM* MIDI_DRUM, unsigned char key)
{
	//printf("%d\n",1<<(24-key));
    if ((MIDI_DRUM->key_state&(1<<(24-key))) != 
	(MIDI_DRUM->prev_keystate&(1<<(24-key))))
    {
        get_velocity(MIDI_DRUM);
	printf("key %d\n",key);
	if (MIDI_DRUM->key_state&(1<<(24-key)))
	    MIDI_DRUM->notedown( MIDI_DRUM->sequencer, MIDI_DRUM->channel, 48+key, MIDI_DRUM->velocity);
	else
	    MIDI_DRUM->noteup(MIDI_DRUM->sequencer, MIDI_DRUM->channel, 48+key, 0);
   }
}

//callback for rockband3 keyboard
void cb_irq_rb3_keyboard(struct libusb_transfer *transfer)
{
    MIDIDRUM* MIDI_DRUM = (MIDIDRUM*)transfer->user_data; 
    if (transfer->status != LIBUSB_TRANSFER_COMPLETED) {
        fprintf(stderr, "irq transfer status %d? %d\n", transfer->status, LIBUSB_TRANSFER_ERROR);
        do_exit = 2;
        libusb_free_transfer(transfer);
        transfer = NULL;
        return;
    }
    MIDI_DRUM->key_state=
	(MIDI_DRUM->buf[5]<<17)+(MIDI_DRUM->buf[6]<<9)+
	(MIDI_DRUM->buf[7]<<1)+((MIDI_DRUM->buf[8]&0x80)>>7);
    //get_velocity(MIDI_DRUM);

    handle_key(MIDI_DRUM,0);
    handle_key(MIDI_DRUM,1);
    handle_key(MIDI_DRUM,2);
    handle_key(MIDI_DRUM,3);
    handle_key(MIDI_DRUM,4);
    handle_key(MIDI_DRUM,5);
    handle_key(MIDI_DRUM,6);
    handle_key(MIDI_DRUM,7);
    handle_key(MIDI_DRUM,8);
    handle_key(MIDI_DRUM,9);
    handle_key(MIDI_DRUM,10);
    handle_key(MIDI_DRUM,11);
    handle_key(MIDI_DRUM,12);
    handle_key(MIDI_DRUM,13);
    handle_key(MIDI_DRUM,14);
    handle_key(MIDI_DRUM,15);
    handle_key(MIDI_DRUM,16);
    handle_key(MIDI_DRUM,17);
    handle_key(MIDI_DRUM,18);
    handle_key(MIDI_DRUM,19);
    handle_key(MIDI_DRUM,20);
    handle_key(MIDI_DRUM,21);
    handle_key(MIDI_DRUM,22);
    handle_key(MIDI_DRUM,23);
    handle_key(MIDI_DRUM,24);

    if (MIDI_DRUM->verbose)
    {
        print_keys(MIDI_DRUM);
	print_buf(MIDI_DRUM);
    }
    MIDI_DRUM->prev_keystate=MIDI_DRUM->key_state; 
    
    if (libusb_submit_transfer(transfer) < 0)
        do_exit = 2;
}
