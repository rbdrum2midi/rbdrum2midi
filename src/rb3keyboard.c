// janifr, jasperro
// based on rbkit.c by ssj71
// rbkit.c
#include "rb3keyboard.h"
//#include "constants.h"

void init_rb3_keyboard(MIDIDRUM *MIDI_DRUM) {
  switch (MIDI_DRUM->kit) {
  case WII_RB3_KEYBOARD:
    MIDI_DRUM->buf_indx[ONE] = 0;
    MIDI_DRUM->buf_mask[ONE] = 0x04;
    MIDI_DRUM->buf_indx[B_BUTTON] = 0;
    MIDI_DRUM->buf_mask[B_BUTTON] = 0x04;

    MIDI_DRUM->buf_indx[UP] = 2;
    MIDI_DRUM->buf_mask[UP] = 0x08;
    MIDI_DRUM->buf_indx[DOWN] = 2;
    MIDI_DRUM->buf_mask[DOWN] = 0x04;
    MIDI_DRUM->buf_indx[LEFT] = 2;
    MIDI_DRUM->buf_mask[LEFT] = 0x01;
    MIDI_DRUM->buf_indx[RIGHT] = 2;
    MIDI_DRUM->buf_mask[RIGHT] = 0x02;

    MIDI_DRUM->buf_indx[MINUS] = 1;
    MIDI_DRUM->buf_mask[MINUS] = 0x01;
    MIDI_DRUM->buf_indx[PLUS] = 1;
    MIDI_DRUM->buf_mask[PLUS] = 0x02;

    MIDI_DRUM->buf_indx[A_BUTTON] = 0;
    MIDI_DRUM->buf_mask[A_BUTTON] = 0x02;
    MIDI_DRUM->buf_indx[B_BUTTON] = 0;
    MIDI_DRUM->buf_mask[B_BUTTON] = 0x04;
    MIDI_DRUM->buf_indx[ONE] = 0;
    MIDI_DRUM->buf_mask[ONE] = 0x01;
    MIDI_DRUM->buf_indx[TWO] = 0;
    MIDI_DRUM->buf_mask[TWO] = 0x08;

    MIDI_DRUM->buf_indx[CONNECT] = 1;
    MIDI_DRUM->buf_mask[CONNECT] = 0x10;

    MIDI_DRUM->buf_indx[KEYS0] = 5;
    MIDI_DRUM->buf_indx[KEYS1] = 6;
    MIDI_DRUM->buf_indx[KEYS2] = 7;
    MIDI_DRUM->buf_indx[KEYS3] = 8;

    MIDI_DRUM->buf_indx[EXPRESSION] = 15;
    MIDI_DRUM->buf_mask[EXPRESSION] = 0x7f;
    MIDI_DRUM->buf_indx[EXPR_TOGGLE] = 13;
    MIDI_DRUM->buf_mask[EXPR_TOGGLE] = 0x80;
    break;
  case XB_RB3_KEYBOARD:
    MIDI_DRUM->buf_indx[UP] = 7;
    MIDI_DRUM->buf_mask[UP] = 0x01;
    MIDI_DRUM->buf_indx[DOWN] = 7;
    MIDI_DRUM->buf_mask[DOWN] = 0x02;
    MIDI_DRUM->buf_indx[LEFT] = 7;
    MIDI_DRUM->buf_mask[LEFT] = 0x04;
    MIDI_DRUM->buf_indx[RIGHT] = 7;
    MIDI_DRUM->buf_mask[RIGHT] = 0x08;

    MIDI_DRUM->buf_indx[MINUS] = 7;
    MIDI_DRUM->buf_mask[MINUS] = 0x40;
    MIDI_DRUM->buf_indx[PLUS] = 7;
    MIDI_DRUM->buf_mask[PLUS] = 0x80;

    MIDI_DRUM->buf_indx[A_BUTTON] = 7;
    MIDI_DRUM->buf_mask[A_BUTTON] = 0x10;
    MIDI_DRUM->buf_indx[B_BUTTON] = 7;
    MIDI_DRUM->buf_mask[B_BUTTON] = 0x20;
    MIDI_DRUM->buf_indx[ONE] = 0;
    MIDI_DRUM->buf_mask[ONE] = 0x01;
    MIDI_DRUM->buf_indx[TWO] = 0;
    MIDI_DRUM->buf_mask[TWO] = 0x08;

    MIDI_DRUM->buf_indx[CONNECT] = 1;
    MIDI_DRUM->buf_mask[CONNECT] = 0x10;

    MIDI_DRUM->buf_indx[KEYS0] = 8;
    MIDI_DRUM->buf_indx[KEYS1] = 9;
    MIDI_DRUM->buf_indx[KEYS2] = 10;
    MIDI_DRUM->buf_indx[KEYS3] = 11;

    MIDI_DRUM->buf_indx[EXPRESSION] = 15;
    MIDI_DRUM->buf_mask[EXPRESSION] = 0x7f;
    MIDI_DRUM->buf_indx[EXPR_TOGGLE] = 13;
    MIDI_DRUM->buf_mask[EXPR_TOGGLE] = 0x80;

    MIDI_DRUM->buf_indx[NOTHING] = 17;
    MIDI_DRUM->buf_mask[NOTHING] = 0x7f;
    break;
  }
  MIDI_DRUM->key_state = 0;
  MIDI_DRUM->prev_keystate = 0;
  MIDI_DRUM->velocity = 0;
  MIDI_DRUM->channel = 0;
}

static inline void get_velocity(MIDIDRUM *MIDI_DRUM) {
  // Velocity data is transmitted for up to 5 keys pressed same time.
  // This is the reason for the weird velocity reading algorithm.
  // For this reason the 6th key wont get any reasonable velocity data.
  if (MIDI_DRUM->buf[12] != 0)
    MIDI_DRUM->velocity = MIDI_DRUM->buf[12];
  else if (MIDI_DRUM->buf[11] != 0)
    MIDI_DRUM->velocity = MIDI_DRUM->buf[11];
  else if (MIDI_DRUM->buf[10] != 0)
    MIDI_DRUM->velocity = MIDI_DRUM->buf[10];
  else if (MIDI_DRUM->buf[9] != 0)
    MIDI_DRUM->velocity = MIDI_DRUM->buf[9];
  else
    MIDI_DRUM->velocity = MIDI_DRUM->buf[8] & 0x7f;
}

static inline void handle_key(MIDIDRUM *MIDI_DRUM, unsigned char key) {
  // printf("%d\n",1<<(24-key));
  if ((MIDI_DRUM->key_state & (1 << (24 - key))) !=
      (MIDI_DRUM->prev_keystate & (1 << (24 - key)))) {
    get_velocity(MIDI_DRUM);
    printf("key %d\n", key);
    if (MIDI_DRUM->key_state & (1 << (24 - key))) {
      MIDI_DRUM->notedown(MIDI_DRUM->sequencer, MIDI_DRUM->channel,
                          48 + key + 12 * MIDI_DRUM->octave,
                          MIDI_DRUM->velocity);
    }
  }
  if (!(MIDI_DRUM->key_state & (1 << (24 - key)))) {
    MIDI_DRUM->noteup(MIDI_DRUM->sequencer, MIDI_DRUM->channel,
                      48 + key + 12 * MIDI_DRUM->octave, 0);
  }
}

// callback for rockband3 keyboard
void cb_irq_rb3_keyboard(struct libusb_transfer *transfer) {
  MIDIDRUM *MIDI_DRUM = (MIDIDRUM *)transfer->user_data;
  if (transfer->status != LIBUSB_TRANSFER_COMPLETED) {
    fprintf(stderr, "irq transfer status %d? %d\n", transfer->status,
            LIBUSB_TRANSFER_ERROR);
    do_exit = 2;
    libusb_free_transfer(transfer);
    transfer = NULL;
    return;
  }

  // the keys are the most critical for latency, ctl values can be slower
  MIDI_DRUM->key_state = (MIDI_DRUM->buf[MIDI_DRUM->buf_indx[KEYS0]] << 17) +
                         (MIDI_DRUM->buf[MIDI_DRUM->buf_indx[KEYS1]] << 9) +
                         (MIDI_DRUM->buf[MIDI_DRUM->buf_indx[KEYS2]] << 1) +
                         ((MIDI_DRUM->buf[MIDI_DRUM->buf_indx[KEYS3]]) >> 7);
  get_state(MIDI_DRUM, NOTHING);
  if (MIDI_DRUM->drum_state[NOTHING]) {
    handle_key(MIDI_DRUM, 0);
    handle_key(MIDI_DRUM, 1);
    handle_key(MIDI_DRUM, 2);
    handle_key(MIDI_DRUM, 3);
    handle_key(MIDI_DRUM, 4);
    handle_key(MIDI_DRUM, 5);
    handle_key(MIDI_DRUM, 6);
    handle_key(MIDI_DRUM, 7);
    handle_key(MIDI_DRUM, 8);
    handle_key(MIDI_DRUM, 9);
    handle_key(MIDI_DRUM, 10);
    handle_key(MIDI_DRUM, 11);
    handle_key(MIDI_DRUM, 12);
    handle_key(MIDI_DRUM, 13);
    handle_key(MIDI_DRUM, 14);
    handle_key(MIDI_DRUM, 15);
    handle_key(MIDI_DRUM, 16);
    handle_key(MIDI_DRUM, 17);
    handle_key(MIDI_DRUM, 18);
    handle_key(MIDI_DRUM, 19);
    handle_key(MIDI_DRUM, 20);
    handle_key(MIDI_DRUM, 21);
    handle_key(MIDI_DRUM, 22);
    handle_key(MIDI_DRUM, 23);
    handle_key(MIDI_DRUM, 24);
  }

  // these buttons control octave
  // octave can get up to +-4 octaves
  get_state(MIDI_DRUM, PLUS);
  get_state(MIDI_DRUM, MINUS);
  get_state(MIDI_DRUM, CONNECT);

  // TODO: this leaves opportunity for currently-on notes to be "orphaned"
  // so that the note-off comes for a different octave
  // NOTE: Also the + and - buttons correspond to start and select on other
  // platforms, when using the midi out port the 1 and B buttons change octave
  // for now, I don't mind just doing it this way
  if (MIDI_DRUM->drum_state[MINUS] && !MIDI_DRUM->prev_state[MINUS] &&
      MIDI_DRUM->octave > -4) {
    MIDI_DRUM->octave--; }
  if (MIDI_DRUM->drum_state[PLUS] && !MIDI_DRUM->prev_state[PLUS] &&
      MIDI_DRUM->octave < 4) {
    MIDI_DRUM->octave++; }
  if (MIDI_DRUM->drum_state[CONNECT] && !MIDI_DRUM->prev_state[CONNECT]) {
    MIDI_DRUM->octave = 0; }

  // now the pitchbender
  get_state(MIDI_DRUM, EXPR_TOGGLE);
  get_state(MIDI_DRUM, EXPRESSION);
  if (MIDI_DRUM->drum_state[EXPRESSION] != MIDI_DRUM->prev_state[EXPRESSION]) {
    if (MIDI_DRUM->drum_state[EXPR_TOGGLE]) {
      // should be MOD wheel change
    } else {
      // TODO: figure out how this data is formatted.
      MIDI_DRUM->pitchbend(MIDI_DRUM->sequencer, MIDI_DRUM->channel,
                           MIDI_DRUM->drum_state[EXPRESSION]);
    }
  }

  memcpy(MIDI_DRUM->prev_state, MIDI_DRUM->drum_state, NUM_DRUMS);

  if (MIDI_DRUM->verbose) {
    print_keys(MIDI_DRUM);
    print_buf(MIDI_DRUM);
  }
  MIDI_DRUM->prev_keystate = MIDI_DRUM->key_state;

  if (libusb_submit_transfer(transfer) < 0)
    do_exit = 2;
}
