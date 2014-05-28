#ifndef JACKDRIVER_H
#define JACKDRIVER_H


typedef struct _jseq{
    jack_ringbuffer_t *ringbuffer;
    jack_client_t	*jack_client;
    jack_port_t	*output_port;
}JACK_SEQ;

int init_jack(JACK_SEQ* seq, unsigned char verbose);
void noteup_jack(void* seqq, unsigned char chan, unsigned char note, unsigned char vel);
void notedown_jack(void* seqq, unsigned char chan, unsigned char note, unsigned char vel);

#endif
