#include "audio.h"
#include "defs.h"
#include "log.h"

#define LEVEL(voltage)  ((s8_t) (AUDIO_MAX_VOLUME * voltage))
#define MAX(a, b)       ((a) > (b) ? (a) : (b))

#define N_AMPLITUDES    16
#define N_REGISTERS     (E_AY_REGISTER_ENVELOPE_IO_PORT_B_DATA_STORE - E_AY_REGISTER_CHANNEL_A_TONE_PERIOD_FINE + 1)
#define A               0
#define B               1
#define C               2
#define N_CHANNELS      3


typedef enum {
  E_AY_REGISTER_CHANNEL_A_TONE_PERIOD_FINE = 0,
  E_AY_REGISTER_CHANNEL_A_TONE_PERIOD_COARSE,
  E_AY_REGISTER_CHANNEL_B_TONE_PERIOD_FINE,
  E_AY_REGISTER_CHANNEL_B_TONE_PERIOD_COARSE,
  E_AY_REGISTER_CHANNEL_C_TONE_PERIOD_FINE,
  E_AY_REGISTER_CHANNEL_C_TONE_PERIOD_COARSE,
  E_AY_REGISTER_NOISE_PERIOD,
  E_AY_REGISTER_ENABLE,
  E_AY_REGISTER_CHANNEL_A_AMPLITUDE,
  E_AY_REGISTER_CHANNEL_B_AMPLITUDE,
  E_AY_REGISTER_CHANNEL_C_AMPLITUDE,
  E_AY_REGISTER_ENVELOPE_PERIOD_FINE,
  E_AY_REGISTER_ENVELOPE_PERIOD_COARSE,
  E_AY_REGISTER_ENVELOPE_SHAPE_CYCLE,
  E_AY_REGISTER_ENVELOPE_IO_PORT_A_DATA_STORE,
  E_AY_REGISTER_ENVELOPE_IO_PORT_B_DATA_STORE
} ay_register_t;


const s8_t ay_volume[N_AMPLITUDES] = {
  LEVEL(0.00000),  /*  0 */
  LEVEL(0.00782),  /*  1 */
  LEVEL(0.00859),  /*  2 */
  LEVEL(0.01563),  /*  3 */
  LEVEL(0.01717),  /*  4 */
  LEVEL(0.03125),  /*  5 */
  LEVEL(0.03535),  /*  6 */
  LEVEL(0.06250),  /*  7 */
  LEVEL(0.07070),  /*  8 */
  LEVEL(0.12500),  /*  9 */
  LEVEL(0.15150),  /* 10 */
  LEVEL(0.25000),  /* 11 */
  LEVEL(0.30303),  /* 12 */
  LEVEL(0.50000),  /* 13 */
  LEVEL(0.70707),  /* 14 */
  LEVEL(1.00000)   /* 15 */
};


typedef struct {
  /* Latched state. */
  int   is_tone_enabled;
  u16_t tone_period_counter;
  int   is_noise_enabled;
  int   is_amplitude_fixed;
  u8_t  fixed_amplitude;

  /* Working state. */
  u16_t tone_divider;
  u16_t tone_divider_halfway_point;
  u8_t  amplitude;
  int   phase;
  s8_t  sample_last;
} ay_channel_t;


typedef struct {
  u8_t          registers[N_REGISTERS];
  ay_register_t selected_register;
  ay_channel_t  channels[N_CHANNELS];
  u8_t          noise_period;
  u16_t         envelope_period;
  int           do_envelope_hold;
  int           do_envelope_alternate;
  int           do_envelope_attack;
  int           do_envelope_continue;
  int           ticks_div_16;
} self_t;


static self_t self;


int ay_init(void) {
  int i;

  for (i = A; i <= C; i++) {
    self.channels[i].amplitude    = 0;
    self.channels[i].phase        = 1;
    self.channels[i].sample_last  = 0;
    self.channels[i].tone_divider = 0;
  }

  /* All AY channels are off by default. */
  self.registers[E_AY_REGISTER_ENABLE] = 0xFF;

  self.selected_register = E_AY_REGISTER_ENABLE;
  self.ticks_div_16      = 0;
  
  return 0;
}


void ay_finit(void) {
}


void ay_register_select(u8_t value) {
  self.selected_register = value;
}


u8_t ay_register_read(void) {
  if (self.selected_register < sizeof(self.registers)) {
    return self.registers[self.selected_register];
  }

  log_wrn("ay: unimplemented read from REGISTER port\n");
  return 0xFF;
}


void ay_register_write(u8_t value) {
  if (self.selected_register >= sizeof(self.registers)) {
    log_wrn("ay: write of $%02X to unknown register $%02X\n", value, self.selected_register);
    return;
  }

  self.registers[self.selected_register] = value;

  switch (self.selected_register) {
    case E_AY_REGISTER_CHANNEL_A_TONE_PERIOD_COARSE:
      self.channels[A].tone_period_counter = self.registers[E_AY_REGISTER_CHANNEL_A_TONE_PERIOD_COARSE] << 8 | self.registers[E_AY_REGISTER_CHANNEL_A_TONE_PERIOD_FINE];
      break;

    case E_AY_REGISTER_CHANNEL_B_TONE_PERIOD_COARSE:
      self.channels[B].tone_period_counter = self.registers[E_AY_REGISTER_CHANNEL_B_TONE_PERIOD_COARSE] << 8 | self.registers[E_AY_REGISTER_CHANNEL_B_TONE_PERIOD_FINE];
      break;

    case E_AY_REGISTER_CHANNEL_C_TONE_PERIOD_COARSE:
      self.channels[C].tone_period_counter = self.registers[E_AY_REGISTER_CHANNEL_C_TONE_PERIOD_COARSE] << 8 | self.registers[E_AY_REGISTER_CHANNEL_C_TONE_PERIOD_FINE];
      break;

    default:
      break;
  }
}


static void ay_channel_step(int n) {
  ay_channel_t* channel = &self.channels[n];
  s8_t          sample;

  if (channel->tone_divider == 0) {
    channel->tone_divider = channel->tone_period_counter;
    if (channel->tone_divider == 0) {
      channel->tone_divider = 1;
    }
    channel->tone_divider_halfway_point = channel->tone_divider / 2;
    channel->phase                      = 1;
  } else if (channel->tone_divider == channel->tone_divider_halfway_point) {
    channel->phase                      = -1;
  }

  if (self.registers[E_AY_REGISTER_ENABLE] & (1 << n)) {
    sample = 0;
  } else {
    /* TODO Assuming fixed amplitude for now. */
    sample = ay_volume[self.registers[E_AY_REGISTER_CHANNEL_A_AMPLITUDE + n] & 0x0F];
    if (channel->phase < 0) {
      sample = -sample;
    }
  }

  if (sample != channel->sample_last) {
    audio_add_sample(E_AUDIO_SOURCE_AY_1_CHANNEL_A + n, sample);
    channel->sample_last = sample;
  }

  channel->tone_divider--;
}


void ay_run(u32_t ticks) {
  u32_t tick;

  for (tick = 0; tick < ticks; tick++) {
    if (++self.ticks_div_16 == 16) {
      self.ticks_div_16 = 0;

      ay_channel_step(A);
      ay_channel_step(B);
      ay_channel_step(C);
    }
  }
}
