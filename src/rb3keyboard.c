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

	MIDI_DRUM->buf_indx[ONE] = 0;
	MIDI_DRUM->buf_mask[ONE] = 0x04;
	MIDI_DRUM->buf_indx[B_BUTTON] = 0;
	MIDI_DRUM->buf_mask[B_BUTTON] = 0x04;

	MIDI_DRUM->buf_indx[UP] = 2;
	MIDI_DRUM->buf_mask[UP] = 0x08;
	MIDI_DRUM->buf_indx[DOWN] = 2;
	MIDI_DRUM->buf_mask[DOWN] = 0x04;
	MIDI_DRUM->buf_indx[LEFT] = 2
	MIDI_DRUM->buf_mask[LEFT] = 0x01;
	MIDI_DRUM->buf_indx[RIGHT] = 2;
	MIDI_DRUM->buf_mask[RIGHT] = 0x02;

	MIDI_DRUM->buf_indx[PLUS] = 1
	MIDI_DRUM->buf_mask[PLUS] = 0x01;
	MIDI_DRUM->buf_indx[MINUS] = 1;
	MIDI_DRUM->buf_mask[MINUS] = 0x02;

	MIDI_DRUM->buf_indx[A_BUTTON] = 0;
	MIDI_DRUM->buf_mask[A_BUTTON] = 0x02;
	MIDI_DRUM->buf_indx[B_BUTTON] = 0;
	MIDI_DRUM->buf_mask[B_BUTTON] = 0x04;
	MIDI_DRUM->buf_indx[ONE] = 0
	MIDI_DRUM->buf_mask[ONE] = 0x01;
	MIDI_DRUM->buf_indx[TWO] = 0;
	MIDI_DRUM->buf_mask[TWO] = 0x08;

	MIDI_DRUM->buf_indx[KEY0] = 5;
	MIDI_DRUM->buf_indx[KEY1] = 6;
	MIDI_DRUM->buf_indx[KEY2] = 7;
	MIDI_DRUM->buf_indx[KEY3] = 8;

	MIDI_DRUM->buf_indx[EXPRESSION] = 15;
	MIDI_DRUM->buf_mask[EXPRESSION] = 0x7f;
	MIDI_DRUM->buf_indx[EXPR_TOGGLE] = 13;
	MIDI_DRUM->buf_mask[EXPR_TOGGLE] = 0x80;
}

/*
+static inline void handle_button(MIDIDRUM* MIDI_DRUM)
+{
+    if ((MIDI_DRUM->button_state&BUTTON_MINUS) &&
+        !(MIDI_DRUM->prev_buttonstate&BUTTON_MINUS))
+        MIDI_DRUM->octave=max(MIDI_DRUM->octave-12, 0);
+     if ((MIDI_DRUM->button_state&BUTTON_PLUS) &&
+        !(MIDI_DRUM->prev_buttonstate&BUTTON_PLUS))
+        MIDI_DRUM->octave=min(MIDI_DRUM->octave+12, 108);
+     if ((MIDI_DRUM->button_state&BUTTON_KEYBOARD) &&
+        !(MIDI_DRUM->prev_buttonstate&BUTTON_KEYBOARD))
+        MIDI_DRUM->octave=MIDI_DRUM->octave=48;
+}
+static inline void handle_controller(MIDIDRUM* MIDI_DRUM)
+{
+    int prev_value=MIDI_DRUM->prev_controller_value & 0x7f;
+    int value=MIDI_DRUM->controller_value & 0x7f;
+
+    if (MIDI_DRUM->controller_value & 0x80) // lets do the pitch bend stuff
+    {
+        if ((MIDI_DRUM->controller_value & 0x7f) !=
+        (MIDI_DRUM->prev_controller_value & 0x7f))
+        {
+           //Here we try to implement some clever pitch bend stuff
+//        if ((MIDI_DRUM->prev_controller_value & 0x7f) > 0x0f)
+            //if !((MIDI_DRUM->controller_value & 0x7f) >0x40)
+        if ((prev_value-value)*(prev_value-value)<0x100)
+        {
+            MIDI_DRUM->pitchbend_value=min(max(MIDI_DRUM->pitchbend_value+
+            (prev_value-value),0),0x7f);
+            MIDI_DRUM->pitchbend(MIDI_DRUM->sequencer, MIDI_DRUM->channel, MIDI_DRUM->pitchbend_value);
+        }
+//        else
+//            MIDI_DRUM->pitchbend_value=0x2000;
+        }
+    }
+    else
+    {
+        if (MIDI_DRUM->prev_controller_value & 0x80)
+        {
+            MIDI_DRUM->pitchbend_value=0x40;
+            MIDI_DRUM->pitchbend(MIDI_DRUM->sequencer, MIDI_DRUM->channel, MIDI_DRUM->pitchbend_value);
+        }
+        //Send control event
+    }
+    printf ("pitcbend: %02x\n",MIDI_DRUM->pitchbend_value);
+}
*/

static inline void get_velocity(MIDIDRUM* MIDI_DRUM)
{
    //Velocity data is transmitted for up to 5 keys pressed same time.
    //This is the reason for the weird velocity reading algorithm.
    //For this reason the 6th key wont get any reasonable velocity data.
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
            MIDI_DRUM->notedown( MIDI_DRUM->sequencer, MIDI_DRUM->channel, 48+key+12*MIDI_DRUM->octave, MIDI_DRUM->velocity);
        else
            MIDI_DRUM->noteup(MIDI_DRUM->sequencer, MIDI_DRUM->channel, 48+key+12*MIDI_DRUM->octave, 0);
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
    //MIDI_DRUM->key_state=
    //(MIDI_DRUM->buf[5]<<17)+(MIDI_DRUM->buf[6]<<9)+
    //(MIDI_DRUM->buf[7]<<1)+((MIDI_DRUM->buf[8]&0x80)>>7);
    MIDI_DRUM->key_state=
      (MIDI_DRUM->buf[MIDI_DRUM->buf_indx[KEY0]]<<17)+(MIDI_DRUM->buf[MIDI_DRUM->buf_indx[KEY1]]<<9)+
      (MIDI_DRUM->buf[MIDI_DRUM->buf_indx[KEY2]]<<1)+((MIDI_DRUM->buf[MIDI_DRUM->buf_indx[KEY3]])>>7);
    //get_velocity(MIDI_DRUM);

    //The button next to control slider is stored in controller_value variable instead of button_state
    MIDI_DRUM->controller_value=(MIDI_DRUM->buf[15]&0x7f)+(MIDI_DRUM->buf[13]&0x80);
    MIDI_DRUM->button_state=
        (MIDI_DRUM->buf[0]<<16)+(MIDI_DRUM->buf[1]<<8)+
        MIDI_DRUM->buf[2];

    if (MIDI_DRUM->button_state != MIDI_DRUM->prev_buttonstate)
	handle_button(MIDI_DRUM);

    if (MIDI_DRUM->controller_value != MIDI_DRUM->prev_controller_value)
        handle_controller(MIDI_DRUM);

    //get_velocity(MIDI_DRUM);
    if (MIDI_DRUM->key_state != MIDI_DRUM->prev_keystate)
    {
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
    }

    //octave can get up to +-4 octaves
    //these buttons control octave
    get_state(MIDI_DRUM,RED);
    get_state(MIDI_DRUM,BLUE);

    if (MIDI_DRUM->verbose)
    {
        print_keys(MIDI_DRUM);
        print_buf(MIDI_DRUM);
    }
    MIDI_DRUM->prev_keystate=MIDI_DRUM->key_state; 
    MIDI_DRUM->prev_buttonstate=MIDI_DRUM->button_state;
    MIDI_DRUM->prev_controller_value=MIDI_DRUM->controller_value;
    
    if (libusb_submit_transfer(transfer) < 0)
        do_exit = 2;
}
