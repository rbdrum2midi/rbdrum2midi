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
//#define EP_DATA			(2 | LIBUSB_ENDPOINT_IN)
//#define CTRL_IN			(LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_ENDPOINT_IN)
//#define CTRL_OUT		(LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_ENDPOINT_OUT)
//#define USB_RQ			0x04
#define INTR_LENGTH		27

#define DEFAULT_CHANNEL 9

#define max(a, b) (((a) > (b)) ? (a) : (b))
#define min(a, b) (((a) < (b)) ? (a) : (b))

#define RB_YELLOW   buf[11]
#define RB_RED      buf[12]
#define RB_GREEN    buf[13]
#define RB_BLUE     buf[14]
#define RB_ORANGE   buf[0] & 0x10
#define RB_BLACK    buf[0] & 0x20
#define RB_CYMBAL   buf[1] & 0x08

#define RB1_YELLOW  buf[3] & 0x80
#define RB1_RED     buf[3] & 0x20
#define RB1_GREEN   buf[3] & 0x10
#define RB1_BLUE    buf[3] & 0x40
#define RB1_ORANGE  buf[3] & 0x01

#define GH_YELLOW   buf[11]
#define GH_RED      buf[12]
#define GH_GREEN    buf[13]
#define GH_BLUE     buf[14]
#define GH_ORANGE   buf[16]
#define GH_PEDAL    buf[15]

#define ROCKBAND    1
#define ROCKBAND1   2
#define GUITARHERO  3

#define YVK_KICK        36
#define YVK_SNARE       37
#define YVK_LO_TOM      38
#define YVK_MID_TOM     39
#define YVK_HI_TOM      40
#define YVK_CLOSED_HAT  41
#define YVK_OPEN_HAT    42
#define YVK_RIDE        43
#define YVK_CRASH       45
int drum_conn[10] = {36, 42, 37, 38, 41, 39, 43, 40, 45, 36};
struct drum_midi
{
    unsigned char red;
    unsigned char yellow;
    unsigned char blue;
    unsigned char green;
    unsigned char yellow_cymbal;
    unsigned char blue_cymbal;
    unsigned char green_cymbal;
    unsigned char orange_cymbal;
    unsigned char orange_bass;
    unsigned char black_bass;
}DRUM_MIDI;

void notedown(snd_seq_t *seq, int port, int chan, int pitch, int vol);
void noteup(snd_seq_t *seq, int port, int chan, int pitch, int vol);

static snd_seq_t *g_seq;
static int g_port;
static int kit;

static int verbose = 0;
static int g_i = 0;
static int state = 0;
static int drum_state[8];
static int do_exit = 0;
unsigned char *buf;
unsigned char yel;
unsigned char red;
unsigned char grn;
unsigned char blu;
unsigned char orange;
unsigned char bass;
unsigned char bass2;
unsigned char bass_down;
int velocity;
static unsigned char irqbuf[INTR_LENGTH];
static struct libusb_device_handle *devh = NULL;
//static struct libusb_transfer *img_transfer = NULL;
static struct libusb_transfer *irq_transfer = NULL;

static int find_rbdrum_device(int i)
{
	// TODO: Currently the i argument is ignored.
	//PS3 RB kit
	devh = libusb_open_device_with_vid_pid(NULL, 0x12ba, 0x0210);
	//xbox RB kit
	if(!devh){
		devh = libusb_open_device_with_vid_pid(NULL, 0x1bad, 0x0003);
		}
	//Wìì RB kit??
	if(!devh){
		devh = libusb_open_device_with_vid_pid(NULL, 0x1bad, 0x0005);
		}
	
	if(devh){
        kit=ROCKBAND;
		}
	//PS3 GH kit
	if(!devh){
		devh = libusb_open_device_with_vid_pid(NULL, 0x12ba, 0x0120);

        if(devh){
            kit=GUITARHERO;
            }
        }
		
	return devh ? 0 : -EIO;
}

// IS THIS THE HARDWARE PROBING CODE?
/*static int print_f0_data(void)
{
	unsigned char data[0x10];
	int r;
	unsigned int i;

	r = libusb_control_transfer(devh, CTRL_IN, USB_RQ, 0xf0, 0, data,
		sizeof(data), 0);
	if (r < 0) {
		fprintf(stderr, "F0 error %d\n", r);
		return r;
	}
	if ((unsigned int) r < sizeof(data)) {
		fprintf(stderr, "short read (%d)\n", r);
		return -1;
	}

	printf("F0 data:");
	for (i = 0; i < sizeof(data); i++)
		printf("%02x ", data[i]);
	printf("\n");
	return 0;
}

static int set_hwstat(unsigned char data)
{
	int r;

	printf("set hwstat to %02x\n", data);
	r = libusb_control_transfer(devh, CTRL_OUT, USB_RQ, 0x07, 0, &data, 1, 0);
	if (r < 0) {
		fprintf(stderr, "set hwstat error %d\n", r);
		return r;
	}
	if ((unsigned int) r < 1) {
		fprintf(stderr, "short write (%d)", r);
		return -1;
	}

	return 0;
}

static int get_hwstat(unsigned char *status)
{
	int r;

	r = libusb_control_transfer(devh, CTRL_IN, USB_RQ, 0x07, 0, status, 1, 0);
	if (r < 0) {
		fprintf(stderr, "read hwstat error %d\n", r);
		return r;
	}
	if ((unsigned int) r < 1) {
		fprintf(stderr, "short read (%d)\n", r);
		return -1;
	}

	printf("hwstat reads %02x\n", *status);
	return 0;
}

static int set_mode(unsigned char data)
{
	int r;
	printf("set mode %02x\n", data);

	r = libusb_control_transfer(devh, CTRL_OUT, USB_RQ, 0x4e, 0, &data, 1, 0);
	if (r < 0) {
		fprintf(stderr, "set mode error %d\n", r);
		return r;
	}
	if ((unsigned int) r < 1) {
		fprintf(stderr, "short write (%d)", r);
		return -1;
	}

	return 0;
}

static void cb_mode_changed(struct libusb_transfer *transfer)
{
	if (transfer->status != LIBUSB_TRANSFER_COMPLETED) {
		fprintf(stderr, "mode change transfer not completed!\n");
		do_exit = 2;
	}

	printf("async cb_mode_changed length=%d actual_length=%d\n",
		transfer->length, transfer->actual_length);
	if (next_state() < 0)
		do_exit = 2;
}

static int set_mode_async(unsigned char data)
{
	unsigned char *buf = malloc(LIBUSB_CONTROL_SETUP_SIZE + 1);
	struct libusb_transfer *transfer;

	if (!buf)
		return -ENOMEM;
	
	transfer = libusb_alloc_transfer(0);
	if (!transfer) {
		free(buf);
		return -ENOMEM;
	}

	printf("async set mode %02x\n", data);
	libusb_fill_control_setup(buf, CTRL_OUT, USB_RQ, 0x4e, 0, 1);
	buf[LIBUSB_CONTROL_SETUP_SIZE] = data;
	libusb_fill_control_transfer(transfer, devh, buf, cb_mode_changed, NULL,
		1000);

	transfer->flags = LIBUSB_TRANSFER_SHORT_NOT_OK
		| LIBUSB_TRANSFER_FREE_BUFFER | LIBUSB_TRANSFER_FREE_TRANSFER;
	return libusb_submit_transfer(transfer);
}*/

static int do_sync_intr(unsigned char *data)
{
	int r;
	int transferred;

	r = libusb_interrupt_transfer(devh, EP_INTR, data, INTR_LENGTH,
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

/*static int next_state(void)
{
	int r = 0;
	printf("old state: %d\n", state);
	switch (state) {
	case STATE_AWAIT_IRQ_FINGER_REMOVED:
		state = STATE_AWAIT_MODE_CHANGE_AWAIT_FINGER_ON;
		r = set_mode_async(MODE_AWAIT_FINGER_ON);
		break;
	case STATE_AWAIT_MODE_CHANGE_AWAIT_FINGER_ON:
		state = STATE_AWAIT_IRQ_FINGER_DETECTED;
		break;
	case STATE_AWAIT_IRQ_FINGER_DETECTED:
		state = STATE_AWAIT_MODE_CHANGE_CAPTURE;
		r = set_mode_async(MODE_CAPTURE);
		break;
	case STATE_AWAIT_MODE_CHANGE_CAPTURE:
		state = STATE_AWAIT_IMAGE;
		break;
	case STATE_AWAIT_IMAGE:
		state = STATE_AWAIT_MODE_CHANGE_AWAIT_FINGER_OFF;
		r = set_mode_async(MODE_AWAIT_FINGER_OFF);
		break;
	case STATE_AWAIT_MODE_CHANGE_AWAIT_FINGER_OFF:
		state = STATE_AWAIT_IRQ_FINGER_REMOVED;
		break;
	default:
		printf("unrecognised state %d\n", state);
	}
	if (r < 0) {
		fprintf(stderr, "error detected changing state\n");
		return r;
	}

	printf("new state: %d\n", state);
	return 0;
}*/

static void cb_irq_rb(struct libusb_transfer *transfer)
{
    if (transfer->status != LIBUSB_TRANSFER_COMPLETED) {
        fprintf(stderr, "irq transfer status %d? %d\n", transfer->status, LIBUSB_TRANSFER_ERROR);
        do_exit = 2;
        libusb_free_transfer(transfer);
        irq_transfer = NULL;
        return;
    }

    //RockBand 3 Drumkit

        yel     = RB_YELLOW; //Yellow
        red     = RB_RED; //Red
        grn     = RB_GREEN; //Green
        blu     = RB_BLUE; //Blue
        bass    = RB_ORANGE; //Orange Pedal
        bass2   = RB_BLACK; //Black Pedal
        bass_down = 0;

		// Events:
		// Down
		if (red && !drum_state[0]) {
			velocity = red * 2;
			velocity = min(max(velocity, 0), 127);
			noteup(g_seq, g_port, DEFAULT_CHANNEL, DRUM_MIDI.red, 0);
			notedown(g_seq, g_port, DEFAULT_CHANNEL, DRUM_MIDI.red, velocity);
			}
        //yellow pad and cymbal
		// Events:
		// Down
		if (yel && !drum_state[1]) {
			velocity = yel * 2;
			velocity = min(max(velocity, 0), 127);
			if(RB_CYMBAL){
				noteup(g_seq, g_port, DEFAULT_CHANNEL, DRUM_MIDI.yellow_cymbal, 0);
				notedown(g_seq, g_port, DEFAULT_CHANNEL, DRUM_MIDI.yellow_cymbal, velocity);
				}
			else{
				noteup(g_seq, g_port, DEFAULT_CHANNEL, DRUM_MIDI.yellow, 0);
				notedown(g_seq, g_port, DEFAULT_CHANNEL, DRUM_MIDI.yellow, velocity);
				}
			}
        //Blue pad and cymbal
		// Events:
		// Down
		if (blu && !drum_state[2]) {
			velocity = blu * 2;
			velocity = min(max(velocity, 0), 127);
			if(RB_CYMBAL){
				noteup(g_seq, g_port, DEFAULT_CHANNEL, DRUM_MIDI.blue_cymbal, 0);
				notedown(g_seq, g_port, DEFAULT_CHANNEL, DRUM_MIDI.blue_cymbal, velocity);
				}
			else{
				noteup(g_seq, g_port, DEFAULT_CHANNEL, DRUM_MIDI.blue, 0);
				notedown(g_seq, g_port, DEFAULT_CHANNEL, DRUM_MIDI.blue, velocity);
				}
			}
        //Green pad and Cymbal
		// Events:
		// Down
		if (grn && !drum_state[3]) {
			velocity = grn * 2;
			velocity = min(max(velocity, 0), 127);
			if(RB_CYMBAL){
				noteup(g_seq, g_port, DEFAULT_CHANNEL, DRUM_MIDI.green_cymbal, 0);
				notedown(g_seq, g_port, DEFAULT_CHANNEL, DRUM_MIDI.green_cymbal, velocity);
				}
			else{
				noteup(g_seq, g_port, DEFAULT_CHANNEL, DRUM_MIDI.green, 0);
				notedown(g_seq, g_port, DEFAULT_CHANNEL, DRUM_MIDI.green, velocity);
				}
			}
        // Pedal 1 (orange)
        if (bass != drum_state[4]) {
            // Events:
            // Down
            if (bass) {
                bass_down = 1;
                velocity = 127;
                velocity = min(max(velocity, 0), 127);
                notedown(g_seq, g_port, DEFAULT_CHANNEL, DRUM_MIDI.orange_bass, velocity);
                }
            // Up
            else{
                noteup(g_seq, g_port, DEFAULT_CHANNEL, DRUM_MIDI.orange_bass, 0);
                }
            }
        // Pedal 2 (black)
        if (bass2 != drum_state[5]) {
            // Events:
            // Down
            if (bass2) {
                bass_down = 1;
                velocity = 127;
                velocity = min(max(velocity, 0), 127);
                notedown(g_seq, g_port, DEFAULT_CHANNEL, DRUM_MIDI.black_bass, velocity);
                }
            // Up
            else {
                noteup(g_seq, g_port, DEFAULT_CHANNEL, DRUM_MIDI.black_bass, 0);
                }
            }
		//now that the time-critical stuff is done, lets do the assignments
		drum_state[0] = red;
		drum_state[1] = yel;
		drum_state[2] = blu;
		drum_state[3] = grn;
		drum_state[4] = bass;
		drum_state[5] = bass2;
		if (verbose && (red || yel || blu || grn || bass_down)) {
			printf("%s %s %s %s %s %s \n", red>0?"VV":"  ", yel>0?"VV":"  ", blu>0?"VV":"  ", grn>0?"VV":"  ", bass>0?"VV":"  ", bass2>0?"VV":"  ");
			printf("%02i %02i %02i %02i %02i %02i\n", red, yel, blu, grn, bass, bass2);
			printf("%02x%02x%02x%02x %02x%02x%02x%02x %02x%02x%02x%02x %02x%02x%02x%02x %02x%02x%02x%02x %02x%02x%02x%02x %02x%02x%02x\n",
			buf[0], buf[1], buf[2], buf[3], buf[4], buf[5], buf[6], buf[7], buf[8], buf[9],
			buf[10], buf[11], buf[12], buf[13], buf[14], buf[15], buf[16], buf[17], buf[18], buf[19],
			buf[20], buf[21], buf[22], buf[23], buf[24], buf[25], buf[26]);
		}

    if (verbose && (g_i++ % 200 == 199)) {
        printf("%02x%02x%02x%02x %02x%02x%02x%02x %02x%02x%02x%02x %02x%02x%02x%02x %02x%02x%02x%02x %02x%02x%02x%02x %02x%02x%02x kit type=%d\n",
        buf[0], buf[1], buf[2], buf[3], buf[4], buf[5], buf[6], buf[7], buf[8], buf[9],
        buf[10], buf[11], buf[12], buf[13], buf[14], buf[15], buf[16], buf[17], buf[18], buf[19],
        buf[20], buf[21], buf[22], buf[23], buf[24], buf[25], buf[26],kit);
    }


    if (libusb_submit_transfer(irq_transfer) < 0)
        do_exit = 2;
}

//irq for rockband 1 drumset with no velocity info
static void cb_irq_rb1(struct libusb_transfer *transfer)
{
    if (transfer->status != LIBUSB_TRANSFER_COMPLETED) {
        fprintf(stderr, "irq transfer status %d? %d\n", transfer->status, LIBUSB_TRANSFER_ERROR);
        do_exit = 2;
        libusb_free_transfer(transfer);
        irq_transfer = NULL;
        return;
    }

    unsigned char *buf = irqbuf;

    yel     = RB1_YELLOW; //Yellow
    red     = RB1_RED; //Red
    grn     = RB1_GREEN; //Green
    blu     = RB1_BLUE; //Blue
    bass    = RB1_ORANGE; //Orange Pedal
    bass_down = 0;
    velocity = 125;

	// Events:
	// Down
	if (red && !drum_state[0]) {
		noteup(g_seq, g_port, DEFAULT_CHANNEL, DRUM_MIDI.red, 0);
		notedown(g_seq, g_port, DEFAULT_CHANNEL, DRUM_MIDI.red, velocity);
		}
    //yellow pad and cymbal
	// Events:
	// Down
	if (yel && !drum_state[1]) {
		noteup(g_seq, g_port, DEFAULT_CHANNEL, DRUM_MIDI.yellow, 0);
		notedown(g_seq, g_port, DEFAULT_CHANNEL, DRUM_MIDI.yellow, velocity);
		}
    //Blue pad and cymbal
	// Events:
	// Down
	if (blu && !drum_state[2]) {
		noteup(g_seq, g_port, DEFAULT_CHANNEL, DRUM_MIDI.blue, 0);
		notedown(g_seq, g_port, DEFAULT_CHANNEL, DRUM_MIDI.blue, velocity);
		}
    //Green pad and Cymbal
	// Events:
	// Down
	if (!drum_state[3]) {
		noteup(g_seq, g_port, DEFAULT_CHANNEL, DRUM_MIDI.green, 0);
		notedown(g_seq, g_port, DEFAULT_CHANNEL, DRUM_MIDI.green, velocity);
		}
    // Pedal 1 (orange)
    if (bass != drum_state[4]) {
        // Events:
        // Down
        if (bass) {
            bass_down = 1;
            notedown(g_seq, g_port, DEFAULT_CHANNEL, DRUM_MIDI.orange_bass, velocity);
            }
        // Up
        else {
            noteup(g_seq, g_port, DEFAULT_CHANNEL, DRUM_MIDI.orange_bass, 0);
            }
        }
	//now that the time-critical stuff is done, lets do the assignments
	drum_state[0] = red;
	drum_state[1] = yel;
	drum_state[2] = blu;
	drum_state[3] = grn;
	drum_state[4] = bass;
     if (verbose && (red || yel || blu || grn || bass_down)) {
        printf("%s %s %s %s %s %s \n", red>0?"VV":"  ", yel>0?"VV":"  ", blu>0?"VV":"  ", grn>0?"VV":"  ", bass>0?"VV":"  ", bass2>0?"VV":"  ");
        printf("%02i %02i %02i %02i %02i %02i\n", red, yel, blu, grn, bass, bass2);
        printf("%02x%02x%02x%02x %02x%02x%02x%02x %02x%02x%02x%02x %02x%02x%02x%02x %02x%02x%02x%02x %02x%02x%02x%02x %02x%02x%02x\n",
        buf[0], buf[1], buf[2], buf[3], buf[4], buf[5], buf[6], buf[7], buf[8], buf[9],
        buf[10], buf[11], buf[12], buf[13], buf[14], buf[15], buf[16], buf[17], buf[18], buf[19],
        buf[20], buf[21], buf[22], buf[23], buf[24], buf[25], buf[26]);
        }


    if (verbose && (g_i++ % 200 == 199)) {
        printf("%02x%02x%02x%02x %02x%02x%02x%02x %02x%02x%02x%02x %02x%02x%02x%02x %02x%02x%02x%02x %02x%02x%02x%02x %02x%02x%02x kit type=%d\n",
        buf[0], buf[1], buf[2], buf[3], buf[4], buf[5], buf[6], buf[7], buf[8], buf[9],
        buf[10], buf[11], buf[12], buf[13], buf[14], buf[15], buf[16], buf[17], buf[18], buf[19],
        buf[20], buf[21], buf[22], buf[23], buf[24], buf[25], buf[26],kit);
    }

    if (libusb_submit_transfer(irq_transfer) < 0)
        do_exit = 2;
}

static void cb_irq_gh(struct libusb_transfer *transfer)
{
    if (transfer->status != LIBUSB_TRANSFER_COMPLETED) {
        fprintf(stderr, "irq transfer status %d? %d\n", transfer->status, LIBUSB_TRANSFER_ERROR);
        do_exit = 2;
        libusb_free_transfer(transfer);
        irq_transfer = NULL;
        return;
    }
    yel     = GH_YELLOW; //Yellow cymbal
    red     = GH_RED; //Red
    grn     = GH_GREEN; //Green
    blu     = GH_BLUE; //Blue
    orange  = GH_ORANGE; //Orange cymbal
    bass    = GH_PEDAL; //Black Pedal
    bass_down = 0;
    //guitar hero world tour drumset
		// Events:
		// Down
		if (red && !drum_state[0]) {
			velocity = red * 2;
			velocity = min(max(velocity, 0), 127);
			noteup(g_seq, g_port, DEFAULT_CHANNEL, DRUM_MIDI.red, 0);
			notedown(g_seq, g_port, DEFAULT_CHANNEL, DRUM_MIDI.red, velocity);
			}
        //Yellow Cymabl
		// Events:
		// Down
		if (yel && !drum_state[1]) {
			velocity = yel * 2;
			velocity = min(max(velocity, 0), 127);
			noteup(g_seq, g_port, DEFAULT_CHANNEL, DRUM_MIDI.yellow_cymbal, 0);
			notedown(g_seq, g_port, DEFAULT_CHANNEL, DRUM_MIDI.yellow_cymbal, velocity);
			}
        // Blue pad
		// Events:
		// Down
		if (blu && !drum_state[2]) {
			velocity = blu * 2;
			velocity = min(max(velocity, 0), 127);
			noteup(g_seq, g_port, DEFAULT_CHANNEL, DRUM_MIDI.blue, 0);
			notedown(g_seq, g_port, DEFAULT_CHANNEL, DRUM_MIDI.blue, velocity);
			}
        // Orange Cymbal
		// Events:
		// Down
		if (orange && !drum_state[7]) {
			velocity = orange * 2;
			velocity = min(max(velocity, 0), 127);
			noteup(g_seq, g_port, DEFAULT_CHANNEL, DRUM_MIDI.orange_cymbal, 0);
			notedown(g_seq, g_port, DEFAULT_CHANNEL, DRUM_MIDI.orange_cymbal, velocity);
			}
        //Green pad
		// Events:
		// Down
		if (grn && !drum_state[3]) {
			velocity = grn * 2;
			velocity = min(max(velocity, 0), 127);
			noteup(g_seq, g_port, DEFAULT_CHANNEL, DRUM_MIDI.green, 0);
			notedown(g_seq, g_port, DEFAULT_CHANNEL, DRUM_MIDI.green, velocity);
			}
        //Pedal
		// Events:
		// Down
		if (bass && !drum_state[6]) {
			bass_down = 1;
			velocity = bass * 2;
			velocity = min(max(velocity, 0), 127);
			noteup(g_seq, g_port, DEFAULT_CHANNEL, DRUM_MIDI.black_bass, 0);
			notedown(g_seq, g_port, DEFAULT_CHANNEL, DRUM_MIDI.black_bass, velocity);
			}
		drum_state[0] = red;
		drum_state[1] = yel;
		drum_state[2] = blu;
		drum_state[3] = grn;
		drum_state[6] = bass;
		if (verbose && (red || yel || blu || grn || bass_down || orange)) {
			printf("%s %s %s %s %s %s\n", red>0?"VV":"  ", yel>0?"VV":"  ", blu>0?"VV":"  ", grn>0?"VV":"  ", bass>0?"VV":"  ", orange>0?"VV":"  ");
            printf("%02i %02i %02i %02i %02i %02i\n", red, yel, blu, grn, bass, orange);
            printf("%02x%02x%02x%02x %02x%02x%02x%02x %02x%02x%02x%02x %02x%02x%02x%02x %02x%02x%02x%02x %02x%02x%02x%02x %02x%02x%02x\n",
            buf[0], buf[1], buf[2], buf[3], buf[4], buf[5], buf[6], buf[7], buf[8], buf[9],
            buf[10], buf[11], buf[12], buf[13], buf[14], buf[15], buf[16], buf[17], buf[18], buf[19],
            buf[20], buf[21], buf[22], buf[23], buf[24], buf[25], buf[26]);
            }


    if (verbose && (g_i++ % 200 == 199)) {
        printf("%02x%02x%02x%02x %02x%02x%02x%02x %02x%02x%02x%02x %02x%02x%02x%02x %02x%02x%02x%02x %02x%02x%02x%02x %02x%02x%02x kit type=%d\n",
        buf[0], buf[1], buf[2], buf[3], buf[4], buf[5], buf[6], buf[7], buf[8], buf[9],
        buf[10], buf[11], buf[12], buf[13], buf[14], buf[15], buf[16], buf[17], buf[18], buf[19],
        buf[20], buf[21], buf[22], buf[23], buf[24], buf[25], buf[26],kit);
    }

    if (libusb_submit_transfer(irq_transfer) < 0)
        do_exit = 2;
}

static int alloc_transfers(int type)
{
	/*img_transfer = libusb_alloc_transfer(0);
	if (!img_transfer)
		return -ENOMEM;*/
	
	irq_transfer = libusb_alloc_transfer(0);
	if (!irq_transfer)
		return -ENOMEM;

	/*libusb_fill_bulk_transfer(img_transfer, devh, EP_DATA, imgbuf,
		sizeof(imgbuf), cb_img, NULL, 0);*/
    if(type == ROCKBAND){
        libusb_fill_interrupt_transfer(irq_transfer, devh, EP_INTR, irqbuf,
            sizeof(irqbuf), cb_irq_rb, NULL, 0);
        if(verbose)printf("Rock Band drum kit detected.\n");
    }
    else if(type == ROCKBAND1){
        libusb_fill_interrupt_transfer(irq_transfer, devh, EP_INTR, irqbuf,
            sizeof(irqbuf), cb_irq_rb1, NULL, 0);
        if(verbose)printf("Rock Band 1 drum kit detected.\n");
    }
    else if(type == GUITARHERO){
        libusb_fill_interrupt_transfer(irq_transfer, devh, EP_INTR, irqbuf,
            sizeof(irqbuf), cb_irq_gh, NULL, 0);
        if(verbose)printf("Guitar Hero World Tour drum kit detected.\n");
    }
    else{
        //libusb_fill_interrupt_transfer(irq_transfer, devh, EP_INTR, irqbuf,
        //    sizeof(irqbuf), cb_irq, NULL, 0);
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

/*// send an event to our subscriber.
// since we want realtime instead of tick clock, we replace the function.
void schedule_event(snd_seq_t *seq, int my_port_id, int q)
{
	snd_seq_event_t ev;

	snd_seq_ev_clear(&ev);
	snd_seq_ev_set_source(&ev, my_port_id);
	snd_seq_ev_set_subs(&ev);
	// this t makes no sense...
	snd_seq_real_time_t t;
	// Use realtime instead of tick clock.
	snd_seq_ev_schedule_real(&ev, q, 0, &t);
	//snd_seq_ev_schedule_tick(&ev, Q, 0, t);
	//... TODO:
	// set event type, data, so on..
	snd_seq_ev_clear(&ev);
	
	ev.queue = SND_SEQ_QUEUE_DIRECT; // no scheduling
	ev.data.queue.queue = q;        // affected queue id
	//ev.data.queue.param.value = ...

	snd_seq_event_output(seq, &ev);
	//... TODO: ?
	snd_seq_drain_output(seq);  // if necessary
}*/

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
	if (verbose) printf("Setting up alsa\n");
	
	*seq = open_client();
	if (*seq == NULL) {
		if (verbose >= 0) printf("Error: open_client failed: %s\n", snd_strerror(seq));
		return 0;
	}
	
	int my_client_id = snd_seq_client_id(*seq);
	*port = my_new_port(*seq);
	if (verbose) printf("client:port = %i:%i\n", my_client_id, *port);

	program_change(*seq, *port, DEFAULT_CHANNEL, 0);
	// Subscribing to something else seems unnecssary until we figure out more.
	//subscribe_output(seq, 128, 0);
	
	//int tempo = 120;
	//int q = my_queue(seq);
	int ret = 1;
	/*
	// tempo isn't being used yet, errors *shrug*.
	set_tempo(seq, q);
	int ret = change_tempo(seq, my_client_id, my_port_id, q, tempo);
	if (ret < 0) {
		if (verbose >= 0) printf("Error: change_tempo failed: %s\n", snd_strerror(ret));
		return ret;
	}
	*/
	// Scheduling is scheduling, direct delivery is what I'm doing.
	//schedule_event(seq, my_port_id, q);
	notedown(*seq, *port, DEFAULT_CHANNEL, 57, 55);
	
	if (verbose) printf("Returning %i\n", ret);
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
	do_exit = 1;
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
    buf = irqbuf;

    //default midi values;
    DRUM_MIDI.red = YVK_SNARE;
    DRUM_MIDI.yellow = YVK_HI_TOM;
    DRUM_MIDI.blue = YVK_MID_TOM;
    DRUM_MIDI.green = YVK_LO_TOM;
    DRUM_MIDI.yellow_cymbal = YVK_CLOSED_HAT;
    DRUM_MIDI.blue_cymbal = YVK_CRASH;
    DRUM_MIDI.green_cymbal = YVK_RIDE;
    DRUM_MIDI.orange_cymbal = YVK_CRASH;
    DRUM_MIDI.orange_bass = YVK_KICK;
    DRUM_MIDI.black_bass = YVK_KICK;

	if (argc > 1) {
        for (i = 1;i<argc;i++)
        {
            if (strcmp(argv[i], "-v") == 0) {
                verbose = 1;
            }
            else if (strcmp(argv[i], "-rb1") == 0) {
                //rockband 1 set, use different irq routine
                kit = ROCKBAND1;
            }
            else if (strcmp(argv[i], "-ocy") == 0) {
                //orange cymbal
                DRUM_MIDI.orange_cymbal = atoi(argv[++i]);
            }
            else if (strcmp(argv[i], "-ycy") == 0) {
                //yellow cymbal
                DRUM_MIDI.yellow_cymbal = atoi(argv[++i]);
            }
            else if (strcmp(argv[i], "-gcy") == 0) {
                //green cymbal
                DRUM_MIDI.green_cymbal = atoi(argv[++i]);
            }
            else if (strcmp(argv[i], "-bcy") == 0) {
                //blue cymbal
                DRUM_MIDI.blue_cymbal = atoi(argv[++i]);
            }
            else if (strcmp(argv[i], "-ob") == 0) {
                //orange bass
                DRUM_MIDI.orange_bass = atoi(argv[++i]);
            }
            else if (strcmp(argv[i], "-bkb") == 0) {
                //black bass
                DRUM_MIDI.black_bass = atoi(argv[++i]);
            }
            else if (strcmp(argv[i], "-r") == 0) {
                //red pad
                DRUM_MIDI.red = atoi(argv[++i]);
            }
            else if (strcmp(argv[i], "-y") == 0) {
                //yellow pad
                DRUM_MIDI.yellow = atoi(argv[++i]);
            }
            else if (strcmp(argv[i], "-g") == 0) {
                //green pad
                DRUM_MIDI.green = atoi(argv[++i]);
            }
            else if (strcmp(argv[i], "-b") == 0) {
                //blue pad
                DRUM_MIDI.blue = atoi(argv[++i]);
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
    if(kit==ROCKBAND1){
        //no way of knowing if device is RB1 so reassign kit after claiming
        r = find_rbdrum_device(i);
        kit = ROCKBAND1;
    }
    else
        r = find_rbdrum_device(i);
	if (r < 0) {
		fprintf(stderr, "Could not find/open device\n");
		libusb_close(devh);
		libusb_exit(NULL);
		return -r;
	}

	if (libusb_kernel_driver_active(devh, 0)) {
		/*char driver_name[100];
		driver_name[0] = 0;
		r = usb_get_driver_np(devh, 0, driver_name, 100);
		if (r < 0) {
			printf("did not get driver_name.\n");
			driver_name[0] = 0;
		}*/
		r = libusb_detach_kernel_driver(devh, 0);
		if (r < 0) {
			printf("did not detach.\n");
		}
    }
    r = libusb_claim_interface(devh, 0);
	if (r < 0) {
		fprintf(stderr, "usb_claim_interface error %d %d\n", r, LIBUSB_ERROR_BUSY);
		libusb_close(devh);
		libusb_exit(NULL);
		return -r;
	}
	printf("claimed interface\n");

    if(kit!=ROCKBAND && DRUM_MIDI.yellow==YVK_HI_TOM)
    {
        //reassign yellow tom if not RB kit or user specified
        DRUM_MIDI.yellow = YVK_CLOSED_HAT;

    }

	int ret = setup_alsa(&g_seq, &g_port);
	// 0 is fail.
	if (ret == 0) {
		printf("Error: Alsa setup failed.\n");
		return 1;
	}

	/*r = print_f0_data();
	if (r < 0) {
		// Release
		libusb_release_interface(devh, 0);
		libusb_close(devh);
		libusb_exit(NULL);
		return -r;
	}*/

	r = do_init();
	if (r < 0) {
		// Deinit & Release
		libusb_free_transfer(irq_transfer);
		libusb_release_interface(devh, 0);
		libusb_close(devh);
		libusb_exit(NULL);
		snd_seq_close(g_seq);
		printf("do_init failed.\n");
		return -r;
	}

	/* async from here onwards */

    r = alloc_transfers(kit);
	if (r < 0) {
		// Deinit & Release
		libusb_free_transfer(irq_transfer);
		libusb_release_interface(devh, 0);
		libusb_close(devh);
		libusb_exit(NULL);
		snd_seq_close(g_seq);
		printf("alloc_transfers failed.\n");
		return -r;
	}

	r = init_capture();
	if (r < 0) {
		// Deinit & Release
		libusb_free_transfer(irq_transfer);
		libusb_release_interface(devh, 0);
		libusb_close(devh);
		libusb_exit(NULL);
		snd_seq_close(g_seq);
		printf("init_capture failed.\n");
		return -r;
	}

	sigact.sa_handler = sighandler;
	sigemptyset(&sigact.sa_mask);
	sigact.sa_flags = SA_RESTART;
	r = sigaction(SIGINT, &sigact, NULL);
	r = sigaction(SIGTERM, &sigact, NULL);
	r = sigaction(SIGQUIT, &sigact, NULL);

	// Drum state is all up.
    memset(drum_state, 0, 8);
	while (!do_exit) {
		r = libusb_handle_events(NULL);
		if (r < 0) {
			/*printf("why?\n");
			// Deinit & Release
			libusb_free_transfer(irq_transfer);
			libusb_release_interface(devh, 0);
			libusb_close(devh);
			libusb_exit(NULL);
			return -r;*/
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
			snd_seq_close(g_seq);
			printf("libusb_cancel_transfer failed.\n");
			return -r;
		}
	}

	/*if (img_transfer) {
		r = libusb_cancel_transfer(img_transfer);
		if (r < 0) {
			// TODO: Deinit & Release
			libusb_release_interface(devh, 0);
			libusb_close(devh);
			libusb_exit(NULL);
			return -r;
		}
	}*/

	// || img_transfer
	while (irq_transfer)
		if (libusb_handle_events(NULL) < 0)
			break;
	
	if (do_exit == 1)
		r = 0;
	else
		r = 1;

//out_deinit:
	//libusb_free_transfer(img_transfer);
	libusb_free_transfer(irq_transfer);
	/*set_mode(0);
	set_hwstat(0x80);*/
//out_release:
	libusb_release_interface(devh, 0);
//out:
	libusb_close(devh);
	libusb_exit(NULL);
	snd_seq_close(g_seq);
	return 23;
	return r >= 0 ? r : -r;
}

