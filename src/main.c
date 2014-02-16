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

#include <errno.h>
#include <signal.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include <libusb-1.0/libusb.h>

#include <alsa/asoundlib.h>
#include <alsa/seq.h>


#define EP_INTR			(1 | LIBUSB_ENDPOINT_IN)
#define INTR_LENGTH		27

#define DEFAULT_CHANNEL 9

#define max(a, b) (((a) > (b)) ? (a) : (b))
#define min(a, b) (((a) < (b)) ? (a) : (b))

#define YVK_KICK        36
#define YVK_SNARE       37
#define YVK_LO_TOM      38
#define YVK_MID_TOM     39
#define YVK_HI_TOM      40
#define YVK_CLOSED_HAT  41
#define YVK_OPEN_HAT    42
#define YVK_RIDE        43
#define YVK_CRASH       45

enum drums{
    RED = 0,
    YELLOW,
    BLUE,
    GREEN,
    YELLOW_CYMBAL,
    BLUE_CYMBAL,
    GREEN_CYMBAL,
    ORANGE_CYMBAL,
    ORANGE_BASS,
    BLACK_BASS
};

enum kit_types{
    UNKNOWN = 0,
    PS_ROCKBAND,
    XB_ROCKBAND,
    WII_ROCKBAND,
    XB_ROCKBAND1,
    PS_ROCKBAND1,
    GUITAR_HERO
};

static struct drum_midi
{
    unsigned char kit;
    unsigned char midi_note[10];
    unsigned char buf_indx[10];
    unsigned char buf_mask[10];
    unsigned char *buf;
    unsigned char drum_state[10];
    unsigned char prev_state[10];
    snd_seq_t *g_seq;
    int g_port;

    int verbose;
    unsigned char dbg;
    int do_exit;
    unsigned char bass_down;
    int velocity;
    unsigned char irqbuf[INTR_LENGTH];
    unsigned char oldbuf[INTR_LENGTH];
    struct libusb_device_handle *devh;
    struct libusb_transfer *irq_transfer;

//function pointers
    void (*calc_velocity)(unsigned char);
    void (*handle_bass)(unsigned char);
}MIDI_DRUM;


void notedown(snd_seq_t *seq, int port, int chan, int pitch, int vol);
void noteup(snd_seq_t *seq, int port, int chan, int pitch, int vol);


static int find_rbdrum_device(int i)
{
    // TODO: Currently the i argument is ignored.
    //PS3 RB kit
    MIDI_DRUM.devh = libusb_open_device_with_vid_pid(NULL, 0x12ba, 0x0210);
    if(MIDI_DRUM.devh){
        MIDI_DRUM.kit=PS_ROCKBAND;
        return 0;
	}

    //xbox RB kit
    MIDI_DRUM.devh = libusb_open_device_with_vid_pid(NULL, 0x1bad, 0x0003);
    if(MIDI_DRUM.devh){
        MIDI_DRUM.kit=XB_ROCKBAND;
        return 0;
	}

    //Wìì RB kit??
    MIDI_DRUM.devh = libusb_open_device_with_vid_pid(NULL, 0x1bad, 0x0005);      
    if(MIDI_DRUM.devh){
        MIDI_DRUM.kit=WII_ROCKBAND;
        return 0;
	}

    //PS3 GH kit
    MIDI_DRUM.devh = libusb_open_device_with_vid_pid(NULL, 0x12ba, 0x0120);
    if(MIDI_DRUM.devh){
        MIDI_DRUM.kit=GUITAR_HERO;
    }
  

    return MIDI_DRUM.devh ? 0 : -EIO;
}


static int do_sync_intr(unsigned char *data)
{
    int r;
    int transferred;

    r = libusb_interrupt_transfer(MIDI_DRUM.devh, EP_INTR, data, INTR_LENGTH,
        &transferred, 1000);
    if (r < 0) {
        fprintf(stderr, "intr error %d\n", r);
        return r;
    }
    if (transferred < INTR_LENGTH) {
        fprintf(stderr, "short read (%d)\n", r);
        return -1;
    }

    printf("recv interrupt %04x\n", *((uint16_t *) data));
    return 0;
}

static int sync_intr(unsigned char type)
{
    int r;
    unsigned char data[INTR_LENGTH];

    while (1) {
        r = do_sync_intr(data);
        if (r < 0)
            return r;
        if (data[0] == type)
            return 0;
    }
}

void get_state(unsigned char drum)
{
    MIDI_DRUM.drum_state[drum] = MIDI_DRUM.buf[MIDI_DRUM.buf_indx[drum]] & MIDI_DRUM.buf_mask[drum];
}

void calc_velocity_gh(unsigned char value)
{
    MIDI_DRUM.velocity = min(max(value * 2, 0), 127);
}

void calc_velocity_rb(unsigned char value)
{
    MIDI_DRUM.velocity = min(max((280-value) * 2, 0), 127);
}

void calc_velocity_rb1(unsigned char value)
{
    MIDI_DRUM.velocity = 125;
}

void handle_drum(unsigned char drum)
{
   if (MIDI_DRUM.drum_state[drum] && !MIDI_DRUM.prev_state[drum]) {
       MIDI_DRUM.calc_velocity(MIDI_DRUM.drum_state[drum]);
       noteup(MIDI_DRUM.g_seq, MIDI_DRUM.g_port, DEFAULT_CHANNEL, MIDI_DRUM.midi_note[drum], -1);
       notedown( MIDI_DRUM.g_seq,  MIDI_DRUM.g_port, DEFAULT_CHANNEL, MIDI_DRUM.midi_note[drum], MIDI_DRUM.velocity);
            }
}

void handle_bass_rb(unsigned char drum)
{
    if (MIDI_DRUM.drum_state[drum] != MIDI_DRUM.prev_state[drum]) {
        if (MIDI_DRUM.drum_state[drum]) {
            MIDI_DRUM.velocity = 125;
            notedown( MIDI_DRUM.g_seq,  MIDI_DRUM.g_port, DEFAULT_CHANNEL, MIDI_DRUM.midi_note[drum],  MIDI_DRUM.velocity);
        }
        // Up
        else {
            noteup( MIDI_DRUM.g_seq,  MIDI_DRUM.g_port, DEFAULT_CHANNEL, MIDI_DRUM.midi_note[drum], 0);
        }
    }
}

void handle_bass_rb1(unsigned char drum)
{
    if (MIDI_DRUM.drum_state[drum] && !MIDI_DRUM.prev_state[drum]) {
        MIDI_DRUM.bass_down = !MIDI_DRUM.bass_down;
        // Events:
        // Down
        if (MIDI_DRUM.bass_down) {
            MIDI_DRUM.velocity = 125;
            notedown( MIDI_DRUM.g_seq,  MIDI_DRUM.g_port, DEFAULT_CHANNEL, MIDI_DRUM.midi_note[drum],  MIDI_DRUM.velocity);
            }
        // Up
        else {
            noteup( MIDI_DRUM.g_seq,  MIDI_DRUM.g_port, DEFAULT_CHANNEL, MIDI_DRUM.midi_note[drum], 0);
            }
        }
}

void init_midi_drum()
{
    switch(MIDI_DRUM.kit)
    {
        case PS_ROCKBAND:
	case XB_ROCKBAND:
	case WII_ROCKBAND:
            MIDI_DRUM.calc_velocity = &calc_velocity_rb;
	    MIDI_DRUM.handle_bass = &handle_bass_rb;
	    MIDI_DRUM.buf_indx[RED] = 12;
	    MIDI_DRUM.buf_mask[RED] = 0xFF;
	    MIDI_DRUM.buf_indx[YELLOW] = 11;
	    MIDI_DRUM.buf_mask[YELLOW] = 0xFF;
	    MIDI_DRUM.buf_indx[BLUE] = 14;
	    MIDI_DRUM.buf_mask[BLUE] = 0xFF;
	    MIDI_DRUM.buf_indx[GREEN] = 13;
	    MIDI_DRUM.buf_mask[GREEN] = 0xFF;
	    MIDI_DRUM.buf_indx[YELLOW_CYMBAL] = 1;
	    MIDI_DRUM.buf_mask[YELLOW_CYMBAL] = 0x08;
	    MIDI_DRUM.buf_indx[BLUE_CYMBAL] = 1;
	    MIDI_DRUM.buf_mask[BLUE_CYMBAL] = 0x08;
	    MIDI_DRUM.buf_indx[GREEN_CYMBAL] = 1;
	    MIDI_DRUM.buf_mask[GREEN_CYMBAL] = 0x08;
	    MIDI_DRUM.buf_indx[ORANGE_BASS] = 0;
	    MIDI_DRUM.buf_mask[ORANGE_BASS] = 0x10;
	    MIDI_DRUM.buf_indx[BLACK_BASS] = 0;
	    MIDI_DRUM.buf_mask[BLACK_BASS] = 0x20;
	    break;
        case PS_ROCKBAND1:
            MIDI_DRUM.calc_velocity = &calc_velocity_rb1;
	    MIDI_DRUM.handle_bass = &handle_bass_rb1;
	    MIDI_DRUM.buf_indx[RED] = 0;
	    MIDI_DRUM.buf_mask[RED] = 0x04;
	    MIDI_DRUM.buf_indx[YELLOW] = 0;
	    MIDI_DRUM.buf_mask[YELLOW] = 0x08;
	    MIDI_DRUM.buf_indx[BLUE] = 0;
	    MIDI_DRUM.buf_mask[BLUE] = 0x01;
	    MIDI_DRUM.buf_indx[GREEN] = 0;
	    MIDI_DRUM.buf_mask[GREEN] = 0x02;
	    MIDI_DRUM.buf_indx[YELLOW_CYMBAL] = 7;//this is a dummy value, always empty
	    MIDI_DRUM.buf_mask[YELLOW_CYMBAL] = 0x00;
	    MIDI_DRUM.buf_indx[ORANGE_BASS] = 0;
	    MIDI_DRUM.buf_mask[ORANGE_BASS] = 0x10;
	    MIDI_DRUM.buf_indx[BLACK_BASS] = 7;
	    MIDI_DRUM.buf_mask[BLACK_BASS] = 0x00;
	    break;
	case XB_ROCKBAND1:
            MIDI_DRUM.calc_velocity = &calc_velocity_rb1;
	    MIDI_DRUM.handle_bass = &handle_bass_rb1;
	    MIDI_DRUM.buf_indx[RED] = 3;
	    MIDI_DRUM.buf_mask[RED] = 0x20;
	    MIDI_DRUM.buf_indx[YELLOW] = 3;
	    MIDI_DRUM.buf_mask[YELLOW] = 0x80;
	    MIDI_DRUM.buf_indx[BLUE] = 3;
	    MIDI_DRUM.buf_mask[BLUE] = 0x40;
	    MIDI_DRUM.buf_indx[GREEN] = 3;
	    MIDI_DRUM.buf_mask[GREEN] = 0x10;
	    MIDI_DRUM.buf_indx[YELLOW_CYMBAL] = 7;
	    MIDI_DRUM.buf_mask[YELLOW_CYMBAL] = 0x00;
	    MIDI_DRUM.buf_indx[ORANGE_BASS] = 3;
	    MIDI_DRUM.buf_mask[ORANGE_BASS] = 0x01;
	    MIDI_DRUM.buf_indx[BLACK_BASS] = 7;
	    MIDI_DRUM.buf_mask[BLACK_BASS] = 0x00;
	    break;
        case GUITAR_HERO:
	    MIDI_DRUM.calc_velocity = &calc_velocity_gh;
	    MIDI_DRUM.buf_indx[RED] = 12;
	    MIDI_DRUM.buf_mask[RED] = 0xFF;	
	    MIDI_DRUM.buf_indx[YELLOW_CYMBAL] = 11;
	    MIDI_DRUM.buf_mask[YELLOW_CYMBAL] = 0xff;
	    MIDI_DRUM.buf_indx[BLUE] = 14;
	    MIDI_DRUM.buf_mask[BLUE] = 0xff;
	    MIDI_DRUM.buf_indx[GREEN] = 13;
	    MIDI_DRUM.buf_mask[GREEN] = 0xff;
	    MIDI_DRUM.buf_indx[ORANGE_CYMBAL] = 16;
	    MIDI_DRUM.buf_mask[ORANGE_CYMBAL] = 0xff;
	    MIDI_DRUM.buf_indx[ORANGE_BASS] = 15;
	    MIDI_DRUM.buf_mask[ORANGE_BASS] = 0xff;
	    break;
    }
}

//guitar hero callback
static void cb_irq_gh(struct libusb_transfer *transfer)
{
    if (transfer->status != LIBUSB_TRANSFER_COMPLETED) {
        fprintf(stderr, "irq transfer status %d? %d\n", transfer->status, LIBUSB_TRANSFER_ERROR);
        MIDI_DRUM.do_exit = 2;
        libusb_free_transfer(transfer);
        MIDI_DRUM.irq_transfer = NULL;
        return;
    }

    //Guitar Hero Drumkit
    get_state(RED);
    get_state(YELLOW_CYMBAL);
    get_state(GREEN);
    get_state(BLUE);
    get_state(ORANGE_CYMBAL);
    get_state(ORANGE_BASS);

    handle_drum(RED);
    handle_drum(YELLOW_CYMBAL);
    handle_drum(GREEN);
    handle_drum(BLUE);
    handle_drum(ORANGE_CYMBAL); 
    handle_drum(ORANGE_BASS);
        
	//now that the time-critical stuff is done, lets do the assignments
       MIDI_DRUM.prev_state[RED] = MIDI_DRUM.drum_state[RED];
       MIDI_DRUM.prev_state[YELLOW_CYMBAL] = MIDI_DRUM.drum_state[YELLOW_CYMBAL];
       MIDI_DRUM.prev_state[BLUE] = MIDI_DRUM.drum_state[BLUE];
       MIDI_DRUM.prev_state[GREEN] = MIDI_DRUM.drum_state[GREEN];
       MIDI_DRUM.prev_state[ORANGE_CYMBAL] = MIDI_DRUM.drum_state[ORANGE_CYMBAL]; 
       MIDI_DRUM.prev_state[ORANGE_BASS] = MIDI_DRUM.drum_state[ORANGE_BASS];
                             
        if ( MIDI_DRUM.verbose && ( MIDI_DRUM.drum_state[RED] ||  
	                            MIDI_DRUM.drum_state[YELLOW_CYMBAL] ||
				    MIDI_DRUM.drum_state[BLUE] ||
				    MIDI_DRUM.drum_state[GREEN] ||
				    MIDI_DRUM.drum_state[ORANGE_CYMBAL] ||
				    MIDI_DRUM.drum_state[ORANGE_BASS]  )) {
            printf("%s %s %s %s %s %s \n",  MIDI_DRUM.drum_state[RED]>0?"VV":"  ",
	                                    MIDI_DRUM.drum_state[YELLOW_CYMBAL]>0?"VV":"  ",
					    MIDI_DRUM.drum_state[BLUE]>0?"VV":"  ", 
					    MIDI_DRUM.drum_state[GREEN]>0?"VV":"  ",
					    MIDI_DRUM.drum_state[ORANGE_CYMBAL]>0?"VV":"  ", 
					    MIDI_DRUM.drum_state[ORANGE_BASS]>0?"VV":"  ");
            printf("%02i %02i %02i %02i %02i %02i\n", MIDI_DRUM.drum_state[RED],
	                                              MIDI_DRUM.drum_state[YELLOW_CYMBAL], 
						      MIDI_DRUM.drum_state[BLUE], 
						      MIDI_DRUM.drum_state[GREEN], 
						      MIDI_DRUM.drum_state[ORANGE_CYMBAL], 
						      MIDI_DRUM.drum_state[ORANGE_BASS]);
        }

    if ( MIDI_DRUM.verbose && memcmp(MIDI_DRUM.oldbuf,MIDI_DRUM.buf,INTR_LENGTH))
    {
        printf("%02x%02x%02x%02x %02x%02x%02x%02x %02x%02x%02x%02x %02x%02x%02x%02x %02x%02x%02x%02x %02x%02x%02x%02x %02x%02x%02x kit type=%d\n",
               MIDI_DRUM.buf[0], MIDI_DRUM.buf[1], MIDI_DRUM.buf[2], MIDI_DRUM.buf[3], MIDI_DRUM.buf[4],
	       MIDI_DRUM.buf[5], MIDI_DRUM.buf[6], MIDI_DRUM.buf[7], MIDI_DRUM.buf[8], MIDI_DRUM.buf[9],
               MIDI_DRUM.buf[10], MIDI_DRUM.buf[11], MIDI_DRUM.buf[12], MIDI_DRUM.buf[13], MIDI_DRUM.buf[14], 
	       MIDI_DRUM.buf[15], MIDI_DRUM.buf[16], MIDI_DRUM.buf[17], MIDI_DRUM.buf[18], MIDI_DRUM.buf[19],
               MIDI_DRUM.buf[20], MIDI_DRUM.buf[21], MIDI_DRUM.buf[22], MIDI_DRUM.buf[23], MIDI_DRUM.buf[24], 
	       MIDI_DRUM.buf[25], MIDI_DRUM.buf[26],MIDI_DRUM.kit);
	memcpy(MIDI_DRUM.oldbuf,MIDI_DRUM.buf,INTR_LENGTH);
    }
    if (libusb_submit_transfer(MIDI_DRUM.irq_transfer) < 0)
        MIDI_DRUM.do_exit = 2;
}

//interrupt callback for RB1&3 drumkit
static void cb_irq_rb(struct libusb_transfer *transfer)
{
    if (transfer->status != LIBUSB_TRANSFER_COMPLETED) {
        fprintf(stderr, "irq transfer status %d? %d\n", transfer->status, LIBUSB_TRANSFER_ERROR);
        MIDI_DRUM.do_exit = 2;
        libusb_free_transfer(transfer);
        MIDI_DRUM.irq_transfer = NULL;
        return;
    }

    //RockBand 3 Drumkit
    get_state(RED);
    get_state(YELLOW);
    get_state(GREEN);
    get_state(BLUE);
    get_state(YELLOW_CYMBAL);
    get_state(BLUE_CYMBAL);
    get_state(GREEN_CYMBAL);
    get_state(ORANGE_BASS);
    get_state(BLACK_BASS);

    handle_drum(RED);
   
    if(MIDI_DRUM.drum_state[YELLOW]){
    	if(MIDI_DRUM.drum_state[YELLOW_CYMBAL]){
        	handle_drum(YELLOW_CYMBAL);
        }
        else{
        	handle_drum(YELLOW);
        }
    }    
    if(MIDI_DRUM.drum_state[BLUE]){
    	if(MIDI_DRUM.drum_state[BLUE_CYMBAL]){
        	handle_drum(BLUE_CYMBAL);
        }
        else{
        	handle_drum(BLUE);
        }
    }   
    if(MIDI_DRUM.drum_state[GREEN]){
    	if(MIDI_DRUM.drum_state[GREEN_CYMBAL]){
        	handle_drum(GREEN_CYMBAL);
        }
        else{
        	handle_drum(GREEN);
        }
    }   



    MIDI_DRUM.handle_bass(ORANGE_BASS);
    MIDI_DRUM.handle_bass(BLACK_BASS);
        
	//now that the time-critical stuff is done, lets do the assignments
       MIDI_DRUM.prev_state[RED] = MIDI_DRUM.drum_state[RED];
       MIDI_DRUM.prev_state[YELLOW] = MIDI_DRUM.drum_state[YELLOW];
       MIDI_DRUM.prev_state[BLUE] = MIDI_DRUM.drum_state[BLUE];
       MIDI_DRUM.prev_state[GREEN] = MIDI_DRUM.drum_state[GREEN];
       MIDI_DRUM.prev_state[YELLOW_CYMBAL] = MIDI_DRUM.drum_state[YELLOW_CYMBAL];
	   MIDI_DRUM.prev_state[BLUE_CYMBAL] = MIDI_DRUM.drum_state[BLUE_CYMBAL];
       MIDI_DRUM.prev_state[GREEN_CYMBAL] = MIDI_DRUM.drum_state[GREEN_CYMBAL];
       MIDI_DRUM.prev_state[ORANGE_BASS] = MIDI_DRUM.drum_state[ORANGE_BASS];
       MIDI_DRUM.prev_state[BLACK_BASS] = MIDI_DRUM.drum_state[BLACK_BASS];
                             
        if ( MIDI_DRUM.verbose && ( MIDI_DRUM.drum_state[RED] ||  
	                            MIDI_DRUM.drum_state[YELLOW] ||
				    MIDI_DRUM.drum_state[BLUE] ||
				    MIDI_DRUM.drum_state[GREEN] ||
				    MIDI_DRUM.drum_state[ORANGE_BASS] ||
				    MIDI_DRUM.drum_state[BLACK_BASS]  )) {
            printf("%s %s %s %s %s %s \n",  MIDI_DRUM.drum_state[RED]>0?"VV":"  ",
	                                    MIDI_DRUM.drum_state[YELLOW]>0?"VV":"  ",
					    MIDI_DRUM.drum_state[BLUE]>0?"VV":"  ", 
					    MIDI_DRUM.drum_state[GREEN]>0?"VV":"  ",
					    MIDI_DRUM.drum_state[ORANGE_BASS]>0?"VV":"  ", 
					    MIDI_DRUM.drum_state[BLACK_BASS]>0?"VV":"  ");
            printf("%02i %02i %02i %02i %02i %02i\n", MIDI_DRUM.drum_state[RED],
	                                              MIDI_DRUM.drum_state[YELLOW], 
						      MIDI_DRUM.drum_state[BLUE], 
						      MIDI_DRUM.drum_state[GREEN], 
						      MIDI_DRUM.drum_state[ORANGE_BASS], 
						      MIDI_DRUM.drum_state[BLACK_BASS]);
        }

    if ( MIDI_DRUM.verbose && memcmp(MIDI_DRUM.oldbuf,MIDI_DRUM.buf,INTR_LENGTH))
    {
        printf("%02x%02x%02x%02x %02x%02x%02x%02x %02x%02x%02x%02x %02x%02x%02x%02x %02x%02x%02x%02x %02x%02x%02x%02x %02x%02x%02x kit type=%d\n",
               MIDI_DRUM.buf[0], MIDI_DRUM.buf[1], MIDI_DRUM.buf[2], MIDI_DRUM.buf[3], MIDI_DRUM.buf[4],
	       MIDI_DRUM.buf[5], MIDI_DRUM.buf[6], MIDI_DRUM.buf[7], MIDI_DRUM.buf[8], MIDI_DRUM.buf[9],
               MIDI_DRUM.buf[10], MIDI_DRUM.buf[11], MIDI_DRUM.buf[12], MIDI_DRUM.buf[13], MIDI_DRUM.buf[14], 
	       MIDI_DRUM.buf[15], MIDI_DRUM.buf[16], MIDI_DRUM.buf[17], MIDI_DRUM.buf[18], MIDI_DRUM.buf[19],
               MIDI_DRUM.buf[20], MIDI_DRUM.buf[21], MIDI_DRUM.buf[22], MIDI_DRUM.buf[23], MIDI_DRUM.buf[24], 
	       MIDI_DRUM.buf[25], MIDI_DRUM.buf[26],MIDI_DRUM.kit);
	memcpy(MIDI_DRUM.oldbuf,MIDI_DRUM.buf,INTR_LENGTH);
    }
    if (libusb_submit_transfer(MIDI_DRUM.irq_transfer) < 0)
        MIDI_DRUM.do_exit = 2;
}

//debug mode callback
static void cb_irq_dbg(struct libusb_transfer *transfer)
{
    if (transfer->status != LIBUSB_TRANSFER_COMPLETED) {
        fprintf(stderr, "irq transfer status %d? %d\n", transfer->status, LIBUSB_TRANSFER_ERROR);
        MIDI_DRUM.do_exit = 2;
        libusb_free_transfer(transfer);
        MIDI_DRUM.irq_transfer = NULL;
        return;
    }

    if ( memcmp(MIDI_DRUM.oldbuf,MIDI_DRUM.buf,INTR_LENGTH))
    {
        printf("%02x%02x%02x%02x %02x%02x%02x%02x %02x%02x%02x%02x %02x%02x%02x%02x %02x%02x%02x%02x %02x%02x%02x%02x %02x%02x%02x kit type=%d\n",
               MIDI_DRUM.buf[0], MIDI_DRUM.buf[1], MIDI_DRUM.buf[2], MIDI_DRUM.buf[3], MIDI_DRUM.buf[4],
	       MIDI_DRUM.buf[5], MIDI_DRUM.buf[6], MIDI_DRUM.buf[7], MIDI_DRUM.buf[8], MIDI_DRUM.buf[9],
               MIDI_DRUM.buf[10], MIDI_DRUM.buf[11], MIDI_DRUM.buf[12], MIDI_DRUM.buf[13], MIDI_DRUM.buf[14], 
	       MIDI_DRUM.buf[15], MIDI_DRUM.buf[16], MIDI_DRUM.buf[17], MIDI_DRUM.buf[18], MIDI_DRUM.buf[19],
               MIDI_DRUM.buf[20], MIDI_DRUM.buf[21], MIDI_DRUM.buf[22], MIDI_DRUM.buf[23], MIDI_DRUM.buf[24], 
	       MIDI_DRUM.buf[25], MIDI_DRUM.buf[26],MIDI_DRUM.kit);
	memcpy(MIDI_DRUM.oldbuf,MIDI_DRUM.buf,INTR_LENGTH);
    }
    if (libusb_submit_transfer(MIDI_DRUM.irq_transfer) < 0)
        MIDI_DRUM.do_exit = 2;
}
static int init_capture(void)
{
    int r;

    r = libusb_submit_transfer(MIDI_DRUM.irq_transfer);
    if (r < 0)
        return r;

    return 6;
}

static int alloc_transfers(int type)
{
    MIDI_DRUM.irq_transfer = libusb_alloc_transfer(0);
    if (!MIDI_DRUM.irq_transfer)
        return -ENOMEM;

    if(MIDI_DRUM.dbg){
        libusb_fill_interrupt_transfer(MIDI_DRUM.irq_transfer, MIDI_DRUM.devh, EP_INTR, MIDI_DRUM.irqbuf,
            sizeof(MIDI_DRUM.irqbuf), cb_irq_dbg, NULL, 0);
        if( MIDI_DRUM.verbose)printf("Debug Mode Enabled..\n");
    }
    else if(type == PS_ROCKBAND  || type == XB_ROCKBAND ||
            type == WII_ROCKBAND || type == PS_ROCKBAND1 ||
            type == XB_ROCKBAND1){
        libusb_fill_interrupt_transfer(MIDI_DRUM.irq_transfer, MIDI_DRUM.devh, EP_INTR, MIDI_DRUM.irqbuf,
            sizeof(MIDI_DRUM.irqbuf), cb_irq_rb, NULL, 0);
        if( MIDI_DRUM.verbose)printf("Rock Band drum kit detected.\n");
    }
    else if(type == GUITAR_HERO){
        libusb_fill_interrupt_transfer(MIDI_DRUM.irq_transfer, MIDI_DRUM.devh, EP_INTR, MIDI_DRUM.irqbuf,
            sizeof(MIDI_DRUM.irqbuf), cb_irq_gh, NULL, 0);
        if( MIDI_DRUM.verbose)printf("Guitar Hero World Tour drum kit detected.\n");
    }
    else{
        printf("error in drum type! %i\n",type);
    }

    return 0;
}

/*
These functions are from the official alsa docs:
http://www.alsa-project.org/alsa-doc/alsa-lib/seq.html
*/

// create a new client
snd_seq_t *open_client()
{
    snd_seq_t *handle;
    int err;
    err = snd_seq_open(&handle, "default", SND_SEQ_OPEN_OUTPUT, 0);
    if (err < 0)
        return NULL;
    snd_seq_set_client_name(handle, "PS3 Joystick Client");
    return handle;
}

// create a new port; return the port id
// port will be writable and accept the write-subscription.
int my_new_port(snd_seq_t *handle)
{
    // |SND_SEQ_PORT_CAP_WRITE||SND_SEQ_PORT_CAP_SUBS_WRITE
    return snd_seq_create_simple_port(handle, "PS3 Joystick port 2",
        SND_SEQ_PORT_CAP_READ|SND_SEQ_PORT_CAP_SUBS_READ,
        SND_SEQ_PORT_TYPE_MIDI_GENERIC | SND_SEQ_PORT_TYPE_APPLICATION);
}

// create a queue and return its id
int my_queue(snd_seq_t *handle)
{
    return snd_seq_alloc_named_queue(handle, "PS3 Joystick queue");
}

// set the tempo and pulse per quarter note
void set_tempo(snd_seq_t *handle, int q)
{
    snd_seq_queue_tempo_t *tempo;
    snd_seq_queue_tempo_alloca(&tempo);
    perror("snd_seq_queue_tempo_alloca");
    snd_seq_queue_tempo_set_tempo(tempo, 1000000); // 60 BPM
    perror("snd_seq_queue_tempo_set_tempo");
    snd_seq_queue_tempo_set_ppq(tempo, 48); // 48 PPQ
    perror("snd_seq_queue_tempo_set_ppq");
    snd_seq_set_queue_tempo(handle, q, tempo);
    perror("snd_seq_set_queue_tempo");
}

// change the tempo on the fly
int change_tempo(snd_seq_t *handle, int my_client_id, int my_port_id, int q, unsigned int tempo)
{
    snd_seq_event_t ev;
    snd_seq_ev_clear(&ev);
    ev.dest.client = SND_SEQ_CLIENT_SYSTEM;
    ev.dest.port = SND_SEQ_PORT_SYSTEM_TIMER;
    ev.source.client = my_client_id;
    ev.source.port = my_port_id;
    ev.queue = SND_SEQ_QUEUE_DIRECT; // no scheduling
    ev.data.queue.queue = q;        // affected queue id
    ev.data.queue.param.value = tempo;    // new tempo in microsec.
    return snd_seq_event_output(handle, &ev);
}

static void program_change(snd_seq_t *seq, int port, int chan, int program)
{
    snd_seq_event_t ev;

    snd_seq_ev_clear(&ev);
    snd_seq_ev_set_source(&ev, port);
    snd_seq_ev_set_subs(&ev);
    snd_seq_ev_set_direct(&ev);
    //... // set event type, data, so on..
    //set_event_time(&ev, Mf_currtime);
    //snd_seq_ev_schedule_tick(&ev, q, 0, Mf_currtime);

    snd_seq_ev_set_pgmchange(&ev, chan, program);

    int rc = snd_seq_event_output(seq, &ev);
    if (rc < 0) {
        printf("written = %i (%s)\n", rc, snd_strerror(rc));
    }
    snd_seq_drain_output(seq);
}

// A lot easier:
void subscribe_output(snd_seq_t *seq, int client, int port)
{
    snd_seq_connect_to(seq, DEFAULT_CHANNEL, client, port);
}

// From test/playmidi1.c from alsa-lib-1.0.3.

// Direct delivery seems like what I'm doing..
void notedown(snd_seq_t *seq, int port, int chan, int pitch, int vol)
{
    snd_seq_event_t ev;

    snd_seq_ev_clear(&ev);
    snd_seq_ev_set_source(&ev, port);
    snd_seq_ev_set_subs(&ev);
    snd_seq_ev_set_direct(&ev);
    //... // set event type, data, so on..
    //set_event_time(&ev, Mf_currtime);
    //snd_seq_ev_schedule_tick(&ev, q, 0, Mf_currtime);

    snd_seq_ev_set_noteon(&ev, chan, pitch, vol);

    int rc = snd_seq_event_output(seq, &ev);
    if (rc < 0) {
        printf("written = %i (%s)\n", rc, snd_strerror(rc));
    }
    snd_seq_drain_output(seq);
}

// When the note up, note off.
void noteup(snd_seq_t *seq, int port, int chan, int pitch, int vol)
{
    snd_seq_event_t ev;

    snd_seq_ev_clear(&ev);
    snd_seq_ev_set_source(&ev, port);
    snd_seq_ev_set_subs(&ev);
    snd_seq_ev_set_direct(&ev);
    //... // set event type, data, so on..

    snd_seq_ev_set_noteoff(&ev, chan, pitch, vol);

    snd_seq_event_output(seq, &ev);
    snd_seq_drain_output(seq);
}

int setup_alsa(snd_seq_t **seq, int *port)
{
    if ( MIDI_DRUM.verbose) printf("Setting up alsa\n");

    *seq = open_client();
    if (*seq == NULL) {
        if ( MIDI_DRUM.verbose >= 0) printf("Error: open_client failed: %s\n", snd_strerror(seq));
        return 0;
    }

    int my_client_id = snd_seq_client_id(*seq);
    *port = my_new_port(*seq);
    if ( MIDI_DRUM.verbose) printf("client:port = %i:%i\n", my_client_id, *port);

    program_change(*seq, *port, DEFAULT_CHANNEL, 0);
    int ret = 1;
    notedown(*seq, *port, DEFAULT_CHANNEL, 57, 55);

    if ( MIDI_DRUM.verbose) printf("Returning %i\n", ret);
    return ret;
}

void testAlsa(snd_seq_t *seq, int port)
{
    notedown(seq, port, DEFAULT_CHANNEL, 57, 127);
    usleep(1000000);
    noteup(seq, port, DEFAULT_CHANNEL, 57, 0);
    usleep(1000000);

}

static void sighandler(int signum)
{
    MIDI_DRUM.do_exit = 1;
}

void useage()
{
    printf("rbdrum2midi a rockband/guitar hero drumset driver in userland\n");
    printf("\n");
    printf("\n");
    printf("USEAGE:\n");
    printf("    rbdrum2midi [-option <value>...]\n");
    printf("\n");
    printf("OPTIONS:\n");
    printf("    -v                          verbose mode\n");
    printf("    -r/y/b/g <value>            set midi value for -color of drum head\n");
    printf("    -ocy/ycy/bcy/gcy <value>    set midi values for -color of cymbal\n");
    printf("    -ob/bkb <value>             set midi values for -color bass pedal\n");
    printf("    -rb1                        specify rockband 1 drumset\n");
    printf("    -dbg                        debug mode\n");
    printf("    -h                          show this message\n");
    printf("\n");
    printf("EXAMPLES:\n");
    printf("    rbdrum2midi -r 16 -bcy 64 -ob 32 -g 17 -rb1\n");
    printf("    rbdrum2midi -v\n");
    printf("\n");
    printf("NOTES:\n");
    printf("    r=red, o=orange, y=yellow, g=green, b=blue, bk=black\n");
    printf("    cy=cymbal, b=bass pedal, otherwise drum pad\n");
    printf("    the default midi values are for the hydrogen yamaha vintage kit\n");
    printf("\n");

    return;
}

int main(int argc, char **argv)
{
    struct sigaction sigact;
    int r = 1;
    int i = 0;
    MIDI_DRUM.buf = MIDI_DRUM.irqbuf;
    MIDI_DRUM.bass_down = 0;
    MIDI_DRUM.verbose = 0;
    MIDI_DRUM.dbg = 0;
    MIDI_DRUM.kit = PS_ROCKBAND;
    memset(MIDI_DRUM.oldbuf,0,INTR_LENGTH);
    memset(MIDI_DRUM.drum_state,0,10);
    memset(MIDI_DRUM.prev_state,0,10);
    
    //default midi values;
    MIDI_DRUM.midi_note[RED] = YVK_SNARE; 
    MIDI_DRUM.midi_note[YELLOW] = YVK_HI_TOM;
    MIDI_DRUM.midi_note[BLUE] = YVK_MID_TOM;
    MIDI_DRUM.midi_note[GREEN] = YVK_LO_TOM;
    MIDI_DRUM.midi_note[YELLOW_CYMBAL] = YVK_CLOSED_HAT;
    MIDI_DRUM.midi_note[GREEN_CYMBAL] = YVK_CRASH;
    MIDI_DRUM.midi_note[BLUE_CYMBAL] = YVK_RIDE;
    MIDI_DRUM.midi_note[ORANGE_CYMBAL] = YVK_CRASH;
    MIDI_DRUM.midi_note[ORANGE_BASS] = YVK_KICK;
    MIDI_DRUM.midi_note[BLACK_BASS] = YVK_OPEN_HAT;

    if (argc > 1) {
        for (i = 1;i<argc;i++)
        {
            if (strcmp(argv[i], "-v") == 0) {
                 MIDI_DRUM.verbose = 1;
            }
            else if (strcmp(argv[i], "-rb1") == 0) {
                //rockband 1 set, use different irq routine
                MIDI_DRUM.kit = PS_ROCKBAND1;
            }
            else if (strcmp(argv[i], "-ocy") == 0) {
                //orange cymbal
                MIDI_DRUM.midi_note[ORANGE_CYMBAL] = atoi(argv[++i]);
            }
            else if (strcmp(argv[i], "-ycy") == 0) {
                //yellow cymbal
                MIDI_DRUM.midi_note[YELLOW_CYMBAL] = atoi(argv[++i]);
            }
            else if (strcmp(argv[i], "-gcy") == 0) {
                //green cymbal
                MIDI_DRUM.midi_note[GREEN_CYMBAL] = atoi(argv[++i]);
            }
            else if (strcmp(argv[i], "-bcy") == 0) {
                //blue cymbal
                MIDI_DRUM.midi_note[BLUE_CYMBAL] = atoi(argv[++i]);
            }
            else if (strcmp(argv[i], "-ob") == 0) {
                //orange bass
                MIDI_DRUM.midi_note[ORANGE_BASS] = atoi(argv[++i]);
            }
            else if (strcmp(argv[i], "-bkb") == 0) {
                //black bass
                MIDI_DRUM.midi_note[BLACK_BASS] = atoi(argv[++i]);
            }
            else if (strcmp(argv[i], "-r") == 0) {
                //red pad
                MIDI_DRUM.midi_note[RED] = atoi(argv[++i]);
            }
            else if (strcmp(argv[i], "-y") == 0) {
                //yellow pad
                MIDI_DRUM.midi_note[YELLOW] = atoi(argv[++i]);
            }
            else if (strcmp(argv[i], "-g") == 0) {
                //green pad
                MIDI_DRUM.midi_note[GREEN] = atoi(argv[++i]);
            }
            else if (strcmp(argv[i], "-b") == 0) {
                //blue pad
                 MIDI_DRUM.midi_note[BLUE] = atoi(argv[++i]);
            }
            else if (strcmp(argv[i], "-dbg") == 0) {
                //debug mode
                MIDI_DRUM.dbg = 1;
		MIDI_DRUM.verbose = 1;
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
    if(MIDI_DRUM.kit==PS_ROCKBAND1){
        //no way of knowing if device is RB1 so reassign kit after claiming
        r = find_rbdrum_device(i);
        switch(MIDI_DRUM.kit)
	{
	    case PS_ROCKBAND:
	    case WII_ROCKBAND:
  	        MIDI_DRUM.kit = PS_ROCKBAND1; 
	        break;
	    case XB_ROCKBAND:
	        MIDI_DRUM.kit = XB_ROCKBAND1; 
	        break;
	}
    }
    else
        r = find_rbdrum_device(i);
    if (r < 0) {
        fprintf(stderr, "Could not find/open device\n");
        libusb_close(MIDI_DRUM.devh);
        libusb_exit(NULL);
        return -r;
    }
    init_midi_drum();

    if (libusb_kernel_driver_active(MIDI_DRUM.devh, 0)) {
        r = libusb_detach_kernel_driver(MIDI_DRUM.devh, 0);
        if (r < 0) {
            printf("did not detach.\n");
        }
    }
    r = libusb_claim_interface(MIDI_DRUM.devh, 0);
    if (r < 0) {
        fprintf(stderr, "usb_claim_interface error %d %d\n", r, LIBUSB_ERROR_BUSY);
        libusb_close(MIDI_DRUM.devh);
        libusb_exit(NULL);
        return -r;
    }
    printf("claimed interface\n");

    int ret = setup_alsa(& MIDI_DRUM.g_seq, & MIDI_DRUM.g_port);
    // 0 is fail.
    if (ret == 0) {
        printf("Error: Alsa setup failed.\n");
        return 1;
    }

    if (r < 0) {
        // Deinit & Release
        libusb_free_transfer(MIDI_DRUM.irq_transfer);
        libusb_release_interface(MIDI_DRUM.devh, 0);
        libusb_close(MIDI_DRUM.devh);
        libusb_exit(NULL);
        snd_seq_close( MIDI_DRUM.g_seq);
        printf("do_init failed.\n");
        return -r;
    }

    /* async from here onwards */

    r = alloc_transfers(MIDI_DRUM.kit);
    if (r < 0) {
        // Deinit & Release
        libusb_free_transfer(MIDI_DRUM.irq_transfer);
        libusb_release_interface(MIDI_DRUM.devh, 0);
        libusb_close(MIDI_DRUM.devh);
        libusb_exit(NULL);
        snd_seq_close( MIDI_DRUM.g_seq);
        printf("alloc_transfers failed.\n");
        return -r;
    }

    r = init_capture();
    if (r < 0) {
        // Deinit & Release
        libusb_free_transfer(MIDI_DRUM.irq_transfer);
        libusb_release_interface(MIDI_DRUM.devh, 0);
        libusb_close(MIDI_DRUM.devh);
        libusb_exit(NULL);
        snd_seq_close( MIDI_DRUM.g_seq);
        printf("init_capture failed.\n");
        return -r;
    }

    sigact.sa_handler = sighandler;
    sigemptyset(&sigact.sa_mask);
    sigact.sa_flags = SA_RESTART;
    r = sigaction(SIGINT, &sigact, NULL);
    r = sigaction(SIGTERM, &sigact, NULL);
    r = sigaction(SIGQUIT, &sigact, NULL);

    while (!MIDI_DRUM.do_exit) {
        r = libusb_handle_events(NULL);
        if (r < 0) {
            break;
        }
    }

    printf("shutting down...\n");

    if (MIDI_DRUM.irq_transfer) {
        r = libusb_cancel_transfer(MIDI_DRUM.irq_transfer);
        if (r < 0) {
            // Deinit & Release
            libusb_free_transfer(MIDI_DRUM.irq_transfer);
            libusb_release_interface(MIDI_DRUM.devh, 0);
            libusb_close(MIDI_DRUM.devh);
            libusb_exit(NULL);
            snd_seq_close( MIDI_DRUM.g_seq);
            printf("libusb_cancel_transfer failed.\n");
            return -r;
        }
    }


    // || img_transfer
    while (MIDI_DRUM.irq_transfer)
        if (libusb_handle_events(NULL) < 0)
            break;

    if (MIDI_DRUM.do_exit == 1)
        r = 0;
    else
        r = 1;

//out_deinit: 
    libusb_free_transfer(MIDI_DRUM.irq_transfer); 
//out_release:
    libusb_release_interface(MIDI_DRUM.devh, 0);
//out:
    libusb_close(MIDI_DRUM.devh);
    libusb_exit(NULL);
    snd_seq_close( MIDI_DRUM.g_seq);
    return 23;
    return r >= 0 ? r : -r;
}

