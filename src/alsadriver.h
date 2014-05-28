//ssj71
#ifndef ALSADRIVER_H
#define ALSADRIVER_H
#include <alsa/asoundlib.h>
#include <alsa/seq.h>

typedef struct aseq{
    snd_seq_t *g_seq;
    int g_port;
}ALSA_SEQ;

int init_alsa(ALSA_SEQ* seq, unsigned char verbose);
void noteup_alsa(void* seqq, unsigned char chan, unsigned char note, unsigned char vel);
void notedown_alsa(void* seqq, unsigned char chan, unsigned char note, unsigned char vel);

#endif
