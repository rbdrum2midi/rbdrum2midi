//ssj71
#include "alsadriver.h"
#include <alsa/asoundlib.h>
#include <alsa/seq.h>

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
    snd_seq_set_client_name(handle, "Game Drumkit Client");
    return handle;
}

// create a new port; return the port id
// port will be writable and accept the write-subscription.
int my_new_port(snd_seq_t *handle)
{
    // |SND_SEQ_PORT_CAP_WRITE||SND_SEQ_PORT_CAP_SUBS_WRITE
    return snd_seq_create_simple_port(handle, "Game Drumkit port 2",
        SND_SEQ_PORT_CAP_READ|SND_SEQ_PORT_CAP_SUBS_READ,
        SND_SEQ_PORT_TYPE_MIDI_GENERIC | SND_SEQ_PORT_TYPE_APPLICATION);
}

// create a queue and return its id
int my_queue(snd_seq_t *handle)
{
    return snd_seq_alloc_named_queue(handle, "Game Drumkit queue");
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
    snd_seq_connect_to(seq, 9, client, port);
}

// From test/playmidi1.c from alsa-lib-1.0.3.

// Direct delivery seems like what I'm doing..
void notedown_alsa(void* seqq, unsigned char chan, unsigned char note, unsigned char vel)
//void notedown_alsa(snd_seq_t *seq, int port, int chan, int pitch, int vol)
{
    ALSA_SEQ* seq = (ALSA_SEQ*)seqq;
    snd_seq_event_t ev;

    snd_seq_ev_clear(&ev);
    snd_seq_ev_set_source(&ev, seq->g_port);
    snd_seq_ev_set_subs(&ev);
    snd_seq_ev_set_direct(&ev);
    //... // set event type, data, so on..
    //set_event_time(&ev, Mf_currtime);
    //snd_seq_ev_schedule_tick(&ev, q, 0, Mf_currtime);

    snd_seq_ev_set_noteon(&ev, chan, note, vel);

    int rc = snd_seq_event_output(seq->g_seq, &ev);
    if (rc < 0) {
        printf("written = %i (%s)\n", rc, snd_strerror(rc));
    }
    snd_seq_drain_output(seq->g_seq);
}

// When the note up, note off.
void noteup_alsa(void* seqq, unsigned char chan, unsigned char note, unsigned char vel)
//void noteup_alsa(snd_seq_t *seq, int port, int chan, int pitch, int vol)
{
    ALSA_SEQ* seq = (ALSA_SEQ*)seqq;
    snd_seq_event_t ev;

    snd_seq_ev_clear(&ev);
    snd_seq_ev_set_source(&ev, seq->g_port);
    snd_seq_ev_set_subs(&ev);
    snd_seq_ev_set_direct(&ev);
    //... // set event type, data, so on..

    snd_seq_ev_set_noteoff(&ev, chan, note, vel);

    snd_seq_event_output(seq->g_seq, &ev);
    snd_seq_drain_output(seq->g_seq);
}

void pitch_alsa(void* seqq, unsigned char chan, short val)
{
    ALSA_SEQ* seq = (ALSA_SEQ*)seqq;
    snd_seq_event_t ev;

    snd_seq_ev_clear(&ev);
    snd_seq_ev_set_source(&ev, seq->g_port);
    snd_seq_ev_set_subs(&ev);
    snd_seq_ev_set_direct(&ev);

    snd_seq_ev_set_pitchbend(&ev, chan, val);

    snd_seq_event_output(seq->g_seq, &ev);
    snd_seq_drain_output(seq->g_seq);
}

int init_alsa(ALSA_SEQ* seq, unsigned char verbose)
{
    if ( verbose) printf("Setting up alsa\n");

    seq->g_seq = open_client();
    if (seq->g_seq == NULL) {
        if ( verbose >= 0) printf("Error: open_client failed.\n");
        return 0;
    }

    int my_client_id = snd_seq_client_id(seq->g_seq);
    seq->g_port = my_new_port(seq->g_seq);
    if ( verbose) printf("client:port = %i:%i\n", my_client_id, seq->g_port);

    program_change(seq->g_seq, seq->g_port, 9, 0);
    int ret = 1;
    notedown_alsa((void*)seq, 9, 57, 55);

    if ( verbose) printf("Returning %i\n", ret);
    return ret;
}
/*
void testAlsa(snd_seq_t *seq, int port)
{
    notedown_alsa(seq, port, 9, 57, 127);
    usleep(1000000);
    noteup_alsa(seq, port, 9, 57, 0);
    usleep(1000000);

}*/

void close_alsa(ALSA_SEQ* seq)
{
    snd_seq_close(seq->g_seq);
}
