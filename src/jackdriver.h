#ifndef JACKDRIVER_H
#define JACKDRIVER_H
#include<jack/jack.h>
#include<jack/ringbuffer.h>

typedef struct _jseq{
    jack_ringbuffer_t *ringbuffer;
    jack_client_t	*jack_client;
    jack_port_t	*output_port;
}JACK_SEQ;

int init_jack_client(JACK_SEQ* seq, unsigned char verbose, const char* name);
void close_jack(JACK_SEQ* seq);
void noteup_jack(void* seqq, unsigned char chan, unsigned char note, unsigned char vel);
void notedown_jack(void* seqq, unsigned char chan, unsigned char note, unsigned char vel);
void pitch_jack(void* seqq, unsigned char chan, short val);
void control_jack(void* seqq, unsigned char chan, unsigned char indx, unsigned char val);
void prog_jack(void* seqq, unsigned char chan, unsigned char indx);

#endif
