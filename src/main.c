/*
 * libusb rockband 2 drum interface
 * based on U.are.U 4000B fingerprint scanner.
 * Copyright (C) 2007 Daniel Drake <dsd@gentoo.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include "mididrum.h" 
#include "rbkit.h"
#include "rb1kit.h"
#include "rbguitar.h"
#include "ghkit.h"
#include "rb3keyboard.h"
#include "alsadriver.h"
#include "jackdriver.h"

unsigned char rb3_ps3_keyboard_init_code[40] = {0xe9,0,0x89,0x1b,0,0,0,0x02,0,0,0,0,0,0,0,0,0,0,0,0,0,0x80,0,0,0,0,0x89,0,0,0,0,0,0xe9,0x01,0,0,0,0,0,0};

static int claim_interface(struct libusb_device_handle **devh)
{
    int r;
    if (libusb_kernel_driver_active(*devh, 0)) {
        r = libusb_detach_kernel_driver(*devh, 0);
        if (r < 0) {
            printf("did not detach.\n");
        }
    }
    r = libusb_claim_interface(*devh, 0);
    if (r < 0) {
        fprintf(stderr, "usb_claim_interface error %d (busy=%d)\n", r, LIBUSB_ERROR_BUSY);
        //libusb_close(*devh);
        //libusb_exit(NULL);
        return -r;
    }
    printf("claimed interface\n");
    return 0;
}

static int find_rbdrum_device(MIDIDRUM* MIDI_DRUM, struct libusb_device_handle **devh)
{
    //DRUMS
    //PS3 RB kit
    *devh = libusb_open_device_with_vid_pid(NULL, 0x12ba, 0x0210);
    if(*devh){
        MIDI_DRUM->kit=PS_ROCKBAND;
        if(MIDI_DRUM->verbose)printf("PS3 Rockband kit found\n");
        if(claim_interface(devh) == 0)
            return 0;
    }

    //xbox RB kit
    *devh = libusb_open_device_with_vid_pid(NULL, 0x1bad, 0x0003);
    if(*devh){
        MIDI_DRUM->kit=XB_ROCKBAND;
        if(MIDI_DRUM->verbose)printf("XBox Rockband kit found\n");
        if(claim_interface(devh) == 0)
            return 0;
    }

    //Wìì RB1 kit
    *devh = libusb_open_device_with_vid_pid(NULL, 0x1bad, 0x0005);      
    if(*devh){
        MIDI_DRUM->kit=WII_ROCKBAND;
        if(MIDI_DRUM->verbose)printf("Wii Rockband1 kit found\n");
        if(claim_interface(devh) == 0)
            return 0;
    }

    //Wìì RB kit??
    *devh = libusb_open_device_with_vid_pid(NULL, 0x1bad, 0x3110);      
    if(*devh){
        MIDI_DRUM->kit=WII_ROCKBAND;
        if(MIDI_DRUM->verbose)printf("Wii Rockband kit found\n");
        if(claim_interface(devh) == 0)
            return 0;
    }

    //PS3 GH kit
    *devh = libusb_open_device_with_vid_pid(NULL, 0x12ba, 0x0120);
    if(*devh){
        MIDI_DRUM->kit=PS_GUITAR_HERO;
        if(MIDI_DRUM->verbose)printf("PS3 Guitar Hero kit found\n");
        if(claim_interface(devh) == 0)
            return 0;
    }

    //xbox360 GH kit/keytar
    *devh = libusb_open_device_with_vid_pid(NULL, 0x045e, 0x0291);
    if(*devh){
        MIDI_DRUM->kit=XB_GUITAR_HERO;
        if(MIDI_DRUM->verbose)printf("XBox Guitar Hero kit or keyboard found\n");
        if(claim_interface(devh) == 0)
            return 0;
    }

    //GUITARS
    //ps3
    *devh = libusb_open_device_with_vid_pid(NULL, 0x12ba, 0x0200);
    if(*devh){
        MIDI_DRUM->kit=PS_RB_GUITAR;
        if(MIDI_DRUM->verbose)printf("PS3 guitar found\n");
        if(claim_interface(devh) == 0)
            return 0;
    }

    //xbox360
    *devh = libusb_open_device_with_vid_pid(NULL, 0x1bad, 0x0002);
    if(*devh){
        MIDI_DRUM->kit=XB_RB_GUITAR;
        if(MIDI_DRUM->verbose)printf("XBox guitar found\n");
        if(claim_interface(devh) == 0)
            return 0;
    }

    //KEYBOARDS
    //Wii
    *devh = libusb_open_device_with_vid_pid(NULL, 0x1bad, 0x3330);
    if(*devh){
        MIDI_DRUM->kit=WII_RB3_KEYBOARD;
        if(MIDI_DRUM->verbose)printf("Wii keyboard found\n");
        if(claim_interface(devh) == 0)
            return 0;
    }

    //PS3
    *devh = libusb_open_device_with_vid_pid(NULL, 0x12ba, 0x2330);
    if(*devh){
        MIDI_DRUM->kit=PS3_RB3_KEYBOARD;
        if(MIDI_DRUM->verbose)printf("PS3 keyboard found\n");
        if(claim_interface(devh) == 0){
            // Send Special init code or keys won't output
            int res = libusb_control_transfer(*devh, 0x21, 0x09, 0x0300, 0, rb3_ps3_keyboard_init_code, sizeof(rb3_ps3_keyboard_init_code), 100);
            if(res == sizeof(rb3_ps3_keyboard_init_code))
                return 0;
            printf("PS3 Keyboard failed init: %s\n", libusb_error_name(res));
        }
    }

    return *devh ? 0 : -EIO;
}

void init_kit(MIDIDRUM* MIDI_DRUM)
{
    //initialize all values, just to be safe
    memset(MIDI_DRUM->buf_indx,0,NUM_DRUMS);
    memset(MIDI_DRUM->buf_mask,0,NUM_DRUMS);
    switch(MIDI_DRUM->kit)
    {
        case PS_ROCKBAND:
        case XB_ROCKBAND:
        case WII_ROCKBAND:
            init_rb_kit(MIDI_DRUM);
            break;
        case PS_ROCKBAND1:
        case XB_ROCKBAND1:
            init_rb1_kit(MIDI_DRUM);
            break;
        case XB_GUITAR_HERO:
        case PS_GUITAR_HERO:        
            init_gh_kit(MIDI_DRUM);
            break;
        case XB_RB_GUITAR:
        case PS_RB_GUITAR:
            init_rb_guitar(MIDI_DRUM);
            break;
        case PS3_RB3_KEYBOARD:
        case WII_RB3_KEYBOARD:
        case XB_RB3_KEYBOARD:
            init_rb3_keyboard(MIDI_DRUM);
            break;
    }
}

void print_hits(MIDIDRUM* MIDI_DRUM)
{
    if ( MIDI_DRUM->drum_state[RED] ||  
         MIDI_DRUM->drum_state[YELLOW] ||
         MIDI_DRUM->drum_state[BLUE] ||
         MIDI_DRUM->drum_state[GREEN] ||
         MIDI_DRUM->drum_state[ORANGE_CYMBAL] || 
         MIDI_DRUM->drum_state[YELLOW_CYMBAL] ||  
         MIDI_DRUM->drum_state[ORANGE_BASS]   ||
         MIDI_DRUM->drum_state[BLACK_BASS] ) {
         printf("%s %s %s %s %s %s %s %s\n",  
        MIDI_DRUM->drum_state[RED]>0?"VV":"  ",
                MIDI_DRUM->drum_state[YELLOW]>0?"VV":"  ", 
                MIDI_DRUM->drum_state[BLUE]>0?"VV":"  ", 
                MIDI_DRUM->drum_state[GREEN]>0?"VV":"  ",
                MIDI_DRUM->drum_state[ORANGE_CYMBAL]>0?"VV":"  ",
                MIDI_DRUM->drum_state[YELLOW_CYMBAL]>0?"VV":"  ",
                MIDI_DRUM->drum_state[ORANGE_BASS]>0?"VV":"  ",
                MIDI_DRUM->drum_state[BLACK_BASS]>0?"VV":"  "); 
        printf("%02i %02i %02i %02i %02i %02i %02i %02i\n", MIDI_DRUM->drum_state[RED],
              MIDI_DRUM->drum_state[YELLOW],
              MIDI_DRUM->drum_state[BLUE], 
              MIDI_DRUM->drum_state[GREEN], 
              MIDI_DRUM->drum_state[ORANGE_CYMBAL],
              MIDI_DRUM->drum_state[YELLOW_CYMBAL], 
              MIDI_DRUM->drum_state[ORANGE_BASS],
              MIDI_DRUM->drum_state[BLACK_BASS]);
    }
}

void print_buf(MIDIDRUM* MIDI_DRUM)
{
    if ( memcmp(MIDI_DRUM->oldbuf,MIDI_DRUM->buf,INTR_LENGTH))
    {
        printf("%02x%02x%02x%02x %02x%02x%02x%02x %02x%02x%02x%02x %02x%02x%02x%02x %02x%02x%02x%02x %02x%02x%02x%02x %02x%02x%02x kit type=%d\n",
               MIDI_DRUM->buf[0], MIDI_DRUM->buf[1], MIDI_DRUM->buf[2], MIDI_DRUM->buf[3], MIDI_DRUM->buf[4],
               MIDI_DRUM->buf[5], MIDI_DRUM->buf[6], MIDI_DRUM->buf[7], MIDI_DRUM->buf[8], MIDI_DRUM->buf[9],
               MIDI_DRUM->buf[10], MIDI_DRUM->buf[11], MIDI_DRUM->buf[12], MIDI_DRUM->buf[13], MIDI_DRUM->buf[14], 
               MIDI_DRUM->buf[15], MIDI_DRUM->buf[16], MIDI_DRUM->buf[17], MIDI_DRUM->buf[18], MIDI_DRUM->buf[19],
               MIDI_DRUM->buf[20], MIDI_DRUM->buf[21], MIDI_DRUM->buf[22], MIDI_DRUM->buf[23], MIDI_DRUM->buf[24], 
               MIDI_DRUM->buf[25], MIDI_DRUM->buf[26],MIDI_DRUM->kit);
    memcpy(MIDI_DRUM->oldbuf,MIDI_DRUM->buf,INTR_LENGTH);
    }
}

void print_guitar(MIDIDRUM* MIDI_DRUM)
{
    if ( MIDI_DRUM->drum_state[HINOTE] )
    {
        if ( MIDI_DRUM->drum_state[GREEN] )
            printf("%i ", MIDI_DRUM->midi_note[HI_GREEN]);
        if ( MIDI_DRUM->drum_state[RED] )
            printf("%i ", MIDI_DRUM->midi_note[HI_RED]);
        if ( MIDI_DRUM->drum_state[YELLOW] )
            printf("%i ", MIDI_DRUM->midi_note[HI_YELLOW]);
        if ( MIDI_DRUM->drum_state[BLUE] )
            printf("%i ", MIDI_DRUM->midi_note[HI_BLUE]);
        if ( MIDI_DRUM->drum_state[ORANGE] )
            printf("%i ", MIDI_DRUM->midi_note[HI_ORANGE]);
    }
    else
    {
        if ( MIDI_DRUM->drum_state[GREEN] )
            printf("%i ", MIDI_DRUM->midi_note[GREEN]);
        if ( MIDI_DRUM->drum_state[RED] )
            printf("%i ", MIDI_DRUM->midi_note[RED]);
        if ( MIDI_DRUM->drum_state[YELLOW] )
            printf("%i ", MIDI_DRUM->midi_note[YELLOW]);
        if ( MIDI_DRUM->drum_state[BLUE] )
            printf("%i ", MIDI_DRUM->midi_note[BLUE]);
        if ( MIDI_DRUM->drum_state[ORANGE] )
            printf("%i ", MIDI_DRUM->midi_note[ORANGE]);
    }
}

void print_keys(MIDIDRUM* MIDI_DRUM)
{
    //TODO
    if (MIDI_DRUM->key_state!=MIDI_DRUM->prev_keystate)
        printf("Keys: %08X %02x\n",MIDI_DRUM->key_state,MIDI_DRUM->velocity);
}

void midi_defaults(MIDIDRUM* MIDI_DRUM)
{
    int v;
    if(MIDI_DRUM->kit < DRUMS)
    {
        if(MIDI_DRUM->kit == XB_ROCKBAND1 || MIDI_DRUM->kit == PS_ROCKBAND1)
        {
            if(!MIDI_DRUM->midi_note[YELLOW])
                MIDI_DRUM->midi_note[YELLOW] = GM_CLOSED_HAT;
            if(!MIDI_DRUM->midi_note[BLUE])
                MIDI_DRUM->midi_note[BLUE] = GM_CRASH;
        }
        if(!MIDI_DRUM->midi_note[RED])
            MIDI_DRUM->midi_note[RED] = GM_SNARE; 
        if(!MIDI_DRUM->midi_note[YELLOW])
            MIDI_DRUM->midi_note[YELLOW] = GM_HI_TOM;
        if(!MIDI_DRUM->midi_note[BLUE])
            MIDI_DRUM->midi_note[BLUE] = GM_MID_TOM;
        if(!MIDI_DRUM->midi_note[GREEN])
            MIDI_DRUM->midi_note[GREEN] = GM_LO_TOM;
        if(!MIDI_DRUM->midi_note[YELLOW_CYMBAL])
            MIDI_DRUM->midi_note[YELLOW_CYMBAL] = GM_OPEN_HAT;
        if(!MIDI_DRUM->midi_note[GREEN_CYMBAL])
            MIDI_DRUM->midi_note[GREEN_CYMBAL] = GM_CRASH;
        if(!MIDI_DRUM->midi_note[BLUE_CYMBAL])
            MIDI_DRUM->midi_note[BLUE_CYMBAL] = GM_RIDE;
        if(!MIDI_DRUM->midi_note[ORANGE_CYMBAL])
            MIDI_DRUM->midi_note[ORANGE_CYMBAL] = GM_CRASH;
        if(!MIDI_DRUM->midi_note[ORANGE_BASS])
            MIDI_DRUM->midi_note[ORANGE_BASS] = GM_KICK;
        if(!MIDI_DRUM->midi_note[BLACK_BASS])
            MIDI_DRUM->midi_note[BLACK_BASS] = 0;
        if(!MIDI_DRUM->midi_note[OPEN_HAT])
            MIDI_DRUM->midi_note[OPEN_HAT] = GM_OPEN_HAT;
        if(!MIDI_DRUM->midi_note[CLOSED_HAT])
            MIDI_DRUM->midi_note[CLOSED_HAT] = GM_CLOSED_HAT;
    }
    else if(MIDI_DRUM->kit < GUITARS)
    {
        v = 0;
        if(MIDI_DRUM->bass_down)
            v = -12;//this will drop everything 1 octave
        //note defaults
        if(!MIDI_DRUM->midi_note[GREEN])
            MIDI_DRUM->midi_note[GREEN] = 53+v;//F
        if(!MIDI_DRUM->midi_note[RED])
            MIDI_DRUM->midi_note[RED] = 55+v; //G
        if(!MIDI_DRUM->midi_note[YELLOW])
            MIDI_DRUM->midi_note[YELLOW] = 57+v;//A
        if(!MIDI_DRUM->midi_note[BLUE])
            MIDI_DRUM->midi_note[BLUE] = 60+v; //C
        if(!MIDI_DRUM->midi_note[ORANGE])
            MIDI_DRUM->midi_note[ORANGE] = 62+v;//D
        if(!MIDI_DRUM->midi_note[HI_RED])
            MIDI_DRUM->midi_note[HI_RED] = MIDI_DRUM->midi_note[RED]+12;
        if(!MIDI_DRUM->midi_note[HI_YELLOW])
            MIDI_DRUM->midi_note[HI_YELLOW] = MIDI_DRUM->midi_note[YELLOW]+12;
        if(!MIDI_DRUM->midi_note[HI_GREEN])
            MIDI_DRUM->midi_note[HI_GREEN] = MIDI_DRUM->midi_note[GREEN]+12;
        if(!MIDI_DRUM->midi_note[HI_BLUE])
            MIDI_DRUM->midi_note[HI_BLUE] = MIDI_DRUM->midi_note[BLUE]+12;
        if(!MIDI_DRUM->midi_note[HI_ORANGE])
            MIDI_DRUM->midi_note[HI_ORANGE] = MIDI_DRUM->midi_note[ORANGE]+12;
    }
    //keyboards don't have midi note options
}


//debug mode callback
static void cb_irq_dbg(struct libusb_transfer *transfer)
{
    MIDIDRUM* MIDI_DRUM = (MIDIDRUM*)transfer->user_data; 
    if (transfer->status != LIBUSB_TRANSFER_COMPLETED) {
        fprintf(stderr, "irq transfer status %d? %d\n", transfer->status, LIBUSB_TRANSFER_ERROR);
        do_exit = 2;
        libusb_free_transfer(transfer);
        transfer = NULL;
        return;
    }

    print_buf(MIDI_DRUM);
    if (libusb_submit_transfer(transfer) < 0)
        do_exit = 2;
}

static int init_capture(struct libusb_transfer *irq_transfer)
{
    int r;

    r = libusb_submit_transfer(irq_transfer);
    if (r < 0)
        return r;

    return 6;
}

static int alloc_transfers(MIDIDRUM* MIDI_DRUM, libusb_device_handle *devh, struct libusb_transfer **irq_transfer)
{
    *irq_transfer = libusb_alloc_transfer(0);
    if (!*irq_transfer)
        return -ENOMEM;

    if(MIDI_DRUM->dbg){
        libusb_fill_interrupt_transfer(*irq_transfer, devh, EP_INTR, MIDI_DRUM->irqbuf,
            sizeof(MIDI_DRUM->irqbuf), cb_irq_dbg, (void*)MIDI_DRUM, 0);
        if( MIDI_DRUM->verbose)printf("Debug Mode Enabled..\n");
    }
    else if(MIDI_DRUM->kit == XB_RB3_KEYBOARD){
        libusb_fill_interrupt_transfer(*irq_transfer, devh, EP_INTR, MIDI_DRUM->irqbuf,
            sizeof(MIDI_DRUM->irqbuf), cb_irq_rb3_keyboard, (void*)MIDI_DRUM, 0);
        if( MIDI_DRUM->verbose)printf("Xbox Rock Band 3 Wireless keyboard detected.\n");
    }
    else if(MIDI_DRUM->kit == PS_ROCKBAND  || MIDI_DRUM->kit == XB_ROCKBAND ||
            MIDI_DRUM->kit == WII_ROCKBAND){
        libusb_fill_interrupt_transfer(*irq_transfer, devh, EP_INTR, MIDI_DRUM->irqbuf,
            sizeof(MIDI_DRUM->irqbuf), cb_irq_rb, (void*)MIDI_DRUM, 0);
        if( MIDI_DRUM->verbose)printf("Rock Band drum kit connected.\n");
    }
    else if(MIDI_DRUM->kit == PS_ROCKBAND1 || MIDI_DRUM->kit == XB_ROCKBAND1){
        libusb_fill_interrupt_transfer(*irq_transfer, devh, EP_INTR, MIDI_DRUM->irqbuf,
            sizeof(MIDI_DRUM->irqbuf), cb_irq_rb1, (void*)MIDI_DRUM, 0);
        if( MIDI_DRUM->verbose)printf("Rock Band 1 drum kit connected.\n");
    }
    else if(MIDI_DRUM->kit == PS_GUITAR_HERO || MIDI_DRUM->kit == XB_GUITAR_HERO){
        libusb_fill_interrupt_transfer(*irq_transfer, devh, EP_INTR, MIDI_DRUM->irqbuf,
            sizeof(MIDI_DRUM->irqbuf), cb_irq_gh, (void*)MIDI_DRUM, 0);
        if( MIDI_DRUM->verbose)printf("Guitar Hero World Tour drum kit connected.\n");
    }
    else if(MIDI_DRUM->kit == PS_RB_GUITAR || MIDI_DRUM->kit == XB_RB_GUITAR){
        libusb_fill_interrupt_transfer(*irq_transfer, devh, EP_INTR, MIDI_DRUM->irqbuf,
            sizeof(MIDI_DRUM->irqbuf), cb_irq_rb_guitar, (void*)MIDI_DRUM, 0);
        if( MIDI_DRUM->verbose)printf("Rock Band Guitar connected.\n");
    }
    else if(MIDI_DRUM->kit == WII_RB3_KEYBOARD){
        libusb_fill_interrupt_transfer(*irq_transfer, devh, EP_INTR, MIDI_DRUM->irqbuf,
            sizeof(MIDI_DRUM->irqbuf), cb_irq_rb3_keyboard, (void*)MIDI_DRUM, 0);
        if( MIDI_DRUM->verbose)printf("Wii Rock Band 3 Wireless keyboard detected.\n");
    }
    else if(MIDI_DRUM->kit == PS3_RB3_KEYBOARD){
        libusb_fill_interrupt_transfer(*irq_transfer, devh, EP_INTR, MIDI_DRUM->irqbuf,
            sizeof(MIDI_DRUM->irqbuf), cb_irq_rb3_keyboard, (void*)MIDI_DRUM, 0);
        if( MIDI_DRUM->verbose)printf("PS3 Rock Band 3 Wireless keyboard detected.\n");
    }
    else{
        printf("error in drum type! %i\n",MIDI_DRUM->kit);
    }

    return 0;
}

int init_jack(MIDIDRUM* MIDI_DRUM, JACK_SEQ* seq, unsigned char verbose)
{
    if(MIDI_DRUM->kit == PS_ROCKBAND  || MIDI_DRUM->kit == XB_ROCKBAND ||
            MIDI_DRUM->kit == WII_ROCKBAND){
        return init_jack_client(seq,verbose,"Rockband Drum Controller");
    }
    else if(MIDI_DRUM->kit == PS_ROCKBAND1 || MIDI_DRUM->kit == XB_ROCKBAND1){
        return init_jack_client(seq,verbose,"Rockband 1 Drum Controller");
    }
    else if(MIDI_DRUM->kit == PS_GUITAR_HERO || MIDI_DRUM->kit == XB_GUITAR_HERO){
        return init_jack_client(seq,verbose,"Guitar Hero Drum Controller");
    }
    else if(MIDI_DRUM->kit == PS_RB_GUITAR || MIDI_DRUM->kit == XB_RB_GUITAR){
        return init_jack_client(seq,verbose,"Rockband Guitar Controller");
    }
    else if(MIDI_DRUM->kit == WII_RB3_KEYBOARD || MIDI_DRUM->kit == XB_RB3_KEYBOARD){
        return init_jack_client(seq,verbose,"Rockband Keyboard Controller");
    }

    return 0;
}

void close_seq(ALSA_SEQ* aseq, JACK_SEQ* jseq, unsigned char seqtype)
{
    if(seqtype>=2){
        //jack
    close_jack(jseq);
    }
    else{
        //alsa
    close_alsa(aseq);
    }
}

static void sighandler(int signum)
{
    do_exit = 1;
}

void useage()
{
    printf("rbdrum2midi a rockband/guitar hero game instrument midi driver in userland\n");
    printf("  v 0.6\n");
    printf("\n");
    printf("USEAGE:\n");
    printf("    rbdrum2midi [-option <value>...]\n");
    printf("\n");
    printf("OPTIONS:\n");
    printf("    Drum Driver Options\n");
    printf("    -yvk                        note mapping for yamaha vintage kit in Hydrogen\n");
    printf("    -r/y/b/g <value>            set midi note for -color of drum\n");
    printf("    -ocy/ycy/bcy/gcy <value>    set midi note for -color of cymbal\n");
    printf("    -ob/bkb <value>             set midi note for -color bass pedal\n");
    printf("    -rb1                        specify rockband 1 drumset\n");
    printf("    -xbkey                      specify rockband 3 xbox keytar\n");
    printf("    -htdm <value>               set hihat color i.e, r/y.../bcy/gcy\n");
    printf("    -htp <value>                set hihat pedal color i.e. ob/bkb*\n"); 
    printf("    -hto <value>                set open hihat midi value of hihat mode drum\n");
    printf("    -htc <value>                set closed hihat midi value of hihat mode drum\n");
    printf("\n");
    printf("    Guitar Driver Options\n");
    printf("    -r/o/y/g/b <value>            set midi note for -color of button\n");
    printf("    -rhi/ohi/yhi/ghi/bhi <val>  set midi note for -color of upper button\n");
    printf("    -bg                         bass guitar mode\n");
    printf("\n");
    printf("    General Options\n");
    printf("    -vel <value>                set default note velocity\n");
    printf("    -c <value>                  set midi channel to send notes on\n");
    printf("    -dbg                        debug mode (no midi output)\n");
    printf("    -v                          verbose mode\n");
    printf("    -a                          use alsa sequencer for midi output\n");
    printf("    -j                          use JACK midi output\n");
    printf("    -h                          show this message\n");
    printf("\n");
    printf("EXAMPLES:\n");
    printf("    rbdrum2midi -r 16 -bcy 64 -ob 32 -g 17 \n");
    printf("    rbdrum2midi -bkb 0 -htp bkb -htdm ycy -hto 46 -htc 42\n");
    printf("    rbdrum2midi -v -rb1\n");
    printf("\n");
    printf("NOTES:\n");
    printf("    r=red, o=orange, y=yellow, g=green, b=blue, bk=black\n");
    printf("    cy=cymbal, b=bass pedal, ht=hihat, otherwise drum pad\n");
    printf("    the default midi values are for general midi drums\n\n");
    printf("    *using -htp option sets pedal in \"hihat mode,\" allows users to play\n");
    printf("     different notes on drum specified by -htdm dependent on pedal state\n"); 
    printf("     default is hat mode on black bass, use -htp 0 to disable\n"); 
    printf("     hihat mode can also play a sound when the pedal is closed\n");
    printf("     if you don't want the pedal sound, don't specify note for pedal\n");
    printf("\n");

    return;
}

int main(int argc, char **argv)
{
    struct sigaction sigact;
    struct libusb_device_handle *devh;
    struct libusb_transfer *irq_transfer;
    int r = 1;
    int i = 0;
    unsigned char seqtype = 0;
    MIDIDRUM MIDI_DRUM_;
    MIDIDRUM* MIDI_DRUM = &MIDI_DRUM_;
    ALSA_SEQ aseqq;
    JACK_SEQ jseqq;


    //struct ALSA_MIDI_SEQUENCER seq;//currently only alsa supported
    //MIDI_DRUM->sequencer = (void*)&seq;
    
    MIDI_DRUM->buf = MIDI_DRUM->irqbuf;
    //initial conditions, defaults
    MIDI_DRUM->bass_down = 0;
    MIDI_DRUM->default_velocity = 125;
    MIDI_DRUM->octave = 0;
    MIDI_DRUM->channel = DEFAULT_CHANNEL;
    MIDI_DRUM->prog = 0;
    MIDI_DRUM->verbose = 0;
    MIDI_DRUM->dbg = 0;
    MIDI_DRUM->kit = PS_ROCKBAND;
    MIDI_DRUM->hat_mode = BLACK_BASS;
    MIDI_DRUM->hat = YELLOW_CYMBAL;
    MIDI_DRUM->sequencer = 0;
    memset(MIDI_DRUM->oldbuf,0,INTR_LENGTH);
    memset(MIDI_DRUM->drum_state,0,NUM_DRUMS);
    memset(MIDI_DRUM->prev_state,0,NUM_DRUMS);
    memset(MIDI_DRUM->midi_note,0,NUM_DRUMS);
    
    //default midi values;
    /*
    MIDI_DRUM->midi_note[GREEN] = 53;//F
    MIDI_DRUM->midi_note[RED] = 55; //G
    MIDI_DRUM->midi_note[YELLOW] = 60; //C
    MIDI_DRUM->midi_note[BLUE] = 62;//D
    MIDI_DRUM->midi_note[ORANGE] = 69;//A
    MIDI_DRUM->midi_note[HI_RED] = MIDI_DRUM->midi_note[RED]+12;
    MIDI_DRUM->midi_note[HI_YELLOW] = MIDI_DRUM->midi_note[YELLOW]+12;
    MIDI_DRUM->midi_note[HI_GREEN] = MIDI_DRUM->midi_note[GREEN]+12;
    MIDI_DRUM->midi_note[HI_BLUE] = MIDI_DRUM->midi_note[BLUE]+12;
    MIDI_DRUM->midi_note[HI_ORANGE] = MIDI_DRUM->midi_note[ORANGE]+12;
    */

    if (argc > 1) {
        for (i = 1;i<argc;i++)
        {
            if (strcmp(argv[i], "-v") == 0) {
                 MIDI_DRUM->verbose = 1;
            }
            else if (strcmp(argv[i], "-rb1") == 0) {
                //rockband 1 set, use different irq routine
                MIDI_DRUM->kit = PS_ROCKBAND1;
            }
            else if (strcmp(argv[i], "-xbkey") == 0) {
                //rockband 3 keytar for xbox
                MIDI_DRUM->kit = XB_RB3_KEYBOARD;
            }
            else if (strcmp(argv[i], "-ocy") == 0) {
                //orange cymbal
                MIDI_DRUM->midi_note[ORANGE_CYMBAL] = atoi(argv[++i]);
            }
            else if (strcmp(argv[i], "-ycy") == 0) {
                //yellow cymbal
                MIDI_DRUM->midi_note[YELLOW_CYMBAL] = atoi(argv[++i]);
            }
            else if (strcmp(argv[i], "-gcy") == 0) {
                //green cymbal
                MIDI_DRUM->midi_note[GREEN_CYMBAL] = atoi(argv[++i]);
            }
            else if (strcmp(argv[i], "-bcy") == 0) {
                //blue cymbal
                MIDI_DRUM->midi_note[BLUE_CYMBAL] = atoi(argv[++i]);
            }
            else if (strcmp(argv[i], "-ob") == 0) {
                //orange bass
                MIDI_DRUM->midi_note[ORANGE_BASS] = atoi(argv[++i]);
            }
            else if (strcmp(argv[i], "-bkb") == 0) {
                //black bass
                MIDI_DRUM->midi_note[BLACK_BASS] = atoi(argv[++i]);
            }
            else if (strcmp(argv[i], "-r") == 0) {
                //red pad
                MIDI_DRUM->midi_note[RED] = atoi(argv[++i]);
                MIDI_DRUM->midi_note[HI_RED] = MIDI_DRUM->midi_note[RED]+12;
            }
            else if (strcmp(argv[i], "-y") == 0) {
                //yellow pad
                MIDI_DRUM->midi_note[YELLOW] = atoi(argv[++i]);
                MIDI_DRUM->midi_note[HI_YELLOW] = MIDI_DRUM->midi_note[YELLOW]+12;
            }
            else if (strcmp(argv[i], "-g") == 0) {
                //green pad
                MIDI_DRUM->midi_note[GREEN] = atoi(argv[++i]);
                MIDI_DRUM->midi_note[HI_GREEN] = MIDI_DRUM->midi_note[GREEN]+12;
            }
            else if (strcmp(argv[i], "-b") == 0) {
                //blue pad
                MIDI_DRUM->midi_note[BLUE] = atoi(argv[++i]);
                MIDI_DRUM->midi_note[HI_BLUE] = MIDI_DRUM->midi_note[BLUE]+12;
            }
            else if (strcmp(argv[i], "-o") == 0) {
                //blue pad
                MIDI_DRUM->midi_note[ORANGE] = atoi(argv[++i]);
                MIDI_DRUM->midi_note[HI_ORANGE] = MIDI_DRUM->midi_note[ORANGE]+12;
            }
            else if (strcmp(argv[i], "-vel") == 0) {
                r = atoi(argv[++i]);
                MIDI_DRUM->default_velocity = min(max(r,1),127); 
            }
            else if (strcmp(argv[i], "-c") == 0) {
                r = atoi(argv[++i]);
                MIDI_DRUM->channel = min(max(r,0),15); 
            }
            else if (strcmp(argv[i], "-htp") == 0){
                if (strcmp(argv[++i], "0" ) == 0)
                    MIDI_DRUM->hat_mode = 0; 
                else if (strcmp(argv[i], "ob") == 0){
                    if(MIDI_DRUM->midi_note[ORANGE_BASS] == GM_KICK)
                         MIDI_DRUM->midi_note[ORANGE_BASS] = 0;
                    MIDI_DRUM->hat_mode = ORANGE_BASS;
                }
                else if (strcmp(argv[i], "bkb") == 0) 
                    MIDI_DRUM->hat_mode = BLACK_BASS;
                else {
                    printf("ERROR! Unknown pedal for hi-hat! Using default black bass");
                    MIDI_DRUM->hat_mode = BLACK_BASS; 
                }
            }
            else if (strcmp(argv[i], "-htdm") == 0){
                if (strcmp(argv[++i], "ocy") == 0) 
                    MIDI_DRUM->hat = ORANGE_CYMBAL;
                else if (strcmp(argv[i], "ycy") == 0) 
                    MIDI_DRUM->hat = YELLOW_CYMBAL;
                else if (strcmp(argv[i], "gcy") == 0) 
                    MIDI_DRUM->hat = GREEN_CYMBAL;
                else if (strcmp(argv[i], "bcy") == 0) 
                    MIDI_DRUM->hat = BLUE_CYMBAL;
                else if (strcmp(argv[i], "r") == 0) 
                    MIDI_DRUM->hat = RED;
                else if (strcmp(argv[i], "y") == 0) 
                    MIDI_DRUM->hat = YELLOW;
                else if (strcmp(argv[i], "b") == 0) 
                    MIDI_DRUM->hat = RED;
                else if (strcmp(argv[i], "g") == 0) 
                    MIDI_DRUM->hat = GREEN;
                else{
                    printf("ERROR! Unknown drum for hi-hat! Using default yellow cymbal");
                    MIDI_DRUM->hat = YELLOW_CYMBAL;
                }
            }
            else if (strcmp(argv[i], "-hto") == 0){
                MIDI_DRUM->midi_note[OPEN_HAT] = atoi(argv[++i]);
            }
            else if (strcmp(argv[i], "-htc") == 0){ 
                MIDI_DRUM->midi_note[CLOSED_HAT] = atoi(argv[++i]);
            }
            else if (strcmp(argv[i], "-a") == 0) {
                //ALSA midi
                seqtype = 1;
            }
            else if (strcmp(argv[i], "-j") == 0) {
                //JACK midi
                seqtype = 2;
            }
            else if (strcmp(argv[i], "-yvk") == 0) {
                MIDI_DRUM->midi_note[RED] = YVK_SNARE; 
                MIDI_DRUM->midi_note[YELLOW] = YVK_HI_TOM;
                MIDI_DRUM->midi_note[BLUE] = YVK_MID_TOM;
                MIDI_DRUM->midi_note[GREEN] = YVK_LO_TOM;
                MIDI_DRUM->midi_note[YELLOW_CYMBAL] = YVK_OPEN_HAT;
                MIDI_DRUM->midi_note[GREEN_CYMBAL] = YVK_CRASH;
                MIDI_DRUM->midi_note[BLUE_CYMBAL] = YVK_RIDE;
                MIDI_DRUM->midi_note[ORANGE_CYMBAL] = YVK_CRASH;
                MIDI_DRUM->midi_note[ORANGE_BASS] = YVK_KICK;
                MIDI_DRUM->midi_note[BLACK_BASS] = 0;
                MIDI_DRUM->midi_note[OPEN_HAT] = YVK_OPEN_HAT;
                MIDI_DRUM->midi_note[CLOSED_HAT] = YVK_CLOSED_HAT;
            }
            else if (strcmp(argv[i], "-dbg") == 0) {
                //debug mode
                MIDI_DRUM->dbg = 1;
                MIDI_DRUM->verbose = 1;
            }
            else if (strcmp(argv[i], "-bg") == 0) {
                //debug mode
                MIDI_DRUM->bass_down = 1;
            }
            else if (strcmp(argv[i], "-h") == 0) {
                //help
                useage();
            }
            else{
                printf("Unknown argument! %s\n",argv[i]);
                useage();
            }
            //i = atoi(argv[1]);
        }
    }
    r = libusb_init(NULL);
    if (r < 0) {
        fprintf(stderr, "failed to initialise libusb\n");
        exit(1);
    }
    if(MIDI_DRUM->kit==PS_ROCKBAND1){
        //no way of knowing if device is RB1 so reassign kit after claiming
        r = find_rbdrum_device(MIDI_DRUM,&devh);
        switch(MIDI_DRUM->kit)
        {
        case PS_ROCKBAND:
        case WII_ROCKBAND:
              MIDI_DRUM->kit = PS_ROCKBAND1; 
            break;
        case XB_ROCKBAND:
            MIDI_DRUM->kit = XB_ROCKBAND1; 
            break;
        }
    }
	 //reassign xb rb3 keyboard if option selected
	 else if (MIDI_DRUM->kit==XB_RB3_KEYBOARD){
		  r = find_rbdrum_device(MIDI_DRUM,&devh);
 		  MIDI_DRUM->kit = XB_RB3_KEYBOARD;
	 }
    else
        r = find_rbdrum_device(MIDI_DRUM,&devh);
    if (r < 0) {
        fprintf(stderr, "Could not find/open device, try running as root?\n");
        libusb_close(devh);
        libusb_exit(NULL);
        return -r;
    }
    init_kit(MIDI_DRUM); 
    midi_defaults(MIDI_DRUM); 
    if(MIDI_DRUM->kit < DRUMS && MIDI_DRUM->hat_mode)
    {
        MIDI_DRUM->midi_note[MIDI_DRUM->hat] = MIDI_DRUM->midi_note[OPEN_HAT];
    }


    if(seqtype >=2){
        //jack
        r = init_jack(MIDI_DRUM,&jseqq,MIDI_DRUM->verbose);
        MIDI_DRUM->sequencer = (void*)&jseqq;
        MIDI_DRUM->noteup = noteup_jack;
        MIDI_DRUM->notedown = notedown_jack;
        MIDI_DRUM->pitchbend = pitch_jack;
        MIDI_DRUM->control = control_jack;
        MIDI_DRUM->program = prog_jack;
    }
    else{
        r = init_alsa(&aseqq,MIDI_DRUM->verbose);
        MIDI_DRUM->sequencer = (void*)&aseqq;
        MIDI_DRUM->noteup = noteup_alsa;
        MIDI_DRUM->notedown = notedown_alsa;
        MIDI_DRUM->pitchbend = pitch_alsa;
        MIDI_DRUM->control = control_alsa;
        MIDI_DRUM->program = prog_alsa;
    }
    // 0 is fail.
    if (r == 0) {
        printf("Error: MIDI driver setup failed.\n");
        return 1;
    }

    /* async from here onwards */

    r = alloc_transfers(MIDI_DRUM, devh, &irq_transfer);
    if (r < 0) {
        // Deinit & Release
        libusb_free_transfer(irq_transfer);
        libusb_release_interface(devh, 0);
        libusb_close(devh);
        libusb_exit(NULL); 
        close_seq(&aseqq,&jseqq,seqtype);
        printf("alloc_transfers failed.\n");
        return -r;
    }

    r = init_capture(irq_transfer);
    if (r < 0) {
        // Deinit & Release
        libusb_free_transfer(irq_transfer);
        libusb_release_interface(devh, 0);
        libusb_close(devh);
        libusb_exit(NULL);
        close_seq(&aseqq,&jseqq,seqtype);
        printf("init_capture failed.\n");
        return -r;
    }

    sigact.sa_handler = sighandler;
    sigemptyset(&sigact.sa_mask);
    sigact.sa_flags = SA_RESTART;
    r = sigaction(SIGINT, &sigact, NULL);
    r = sigaction(SIGTERM, &sigact, NULL);
    r = sigaction(SIGQUIT, &sigact, NULL);

    while (!do_exit) {
        r = libusb_handle_events(NULL);
        if (r < 0) {
            break;
        }
    }

    printf("shutting down...\n");

    if (irq_transfer) {
        r = libusb_cancel_transfer(irq_transfer);
        if (r < 0) {
            // Deinit & Release
            libusb_free_transfer(irq_transfer);
            libusb_release_interface(devh, 0);
            libusb_close(devh);
            libusb_exit(NULL);
            close_seq(&aseqq,&jseqq,seqtype);
            printf("libusb_cancel_transfer failed.\n");
            return -r;
        }
    }

//leftover transfers are handled in callbacks
    // || img_transfer
    while (do_exit!=2)//(irq_transfer)
        if (libusb_handle_events(NULL) < 0)
            break;


    if (do_exit == 1)
        r = 0;
    else
        r = 1;

//out_deinit: 
//    libusb_free_transfer(irq_transfer); 
//out_release:
    libusb_release_interface(devh, 0);
//out:
    libusb_close(devh);
    libusb_exit(NULL);
    close_seq(&aseqq,&jseqq,seqtype); 
    
    return r >= 0 ? r : -r;
}

