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

#ifdef HAVE_LASH
lash_client_t	*lash_client;
#endif

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

gboolean
process_received_message_async(gpointer evp)
{
	int i;
	struct MidiMessage *ev = (struct MidiMessage *)evp;
	int b0 = ev->data[0];
	int b1 = ev->data[1];

	/* Strip channel from channel messages */
	if (b0 >= 0x80 && b0 <= 0xEF)
		b0 = b0 & 0xF0;

	if (b0 == MIDI_RESET || (b0 == MIDI_CONTROLLER &&
		(b1 == MIDI_ALL_NOTES_OFF || b1 == MIDI_ALL_SOUND_OFF))) {

		for (i = 0; i < NNOTES; i++) {
			piano_keyboard_set_note_off(keyboard, i);
		}
	}

	if (b0 == MIDI_NOTE_ON) {
		if (ev->data[2] == 0)
			piano_keyboard_set_note_off(keyboard, ev->data[1]);
		else
			piano_keyboard_set_note_on(keyboard, ev->data[1]);
	}

	if (b0 == MIDI_NOTE_OFF) {
		piano_keyboard_set_note_off(keyboard, ev->data[1]);
	}

	ev->data [0] = b0 | channel;
	queue_message(ev);

	return (FALSE);
}

struct MidiMessage *
midi_message_from_midi_event(jack_midi_event_t event)
{
	struct MidiMessage *ev = malloc(sizeof(*ev));

	if (ev == NULL) {
		perror("malloc");
		return (NULL);
	}

	assert(event.size >= 1 && event.size <= 3);

	ev->len = event.size;
	ev->time = event.time;

	memcpy(ev->data, event.buffer, ev->len);

	return (ev);
}

gboolean
warning_async(gpointer s)
{
	const char *str = (const char *)s;

	g_warning(str);

	return (FALSE);
}

void
warn_from_jack_thread_context(const char *str)
{
	g_idle_add(warning_async, (gpointer)str);
}

void
process_midi_input(jack_nframes_t nframes)
{
	int read, events, i;
	void *port_buffer;
	jack_midi_event_t event;

	port_buffer = jack_port_get_buffer(input_port, nframes);
	if (port_buffer == NULL) {
		warn_from_jack_thread_context("jack_port_get_buffer failed, cannot receive anything.");
		return;
	}

#ifdef JACK_MIDI_NEEDS_NFRAMES
	events = jack_midi_get_event_count(port_buffer, nframes);
#else
	events = jack_midi_get_event_count(port_buffer);
#endif

	for (i = 0; i < events; i++) {
		struct MidiMessage *rev;

#ifdef JACK_MIDI_NEEDS_NFRAMES
		read = jack_midi_event_get(&event, port_buffer, i, nframes);
#else
		read = jack_midi_event_get(&event, port_buffer, i);
#endif
		if (read) {
			warn_from_jack_thread_context("jack_midi_event_get failed, RECEIVED NOTE LOST.");
			continue;
		}

		if (event.size > 3) {
			warn_from_jack_thread_context("Ignoring MIDI message longer than three bytes, probably a SysEx.");
			continue;
		}

		rev = midi_message_from_midi_event(event);
		if (rev == NULL) {
			warn_from_jack_thread_context("midi_message_from_midi_event failed, RECEIVED NOTE LOST.");
			continue;
		}

		g_idle_add(process_received_message_async, rev);
	}
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
			warn_from_jack_thread_context("Short read from the ringbuffer, possible note loss.");
			jack_ringbuffer_read_advance(ringbuffer, read);
			continue;
		}

		bytes_remaining -= ev.len;

		if (rate_limit > 0.0 && bytes_remaining <= 0) {
			warn_from_jack_thread_context("Rate limiting in effect.");
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
			warn_from_jack_thread_context("jack_midi_event_reserve failed, NOTE LOST.");
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
		warn_from_jack_thread_context("Process callback called with nframes = 0; bug in JACK?");
		return 0;
	}

	process_midi_input(nframes);
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

gboolean
update_connected_to_combo_async(gpointer notused)
{
	int i, count = 0;
	const char **connected, **available, *my_name;
	GtkTreeIter iter;

	if (jack_client == NULL || output_port == NULL)
		return (FALSE);

	connected = jack_port_get_connections(output_port);
	available = jack_get_ports(jack_client, NULL, JACK_DEFAULT_MIDI_TYPE, JackPortIsInput);
	my_name = jack_port_name(input_port);

	assert(my_name);

	/* There will be at least one listening MIDI port - the one we create. */
	assert(available);

	if (available != NULL) {
		gtk_list_store_clear(connected_to_store);

		for (i = 0; available[i] != NULL; i++) {
			if (!strcmp(available[i], my_name) || (!allow_connecting_to_own_kind &&
			    !strncmp(available[i], my_name, strlen(PACKAGE_NAME))))

				continue;

			count++;

			gtk_list_store_append(connected_to_store, &iter);
			gtk_list_store_set(connected_to_store, &iter, 0, available[i], -1);

			if (connected != NULL && connected[0] != NULL && !strcmp(available[i], connected[0]))
				gtk_combo_box_set_active_iter(GTK_COMBO_BOX(connected_to_combo), &iter);
		}
	}

	if (count > 0)
		gtk_widget_set_sensitive(connected_to_combo, TRUE);
	else
		gtk_widget_set_sensitive(connected_to_combo, FALSE);

	if (connected != NULL)
		free(connected);

	free(available);

	return (FALSE);
}



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

/* Connects to the specified input port, disconnecting already connected ports. */
int
connect_to_input_port(const char *port)
{
        int ret;

	if (!strcmp(port, jack_port_name(input_port)))
		return (-1);

	if (!allow_connecting_to_own_kind) {
		if (!strncmp(port, jack_port_name(input_port), strlen(PACKAGE_NAME)))
			return (-2);
	}

        ret = jack_port_disconnect(jack_client, output_port);
        if (ret) {
                g_warning("Cannot disconnect MIDI port.");

                return (-3);
        }

        ret = jack_connect(jack_client, jack_port_name(output_port), port);
        if (ret) {
                g_warning("Cannot connect to %s.", port);

                return (-4);
        } 

	g_warning("Connected to %s.", port);

	program_change_was_sent = 0;

	if (send_program_change_at_reconnect || send_program_change_once) {
		send_program_change();
		draw_window_title();
	}

	send_program_change_once = 0;

	return (0);
}

void
connect_to_another_input_port(int up_not_down)
{
	const char **available_midi_ports;
	const char **connected_ports;
	const char *current = NULL;
	int i, max, current_index;

	available_midi_ports = jack_get_ports(jack_client, NULL, JACK_DEFAULT_MIDI_TYPE, JackPortIsInput);

	/* There will be at least one listening MIDI port - the one we create. */
	assert(available_midi_ports);

	/*
	 * max is the highest possible index into available_midi_ports[],
	 * i.e. number of elements - 1.
	 */
	for (max = 0; available_midi_ports[max + 1] != NULL; max++);

	/* Only one input port - our own. */
	if (max == 0) {
		g_warning("No listening JACK MIDI input ports found.  "
		    "Run some softsynth and press Insert or Delete key.");

		return;
	}

	connected_ports = jack_port_get_connections(output_port);

	if (connected_ports != NULL && connected_ports[0] != NULL)
		current = connected_ports[0];
	else
		current = available_midi_ports[0];

	/*
	 * current is the index of currently connected port into available_midi_ports[].
	 */
	for (i = 0; i <= max && strcmp(available_midi_ports[i], current); i++);

	current_index = i;

	assert(!strcmp(available_midi_ports[current_index], current));

	/* XXX: rewrite. */
	if (up_not_down) {
		for (i = current_index + 1; i <= max; i++) {
			assert(available_midi_ports[i] != NULL);

			if (connect_to_input_port(available_midi_ports[i]) == 0)
				goto connected;
		}

		for (i = 0; i <= current_index; i++) {
			assert(available_midi_ports[i] != NULL);

			if (connect_to_input_port(available_midi_ports[i]) == 0)
				goto connected;
		}

	} else {
		for (i = current_index - 1; i >= 0; i--) {
			assert(available_midi_ports[i] != NULL);

			if (connect_to_input_port(available_midi_ports[i]) == 0)
				goto connected;
		}

		for (i = max; i >= current_index; i--) {
			assert(available_midi_ports[i] != NULL);

			if (connect_to_input_port(available_midi_ports[i]) == 0)
				goto connected;
		}
	}

	g_warning("Cannot connect to any of the input ports.");

connected:
	free(available_midi_ports);

	if (connected_ports != NULL)
		free(connected_ports);
}

void 
connect_to_next_input_port(void)
{
	connect_to_another_input_port(1);
}

void 
connect_to_prev_input_port(void)
{
	connect_to_another_input_port(0);
}

void 
init_jack(void)
{
	int err;

#ifdef HAVE_LASH
	lash_event_t *event;
#endif

	jack_client = jack_client_open(PACKAGE_NAME, JackNoStartServer, NULL);

	if (jack_client == NULL) {
		g_critical("Could not connect to the JACK server; run jackd first?");
		exit(EX_UNAVAILABLE);
	}

	ringbuffer = jack_ringbuffer_create(RINGBUFFER_SIZE);

	if (ringbuffer == NULL) {
		g_critical("Cannot create JACK ringbuffer.");
		exit(EX_SOFTWARE);
	}

	jack_ringbuffer_mlock(ringbuffer);

#ifdef HAVE_LASH
	event = lash_event_new_with_type(LASH_Client_Name);
	assert (event); /* Documentation does not say anything about return value. */
	lash_event_set_string(event, jack_get_client_name(jack_client));
	lash_send_event(lash_client, event);

	lash_jack_client_name(lash_client, jack_get_client_name(jack_client));
#endif

	err = jack_set_process_callback(jack_client, process_callback, 0);
	if (err) {
		g_critical("Could not register JACK process callback.");
		exit(EX_UNAVAILABLE);
	}

	err = jack_set_graph_order_callback(jack_client, graph_order_callback, 0);
	if (err) {
		g_critical("Could not register JACK graph order callback.");
		exit(EX_UNAVAILABLE);
	}

	output_port = jack_port_register(jack_client, OUTPUT_PORT_NAME, JACK_DEFAULT_MIDI_TYPE,
		JackPortIsOutput, 0);

	if (output_port == NULL) {
		g_critical("Could not register JACK output port.");
		exit(EX_UNAVAILABLE);
	}

	input_port = jack_port_register(jack_client, INPUT_PORT_NAME, JACK_DEFAULT_MIDI_TYPE,
		JackPortIsInput, 0);

	if (input_port == NULL) {
		g_critical("Could not register JACK input port.");
		exit(EX_UNAVAILABLE);
	}

	if (jack_activate(jack_client)) {
		g_critical("Cannot activate JACK client.");
		exit(EX_UNAVAILABLE);
	}
}

#ifdef HAVE_LASH

void
load_config_from_lash(void)
{
	lash_config_t *config;
	const char *key;
	int value;

	while ((config = lash_get_config(lash_client))) {
		
		key = lash_config_get_key(config);
		value = lash_config_get_value_int(config);

		if (!strcmp(key, "channel")) {
			if (value < CHANNEL_MIN || value > CHANNEL_MAX) {
				g_warning("Bad value '%d' for 'channel' property received from LASH.", value);
			} else {
				gtk_spin_button_set_value(GTK_SPIN_BUTTON(channel_spin), value);
			}

		} else if (!strcmp(key, "bank")) {
			if (value < BANK_MIN || value > BANK_MAX) {
				g_warning("Bad value '%d' for 'bank' property received from LASH.", value);
			} else {
				gtk_spin_button_set_value(GTK_SPIN_BUTTON(bank_spin), value);
			}

		} else if (!strcmp(key, "program")) {
			if (value < PROGRAM_MIN || value > PROGRAM_MAX) {
				g_warning("Bad value '%d' for 'program' property received from LASH.", value);
			} else {
				gtk_spin_button_set_value(GTK_SPIN_BUTTON(program_spin), value);
			}

		} else if (!strcmp(key, "keyboard_grabbed")) {
			if (value < 0 || value > 1) {
				g_warning("Bad value '%d' for 'keyboard_grabbed' property received from LASH.", value);
			} else {
				gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(grab_keyboard_checkbutton), value);
			}

		} else if (!strcmp(key, "octave")) {
			if (value < OCTAVE_MIN || value > OCTAVE_MAX) {
				g_warning("Bad value '%d' for 'octave' property received from LASH.", value);
			} else {
				gtk_spin_button_set_value(GTK_SPIN_BUTTON(octave_spin), value);
			}

		} else if (!strcmp(key, "velocity_normal")) {
			if (value < VELOCITY_MIN || value > VELOCITY_MAX) {
				g_warning("Bad value '%d' for 'velocity_normal' property received from LASH.", value);
			} else {
		                velocity_normal = value;
				gtk_range_set_value(GTK_RANGE(velocity_hscale), *current_velocity);
			}

		} else if (!strcmp(key, "velocity_high")) {
			if (value < VELOCITY_MIN || value > VELOCITY_MAX) {
				g_warning("Bad value '%d' for 'velocity_high' property received from LASH.", value);
			} else {
		                velocity_high = value;
				gtk_range_set_value(GTK_RANGE(velocity_hscale), *current_velocity);
			}

		} else if (!strcmp(key, "velocity_low")) {
			if (value < VELOCITY_MIN || value > VELOCITY_MAX) {
				g_warning("Bad value '%d' for 'velocity_low' property received from LASH.", value);
			} else {
		                velocity_low = value;
				gtk_range_set_value(GTK_RANGE(velocity_hscale), *current_velocity);
			}

		} else {
			g_warning("Received unknown config key '%s' (value '%d') from LASH.", key, value);
		}

		lash_config_destroy(config);
	}
}

void
save_config_int(const char *name, int value)
{
	lash_config_t *config;

	config = lash_config_new_with_key(name);
	lash_config_set_value_int(config, value);
	lash_send_config(lash_client, config);
}

void
save_config_into_lash(void)
{
	save_config_int("channel", channel + 1);
	save_config_int("bank", bank);
	save_config_int("program", program);
	save_config_int("keyboard_grabbed", keyboard_grabbed);
	save_config_int("octave", octave);
	save_config_int("velocity_normal", velocity_normal);
	save_config_int("velocity_high", velocity_high);
	save_config_int("velocity_low", velocity_low);
}

gboolean
lash_callback(gpointer notused)
{
	lash_event_t *event;

	while ((event = lash_get_event(lash_client))) {
		switch (lash_event_get_type(event)) {
		case LASH_Restore_Data_Set:
			load_config_from_lash();
			lash_send_event(lash_client, event);
			break;

		case LASH_Save_Data_Set:
			save_config_into_lash();
			lash_send_event(lash_client, event);
			break;

		case LASH_Quit:
			g_warning("Exiting due to LASH request.");
			exit(EX_OK);
			break;

		default:
			g_warning("Receieved unknown LASH event of type %d.", lash_event_get_type(event));
			lash_event_destroy(event);
		}
	}

	return (TRUE);
}

void
init_lash(lash_args_t *args)
{
	/* XXX: Am I doing the right thing wrt protocol version? */
	lash_client = lash_init(args, PACKAGE_NAME, LASH_Config_Data_Set, LASH_PROTOCOL(2, 0));

	if (!lash_server_connected(lash_client)) {
		g_critical("Cannot initialize LASH.  Continuing anyway.");
		/* exit(EX_UNAVAILABLE); */

		return;
	}

	/* Schedule a function to process LASH events, ten times per second. */
	g_timeout_add(100, lash_callback, NULL);
}

#endif /* HAVE_LASH */

gboolean
sustain_event_handler(GtkToggleButton *widget, gpointer pressed)
{
	if (pressed) {
		gtk_toggle_button_set_active(widget, TRUE);
		piano_keyboard_sustain_press(keyboard);
	} else {
		gtk_toggle_button_set_active(widget, FALSE);
		piano_keyboard_sustain_release(keyboard);
	}

	return (FALSE);
}

void
channel_event_handler(GtkSpinButton *spinbutton, gpointer notused)
{
	channel = gtk_spin_button_get_value(spinbutton) - 1;

	draw_window_title();
}

void
bank_event_handler(GtkSpinButton *spinbutton, gpointer notused)
{
	bank = gtk_spin_button_get_value(spinbutton);

	send_program_change();
	draw_window_title();
}

void
program_event_handler(GtkSpinButton *spinbutton, gpointer notused)
{
	program = gtk_spin_button_get_value(spinbutton);

	send_program_change();
	draw_window_title();
}

void
connected_to_event_handler(GtkComboBox *widget, gpointer notused)
{
	GtkTreeIter iter;
	gchar *connect_to;
	const char **connected_ports;

	if (gtk_combo_box_get_active_iter(GTK_COMBO_BOX(connected_to_combo), &iter) == FALSE)
		return;

	gtk_tree_model_get(GTK_TREE_MODEL(connected_to_store), &iter, 0, &connect_to, -1);

	connected_ports = jack_port_get_connections(output_port);
	if (connected_ports != NULL && connected_ports[0] != NULL && !strcmp(connect_to, connected_ports[0])) {
		free(connected_ports);
		return;
	}

	connect_to_input_port(connect_to);

	free(connected_ports);
}

void
velocity_event_handler(GtkRange *range, gpointer notused)
{
	assert(current_velocity);

	*current_velocity = gtk_range_get_value(range);
}

void
grab_keyboard_handler(GtkToggleButton *togglebutton, gpointer notused)
{
	gboolean active = gtk_toggle_button_get_active(togglebutton);

	if (active)
		grab_keyboard();
	else
		ungrab_keyboard();
}

void
octave_event_handler(GtkSpinButton *spinbutton, gpointer notused)
{
	octave = gtk_spin_button_get_value(spinbutton);
	piano_keyboard_set_octave(keyboard, octave);
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

void
add_digit(int digit)
{
	if (entered_number == -1)
		entered_number = 0;
	else
		entered_number *= 10;

	entered_number += digit;
}

int
maybe_add_digit(GdkEventKey *event)
{
	/*
	 * User can enter a number from the keypad; after that, pressing
	 * '*'/'/', Page Up/Page Down etc will set program/bank/whatever
	 * to the number that was entered.
	 */
	/*
	 * XXX: This is silly.  Find a way to enter the number without
	 * all these contitional instructions.
	 */
	if (event->keyval == GDK_KP_0 || event->keyval == GDK_KP_Insert) {
		if (event->type == GDK_KEY_PRESS)
			add_digit(0);

		return (TRUE);
	}

	if (event->keyval == GDK_KP_1 || event->keyval == GDK_KP_End) {
		if (event->type == GDK_KEY_PRESS)
			add_digit(1);

		return (TRUE);
	}

	if (event->keyval == GDK_KP_2 || event->keyval == GDK_KP_Down) {
		if (event->type == GDK_KEY_PRESS)
			add_digit(2);

		return (TRUE);
	}

	if (event->keyval == GDK_KP_3 || event->keyval == GDK_KP_Page_Down) {
		if (event->type == GDK_KEY_PRESS)
			add_digit(3);

		return (TRUE);
	}

	if (event->keyval == GDK_KP_4 || event->keyval == GDK_KP_Left) {
		if (event->type == GDK_KEY_PRESS)
			add_digit(4);

		return (TRUE);
	}

	if (event->keyval == GDK_KP_5 || event->keyval == GDK_KP_Begin) {
		if (event->type == GDK_KEY_PRESS)
			add_digit(5);

		return (TRUE);
	}

	if (event->keyval == GDK_KP_6 || event->keyval == GDK_KP_Right) {
		if (event->type == GDK_KEY_PRESS)
			add_digit(6);

		return (TRUE);
	}

	if (event->keyval == GDK_KP_7 || event->keyval == GDK_KP_Home) {
		if (event->type == GDK_KEY_PRESS)
			add_digit(7);

		return (TRUE);
	}

	if (event->keyval == GDK_KP_8 || event->keyval == GDK_KP_Up) {
		if (event->type == GDK_KEY_PRESS)
			add_digit(8);

		return (TRUE);
	}

	if (event->keyval == GDK_KP_9 || event->keyval == GDK_KP_Page_Up) {
		if (event->type == GDK_KEY_PRESS)
			add_digit(9);

		return (TRUE);
	}

	return (FALSE);
}

int
get_entered_number(void)
{
	int tmp;

	tmp = entered_number;

	entered_number = -1;

	return (tmp);
}

int
clip(int val, int lo, int hi)
{
	if (val < lo)
		val = lo;

	if (val > hi)
		val = hi;

	return (val);
}

gint 
keyboard_event_handler(GtkWidget *widget, GdkEventKey *event, gpointer notused)
{
	int tmp;
	gboolean retval = FALSE;

	/* Pass signal to piano_keyboard widget.  Is there a better way to do this? */
	if (event->type == GDK_KEY_PRESS)
		g_signal_emit_by_name(keyboard, "key-press-event", event, &retval);
	else
		g_signal_emit_by_name(keyboard, "key-release-event", event, &retval);

	if (retval)
		return (TRUE);


	if (maybe_add_digit(event))
		return (TRUE);

	/*
	 * '+' key shifts octave up. '-' key shifts octave down.
	 */
	if (event->keyval == GDK_KP_Add || event->keyval  == GDK_equal) {
		if (event->type == GDK_KEY_PRESS && octave < OCTAVE_MAX)
			gtk_spin_button_set_value(GTK_SPIN_BUTTON(octave_spin), octave + 1);

		return (TRUE);
	}

	if (event->keyval == GDK_KP_Subtract || event->keyval == GDK_minus) {
		if (event->type == GDK_KEY_PRESS && octave > OCTAVE_MIN)
			gtk_spin_button_set_value(GTK_SPIN_BUTTON(octave_spin), octave - 1);

		return (TRUE);
	}

	/*
	 * '*' character increases program number. '/' character decreases it.
	 */
	if (event->keyval == GDK_KP_Multiply) {
		if (event->type == GDK_KEY_PRESS) {

			tmp = get_entered_number();

			if (tmp < 0)
				tmp = gtk_spin_button_get_value(GTK_SPIN_BUTTON(program_spin)) + 1;

			gtk_spin_button_set_value(GTK_SPIN_BUTTON(program_spin), clip(tmp, PROGRAM_MIN, PROGRAM_MAX));
		}

		return (TRUE);
	}

	if (event->keyval == GDK_KP_Divide) {
		if (event->type == GDK_KEY_PRESS) {

			tmp = get_entered_number();

			if (tmp < 0)
				tmp = gtk_spin_button_get_value(GTK_SPIN_BUTTON(program_spin)) - 1;

			gtk_spin_button_set_value(GTK_SPIN_BUTTON(program_spin), clip(tmp, PROGRAM_MIN, PROGRAM_MAX));
		}

		return (TRUE);
	}

	/*
	 * PgUp key increases bank number, PgDown decreases it.
	 */
	if (event->keyval == GDK_Page_Up) {
		if (event->type == GDK_KEY_PRESS) {

			tmp = get_entered_number();

			if (tmp < 0)
				tmp = gtk_spin_button_get_value(GTK_SPIN_BUTTON(bank_spin)) + 1;

			gtk_spin_button_set_value(GTK_SPIN_BUTTON(bank_spin), clip(tmp, BANK_MIN, BANK_MAX));
		}

		return (TRUE);
	}

	if (event->keyval == GDK_Page_Down) {
		if (event->type == GDK_KEY_PRESS) {

			tmp = get_entered_number();

			if (tmp < 0)
				tmp = gtk_spin_button_get_value(GTK_SPIN_BUTTON(bank_spin)) - 1;

			gtk_spin_button_set_value(GTK_SPIN_BUTTON(bank_spin), clip(tmp, BANK_MIN, BANK_MAX));
		}

		return (TRUE);
	}

	/*
	 * Home key increases channel number, End decreases it.
	 */
	if (event->keyval == GDK_Home) {
		if (event->type == GDK_KEY_PRESS) {

			tmp = get_entered_number();

			if (tmp < 0)
				tmp = gtk_spin_button_get_value(GTK_SPIN_BUTTON(channel_spin)) + 1;

			gtk_spin_button_set_value(GTK_SPIN_BUTTON(channel_spin), clip(tmp, CHANNEL_MIN, CHANNEL_MAX));
		}

		return (TRUE);
	}

	if (event->keyval == GDK_End) {
		if (event->type == GDK_KEY_PRESS) {

			tmp = get_entered_number();

			if (tmp < 0)
				tmp = gtk_spin_button_get_value(GTK_SPIN_BUTTON(channel_spin)) - 1;

			gtk_spin_button_set_value(GTK_SPIN_BUTTON(channel_spin), clip(tmp, CHANNEL_MIN, CHANNEL_MAX));
		}

		return (TRUE);
	}

	/*
	 * Insert key connects to the next input port.  Delete connects to the previous one.
	 */
	if (event->keyval == GDK_Insert) {
		if (event->type == GDK_KEY_PRESS)
			connect_to_next_input_port();

		return (TRUE);
	}

	if (event->keyval == GDK_Delete) {
		if (event->type == GDK_KEY_PRESS)
			connect_to_prev_input_port();

		return (TRUE);
	}

	if (event->keyval == GDK_Escape) {
		if (event->type == GDK_KEY_PRESS)
			panic();

		return (TRUE);
	}

	/*
	 * Spacebar works as a 'sustain' key.  Holding spacebar while
	 * releasing note will cause the note to continue. Pressing and
	 * releasing spacebar without pressing any note keys will make all
	 * the sustained notes end (it will send 'note off' midi messages for
	 * all the sustained notes).
	 */
	if (event->keyval == GDK_space) {
		if (event->type == GDK_KEY_PRESS)
			gtk_button_pressed(GTK_BUTTON(sustain_button));
		else
			gtk_button_released(GTK_BUTTON(sustain_button));

		return (TRUE);
	}

	/*
	 * Shift increases velocity, i.e. holding it while pressing note key
	 * will make the sound louder. Ctrl decreases velocity.
	 */
	if (event->keyval == GDK_Shift_L || event->keyval == GDK_Shift_R) {
		if (event->type == GDK_KEY_PRESS)
			current_velocity = &velocity_high;
		else
			current_velocity = &velocity_normal;

		gtk_range_set_value(GTK_RANGE(velocity_hscale), *current_velocity);

		return (TRUE);

	}

	if (event->keyval == GDK_Control_L || event->keyval == GDK_Control_R) {
		if (event->type == GDK_KEY_PRESS)
			current_velocity = &velocity_low;
		else
			current_velocity = &velocity_normal;

		gtk_range_set_value(GTK_RANGE(velocity_hscale), *current_velocity);

		return (TRUE);
	}

	return (FALSE);
}

void
note_on_event_handler(GtkWidget *widget, int note)
{
	assert(current_velocity);

	queue_new_message(MIDI_NOTE_ON, note, *current_velocity);
}

void
note_off_event_handler(GtkWidget *widget, int note)
{
	assert(current_velocity);

	queue_new_message(MIDI_NOTE_OFF, note, *current_velocity);
}

void
init_gtk_1(int *argc, char ***argv)
{
	GdkPixbuf *icon = NULL;
	GError *error = NULL;

	gtk_init(argc, argv);

	icon = gtk_icon_theme_load_icon(gtk_icon_theme_get_default(), "jack-keyboard", 48, 0, &error);

	if (icon == NULL) {
		fprintf(stderr, "%s: Cannot load icon: %s.\n", G_LOG_DOMAIN, error->message);
		g_error_free(error);

	} else {
		gtk_window_set_default_icon(icon);
	}

	/* Window. */
	window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_container_set_border_width(GTK_CONTAINER(window), 2);
}

void
init_gtk_2(void)
{
	GtkTable *table;
	GtkWidget *label;
	GtkCellRenderer *renderer;

	/* Table. */
	table = GTK_TABLE(gtk_table_new(4, 8, FALSE));
	gtk_table_set_row_spacings(table, 5);
	gtk_table_set_col_spacings(table, 5);

	if (enable_gui)
		gtk_container_add(GTK_CONTAINER(window), GTK_WIDGET(table));

	/* Channel label and spin. */
	label = gtk_label_new("Channel:");
	gtk_misc_set_alignment(GTK_MISC(label), 1, 0.5);
	gtk_table_attach(table, label, 0, 1, 0, 1, GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);
	channel_spin = gtk_spin_button_new_with_range(1, CHANNEL_MAX, 1);
	GTK_WIDGET_UNSET_FLAGS(channel_spin, GTK_CAN_FOCUS);
	gtk_table_attach(table, channel_spin, 1, 2, 0, 1, GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);
	g_signal_connect(G_OBJECT(channel_spin), "value-changed", G_CALLBACK(channel_event_handler), NULL);

	/* Bank label and spin. */
	label = gtk_label_new("Bank:");
	gtk_misc_set_alignment(GTK_MISC(label), 1, 0.5);
	gtk_table_attach(table, label, 2, 3, 0, 1, GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);
	bank_spin = gtk_spin_button_new_with_range(0, BANK_MAX, 1);
	GTK_WIDGET_UNSET_FLAGS(bank_spin, GTK_CAN_FOCUS);
	gtk_table_attach(table, bank_spin, 3, 4, 0, 1, GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);
	g_signal_connect(G_OBJECT(bank_spin), "value-changed", G_CALLBACK(bank_event_handler), NULL);

	/* Program label and spin. */
	label = gtk_label_new("Program:");
	gtk_misc_set_alignment(GTK_MISC(label), 1, 0.5);
	gtk_table_attach(table, label, 4, 5, 0, 1, GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);
	program_spin = gtk_spin_button_new_with_range(0, PROGRAM_MAX, 1);
	GTK_WIDGET_UNSET_FLAGS(program_spin, GTK_CAN_FOCUS);
	gtk_table_attach(table, program_spin, 5, 6, 0, 1, GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);
	g_signal_connect(G_OBJECT(program_spin), "value-changed", G_CALLBACK(program_event_handler), NULL);

	/* "Connected to" label and combo box. */
	label = gtk_label_new("Connected to:");
	gtk_misc_set_alignment(GTK_MISC(label), 1, 0.5);
	gtk_table_attach(table, label, 6, 7, 0, 1, GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);

	connected_to_store = gtk_list_store_new(1, G_TYPE_STRING);
	connected_to_combo = gtk_combo_box_new_with_model(GTK_TREE_MODEL(connected_to_store));

	renderer = gtk_cell_renderer_text_new ();
	gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(connected_to_combo), renderer, FALSE);
	gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT(connected_to_combo), renderer, "text", 0, NULL);

	GTK_WIDGET_UNSET_FLAGS(connected_to_combo, GTK_CAN_FOCUS);
	gtk_combo_box_set_focus_on_click(GTK_COMBO_BOX(connected_to_combo), FALSE);
	gtk_table_attach(table, connected_to_combo, 7, 8, 0, 1, GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);
	gtk_widget_set_size_request(GTK_WIDGET(connected_to_combo), 200, -1);
	g_signal_connect(G_OBJECT(connected_to_combo), "changed", G_CALLBACK(connected_to_event_handler), NULL);

	/* Octave label and spin. */
	label = gtk_label_new("Octave:");
	gtk_misc_set_alignment(GTK_MISC(label), 1, 0.5);
	gtk_table_attach(table, label, 0, 1, 1, 2, GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);
	octave_spin = gtk_spin_button_new_with_range(OCTAVE_MIN, OCTAVE_MAX, 1);
	GTK_WIDGET_UNSET_FLAGS(octave_spin, GTK_CAN_FOCUS);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(octave_spin), octave);
	gtk_table_attach(table, octave_spin, 1, 2, 1, 2, GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);
	g_signal_connect(G_OBJECT(octave_spin), "value-changed", G_CALLBACK(octave_event_handler), NULL);

	/* "Grab keyboard" label and checkbutton. */
	label = gtk_label_new("Grab keyboard:");
	gtk_misc_set_alignment(GTK_MISC(label), 1, 0.5);
	gtk_table_attach(table, label, 4, 5, 1, 2, GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);
	grab_keyboard_checkbutton = gtk_check_button_new();
	GTK_WIDGET_UNSET_FLAGS(grab_keyboard_checkbutton, GTK_CAN_FOCUS);
	g_signal_connect(G_OBJECT(grab_keyboard_checkbutton), "toggled", G_CALLBACK(grab_keyboard_handler), NULL);
	gtk_table_attach(table, grab_keyboard_checkbutton, 5, 6, 1, 2, GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);

	/* Velocity label and hscale */
	label = gtk_label_new("Velocity:");
	gtk_misc_set_alignment(GTK_MISC(label), 1, 0.5);
	gtk_table_attach(table, label, 6, 7, 1, 2, GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);

	velocity_hscale = gtk_hscale_new_with_range(VELOCITY_MIN, VELOCITY_MAX, 1);
	gtk_scale_set_draw_value(GTK_SCALE(velocity_hscale), FALSE);
	GTK_WIDGET_UNSET_FLAGS(velocity_hscale, GTK_CAN_FOCUS);
	g_signal_connect(G_OBJECT(velocity_hscale), "value-changed", G_CALLBACK(velocity_event_handler), NULL);
	gtk_range_set_value(GTK_RANGE(velocity_hscale), VELOCITY_NORMAL);
	gtk_table_attach(table, velocity_hscale, 7, 8, 1, 2, GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);
	gtk_widget_set_size_request(GTK_WIDGET(velocity_hscale), 200, -1);

	/* Sustain. It's a toggle button, not an ordinary one, because we want gtk_whatever_set_active() to work.*/
	sustain_button = gtk_toggle_button_new_with_label("Sustain");
	gtk_button_set_focus_on_click(GTK_BUTTON(sustain_button), FALSE);
	gtk_table_attach(table, sustain_button, 0, 8, 2, 3, GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);
	g_signal_connect(G_OBJECT(sustain_button), "pressed", G_CALLBACK(sustain_event_handler), (void *)1);
	g_signal_connect(G_OBJECT(sustain_button), "released", G_CALLBACK(sustain_event_handler), (void *)0);

	/* PianoKeyboard widget. */
	keyboard = PIANO_KEYBOARD(piano_keyboard_new());

	if (enable_gui)
		gtk_table_attach_defaults(table, GTK_WIDGET(keyboard), 0, 8, 3, 4);
	else
		gtk_container_add(GTK_CONTAINER(window), GTK_WIDGET(keyboard));

	g_signal_connect(G_OBJECT(keyboard), "note-on", G_CALLBACK(note_on_event_handler), NULL);
	g_signal_connect(G_OBJECT(keyboard), "note-off", G_CALLBACK(note_off_event_handler), NULL);
	g_signal_connect(G_OBJECT(window), "destroy", G_CALLBACK(gtk_main_quit), NULL);
	g_signal_connect(G_OBJECT(window), "key-press-event", G_CALLBACK(keyboard_event_handler), NULL);
	g_signal_connect(G_OBJECT(window), "key-release-event", G_CALLBACK(keyboard_event_handler), NULL);
	gtk_widget_show_all(window);

	draw_window_title();
}

void   
log_handler(const gchar *log_domain, GLogLevelFlags log_level, const gchar *message, gpointer notused)
{
	GtkWidget *dialog;

	fprintf(stderr, "%s: %s\n", log_domain, message);

	if ((log_level | G_LOG_LEVEL_CRITICAL) == G_LOG_LEVEL_CRITICAL) {
		dialog = gtk_message_dialog_new(GTK_WINDOW(window), GTK_DIALOG_DESTROY_WITH_PARENT, GTK_MESSAGE_ERROR,
			GTK_BUTTONS_CLOSE, message);

		gtk_dialog_run(GTK_DIALOG(dialog));

		gtk_widget_destroy(dialog);
	}
}

void
show_version(void)
{
	fprintf(stdout, "%s\n", PACKAGE_NAME " v" PACKAGE_VERSION);

	exit(EX_OK);
}

void
usage(void)
{
	fprintf(stderr, "usage: jack-keyboard [-CGKTVkturf] [ -a <input port>] [-c <channel>] [-b <bank> ] [-p <program>] [-l <layout>]\n");
	fprintf(stderr, "   where <channel> is MIDI channel to use for output, from 1 to 16,\n");
	fprintf(stderr, "   <bank> is MIDI bank to use, from 0 to 16383,\n");
	fprintf(stderr, "   <program> is MIDI program to use, from 0 to 127,\n");
	fprintf(stderr, "   and <layout> is  QWERTY, QWERTY_REV, QWERTY_UK, QWERTY_UK_REV, QWERTZ, AZERTY or DVORAK.\n");
	fprintf(stderr, "See manual page for details.\n");

	exit(EX_USAGE);
}

int 
main(int argc, char *argv[])
{
	int ch, enable_keyboard_cue = 0, initial_channel = 1, initial_bank = 0, initial_program = 0, full_midi_keyboard = 0;
	char *keyboard_layout = NULL, *autoconnect_port_name = NULL;

#ifdef HAVE_LASH
	lash_args_t *lash_args;
#endif

	g_thread_init(NULL);

#ifdef HAVE_LASH
	lash_args = lash_extract_args(&argc, &argv);
#endif

	init_gtk_1(&argc, &argv);

	g_log_set_default_handler(log_handler, NULL);

	while ((ch = getopt(argc, argv, "CGKTVa:nktur:c:b:p:l:f")) != -1) {
		switch (ch) {
		case 'C':
			enable_keyboard_cue = 1;
			break;

		case 'G':
			enable_gui = 0;
			enable_window_title = !enable_window_title;
			break;

		case 'K':
			grab_keyboard_at_startup = 1;
			break;

		case 'T':
			enable_window_title = !enable_window_title;
			break;

		case 'V':
			show_version();
			break;

		case 'a':
			autoconnect_port_name = strdup(optarg);
			break;

		case 'n':
			/* Do nothing; backward compatibility. */
			break;

		case 'k':
			allow_connecting_to_own_kind = 1;
			break;

		case 'l':
			keyboard_layout = strdup(optarg);
			break;
			
		case 't':
			time_offsets_are_zero = 1;
			break;
			
		case 'u':
			send_program_change_at_reconnect = 1;
			break;

		case 'c':
			initial_channel = atoi(optarg);

			if (initial_channel < CHANNEL_MIN || initial_channel > CHANNEL_MAX) {
				g_critical("Invalid MIDI channel number specified on the command line; "
					"valid values are %d-%d.", CHANNEL_MIN, CHANNEL_MAX);

				exit(EX_USAGE);
			}

			break;

		case 'b':
			initial_bank = atoi(optarg);

			send_program_change_once = 1;

			if (initial_bank < BANK_MIN || initial_bank > BANK_MAX) {
				g_critical("Invalid MIDI bank number specified on the command line; "
					"valid values are %d-%d.", BANK_MIN, BANK_MAX);

				exit(EX_USAGE);
			}

			break;

		case 'p':
			initial_program = atoi(optarg);

			send_program_change_once = 1;

			if (initial_program < PROGRAM_MIN || initial_program > PROGRAM_MAX) {
				g_critical("Invalid MIDI program number specified on the command line; "
					"valid values are %d-%d.", PROGRAM_MIN, PROGRAM_MAX);

				exit(EX_USAGE);
			}

			break;

		case 'r':
			rate_limit = strtod(optarg, NULL);
			if (rate_limit <= 0.0) {
				g_critical("Invalid rate limit specified.\n");

				exit(EX_USAGE);
			}

			break;

		case 'f':
			full_midi_keyboard = 1;
			break;

		case '?':
		default:
			usage();
		}
	}

	argc -= optind;
	argv += optind;

	init_gtk_2();

	if (full_midi_keyboard)
		piano_keyboard_enable_all_midi_notes(keyboard);

	if (keyboard_layout != NULL) {
		int ret = piano_keyboard_set_keyboard_layout(keyboard, keyboard_layout);

		if (ret) {
			g_critical("Invalid layout, proper choices are QWERTY, QWERTY_REV, QWERTY_UK, QWERTY_UK_REV, QWERTZ, AZERTY and DVORAK.");
			exit(EX_USAGE);
		}
	}

#ifdef HAVE_LASH
	init_lash(lash_args);
#endif

	init_jack();

	if (autoconnect_port_name) {
		if (connect_to_input_port(autoconnect_port_name)) {
			g_critical("Couldn't connect to '%s', exiting.", autoconnect_port_name);
			exit(EX_UNAVAILABLE);
		}
	}

	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(grab_keyboard_checkbutton), grab_keyboard_at_startup);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(channel_spin), initial_channel);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(bank_spin), initial_bank);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(program_spin), initial_program);
	piano_keyboard_set_keyboard_cue(keyboard, enable_keyboard_cue);

	gtk_main();

	return (0);
}


