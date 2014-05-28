/*-
 * Copyright (c) 2007, 2008 Edward Tomasz Napierała <trasz@FreeBSD.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * jack-keyboard is a virtual keyboard for JACK MIDI.
 *
 * For questions and comments, you can contact:
 * - Edward Tomasz Napierala <trasz@FreeBSD.org>
 * - Hans Petter Selasky <hselasky@FreeBSD.org>.
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/time.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>
#include <sysexits.h>
#include <errno.h>
#include <jack/jack.h>
#include <jack/midiport.h>
#include <jack/ringbuffer.h>


/*
  In main:
  alloc some jack stuff
   jack_client_t	*jack_client = NULL;
   jack_port_t	*output_port;
   jack_ringbuffer_t *ringbuffer;
  init_jack(0)

  In IRQ
  queue_new_message(MIDI_NOTE_ON, note, *current_velocity);
  */


#ifdef HAVE_LASH
#include <lash/lash.h>
#endif

#define VELOCITY_MAX		127
#define VELOCITY_HIGH		100
#define VELOCITY_NORMAL		64
#define VELOCITY_LOW		32
#define VELOCITY_MIN		1

#define OUTPUT_PORT_NAME	"midi_out"
#define INPUT_PORT_NAME		"midi_in"
#define PACKAGE_NAME		"jack-keyboard"
#define PACKAGE_VERSION		"2.7.2"

jack_port_t	*output_port;
jack_port_t	*input_port;
int		entered_number = -1;
int		allow_connecting_to_own_kind = 0;
int		enable_gui = 1;
int		grab_keyboard_at_startup = 0;
volatile int	keyboard_grabbed = 0;
int		enable_window_title = 0;
int		time_offsets_are_zero = 0;
int		send_program_change_at_reconnect = 0;
int		send_program_change_once = 0;
int		program_change_was_sent = 0;
int		velocity_high = VELOCITY_HIGH;
int		velocity_normal = VELOCITY_NORMAL;
int		velocity_low = VELOCITY_LOW;
int		*current_velocity = &velocity_normal;
int		octave = 4;
double		rate_limit = 0.0;
jack_client_t	*jack_client = NULL;


#define MIDI_NOTE_ON		0x90
#define MIDI_NOTE_OFF		0x80
#define MIDI_PROGRAM_CHANGE	0xC0
#define MIDI_CONTROLLER		0xB0
#define MIDI_RESET		0xFF
#define MIDI_HOLD_PEDAL		64
#define MIDI_ALL_SOUND_OFF	120
#define MIDI_ALL_MIDI_CONTROLLERS_OFF	121
#define MIDI_ALL_NOTES_OFF	123
#define MIDI_BANK_SELECT_MSB	0
#define MIDI_BANK_SELECT_LSB	32

#define BANK_MIN		0
#define BANK_MAX		127
#define PROGRAM_MIN		0
#define PROGRAM_MAX		127
#define CHANNEL_MIN		1
#define CHANNEL_MAX		16

struct MidiMessage {
	jack_nframes_t	time;
	int		len;	/* Length of MIDI message, in bytes. */
	unsigned char	data[3];
};

#define RINGBUFFER_SIZE		1024*sizeof(struct MidiMessage)

/* Will emit a warning if time between jack callbacks is longer than this. */
#define MAX_TIME_BETWEEN_CALLBACKS	0.1

/* Will emit a warning if execution of jack callback takes longer than this. */
#define MAX_PROCESSING_TIME	0.01

jack_ringbuffer_t *ringbuffer;

/* Number of currently used program. */
int		program = 0;

/* Number of currently selected bank. */
int		bank = 0;

/* Number of currently selected channel (0..15). */
int		channel = 0;

void draw_note(int key);
void queue_message(struct MidiMessage *ev);

double 
get_time(void)
{
	double seconds;
	int ret;
	struct timeval tv;

	ret = gettimeofday(&tv, NULL);

	if (ret) {
		perror("gettimeofday");
		exit(EX_OSERR);
	}

	seconds = tv.tv_sec + tv.tv_usec / 1000000.0;

	return (seconds);
}

double
get_delta_time(void)
{
	static double previously = -1.0;
	double now;
	double delta;

	now = get_time();

	if (previously == -1.0) {
		previously = now;

		return (0);
	}

	delta = now - previously;
	previously = now;

	assert(delta >= 0.0);

	return (delta);
}


double
nframes_to_ms(jack_nframes_t nframes)
{
	jack_nframes_t sr;

	sr = jack_get_sample_rate(jack_client);

	assert(sr > 0);

	return ((nframes * 1000.0) / (double)sr);
}

void
process_midi_output(jack_nframes_t nframes)
{
	int read, t, bytes_remaining;
	unsigned char *buffer;
	void *port_buffer;
	jack_nframes_t last_frame_time;
	struct MidiMessage ev;

	last_frame_time = jack_last_frame_time(jack_client);

	port_buffer = jack_port_get_buffer(output_port, nframes);
	if (port_buffer == NULL) {
		warn_from_jack_thread_context("jack_port_get_buffer failed, cannot send anything.");
		return;
	}

#ifdef JACK_MIDI_NEEDS_NFRAMES
	jack_midi_clear_buffer(port_buffer, nframes);
#else
	jack_midi_clear_buffer(port_buffer);
#endif

	/* We may push at most one byte per 0.32ms to stay below 31.25 Kbaud limit. */
	bytes_remaining = nframes_to_ms(nframes) * rate_limit;

	while (jack_ringbuffer_read_space(ringbuffer)) {
		read = jack_ringbuffer_peek(ringbuffer, (char *)&ev, sizeof(ev));

		if (read != sizeof(ev)) {
            //warn_from_jack_thread_context("Short read from the ringbuffer, possible note loss.");
			jack_ringbuffer_read_advance(ringbuffer, read);
			continue;
		}

		bytes_remaining -= ev.len;

		if (rate_limit > 0.0 && bytes_remaining <= 0) {
            //warn_from_jack_thread_context("Rate limiting in effect.");
			break;
		}

		t = ev.time + nframes - last_frame_time;

		/* If computed time is too much into the future, we'll need
		   to send it later. */
		if (t >= (int)nframes)
			break;

		/* If computed time is < 0, we missed a cycle because of xrun. */
		if (t < 0)
			t = 0;

		if (time_offsets_are_zero)
			t = 0;

		jack_ringbuffer_read_advance(ringbuffer, sizeof(ev));

#ifdef JACK_MIDI_NEEDS_NFRAMES
		buffer = jack_midi_event_reserve(port_buffer, t, ev.len, nframes);
#else
		buffer = jack_midi_event_reserve(port_buffer, t, ev.len);
#endif

		if (buffer == NULL) {
            //warn_from_jack_thread_context("jack_midi_event_reserve failed, NOTE LOST.");
			break;
		}

		memcpy(buffer, ev.data, ev.len);
	}
}

int 
process_callback(jack_nframes_t nframes, void *notused)
{
#ifdef MEASURE_TIME
	if (get_delta_time() > MAX_TIME_BETWEEN_CALLBACKS)
		warn_from_jack_thread_context("Had to wait too long for JACK callback; scheduling problem?");
#endif

	/* Check for impossible condition that actually happened to me, caused by some problem between jackd and OSS4. */
	if (nframes <= 0) {
        //warn_from_jack_thread_context("Process callback called with nframes = 0; bug in JACK?");
		return 0;
	}

	process_midi_output(nframes);

#ifdef MEASURE_TIME
	if (get_delta_time() > MAX_PROCESSING_TIME)
		warn_from_jack_thread_context("Processing took too long; scheduling problem?");
#endif

	return (0);
}

void
queue_message(struct MidiMessage *ev)
{
	int written;

	if (jack_ringbuffer_write_space(ringbuffer) < sizeof(*ev)) {
		g_critical("Not enough space in the ringbuffer, NOTE LOST.");
		return;
	}

	written = jack_ringbuffer_write(ringbuffer, (char *)ev, sizeof(*ev));

	if (written != sizeof(*ev))
		g_warning("jack_ringbuffer_write failed, NOTE LOST.");
}

void 
queue_new_message(int b0, int b1, int b2)
{
	struct MidiMessage ev;

	/* For MIDI messages that specify a channel number, filter the original
	   channel number out and add our own. */
	if (b0 >= 0x80 && b0 <= 0xEF) {
		b0 &= 0xF0;
		b0 += channel;
	}

	if (b1 == -1) {
		ev.len = 1;
		ev.data[0] = b0;

	} else if (b2 == -1) {
		ev.len = 2;
		ev.data[0] = b0;
		ev.data[1] = b1;

	} else {
		ev.len = 3;
		ev.data[0] = b0;
		ev.data[1] = b1;
		ev.data[2] = b2;
	}

	ev.time = jack_frame_time(jack_client);

	queue_message(&ev);
}

//not using, but might later
void
send_program_change(void)
{
	if (jack_port_connected(output_port) == 0)
		return;

	queue_new_message(MIDI_CONTROLLER, MIDI_BANK_SELECT_LSB, bank % 128);
	queue_new_message(MIDI_CONTROLLER, MIDI_BANK_SELECT_MSB, bank / 128);
	queue_new_message(MIDI_PROGRAM_CHANGE, program, -1);

	program_change_was_sent = 1;
}

void 
init_jack(void)
{
	int err;

    jack_client = jack_client_open("Game Drumkit Client", JackNoStartServer, NULL);

	if (jack_client == NULL) {
        printf("Could not connect to the JACK server; run jackd first?");
        //exit(EX_UNAVAILABLE);
	}

	ringbuffer = jack_ringbuffer_create(RINGBUFFER_SIZE);

	if (ringbuffer == NULL) {
        printf("Cannot create JACK ringbuffer.");
        //exit(EX_SOFTWARE);
	}

	jack_ringbuffer_mlock(ringbuffer);


	err = jack_set_process_callback(jack_client, process_callback, 0);
	if (err) {
        printf("Could not register JACK process callback.");
        //exit(EX_UNAVAILABLE);
	}

	err = jack_set_graph_order_callback(jack_client, graph_order_callback, 0);
	if (err) {
        printf("Could not register JACK graph order callback.");
        //exit(EX_UNAVAILABLE);
	}

	output_port = jack_port_register(jack_client, OUTPUT_PORT_NAME, JACK_DEFAULT_MIDI_TYPE,
		JackPortIsOutput, 0);

	if (output_port == NULL) {
        printf("Could not register JACK output port.");
        //exit(EX_UNAVAILABLE);
	}

	if (jack_activate(jack_client)) {
        printf("Cannot activate JACK client.");
        //exit(EX_UNAVAILABLE);
	}
}

void 
panic(void)
{
	int i;

	/*
	 * These two have to be sent first, in case we have no room in the
	 * ringbuffer for all these MIDI_NOTE_OFF messages sent five lines below.
	 */
	queue_new_message(MIDI_CONTROLLER, MIDI_ALL_NOTES_OFF, 0);
	queue_new_message(MIDI_CONTROLLER, MIDI_ALL_SOUND_OFF, 0);

	for (i = 0; i < NNOTES; i++) {
		queue_new_message(MIDI_NOTE_OFF, i, 0);
		piano_keyboard_set_note_off(keyboard, i);
		usleep(100);
	}

	queue_new_message(MIDI_CONTROLLER, MIDI_HOLD_PEDAL, 0);
	queue_new_message(MIDI_CONTROLLER, MIDI_ALL_MIDI_CONTROLLERS_OFF, 0);
	queue_new_message(MIDI_CONTROLLER, MIDI_ALL_NOTES_OFF, 0);
	queue_new_message(MIDI_CONTROLLER, MIDI_ALL_SOUND_OFF, 0);
	queue_new_message(MIDI_RESET, -1, -1);
}
