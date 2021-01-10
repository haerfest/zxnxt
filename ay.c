#include "audio.h"
#include "ay.h"
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
  int   is_noise_enabled;
  int   is_tone_enabled;
  u16_t tone_period_counter;
  int   is_amplitude_fixed;
  u8_t  amplitude;
} channel_latched_t;


typedef struct {
  u8_t  noise_period;
  u16_t envelope_period;
  int   do_envelope_hold;
  int   do_envelope_alternate;
  int   do_envelope_attack;
  int   do_envelope_continue;
} general_latched_t;


typedef struct {
  channel_latched_t latched;
  u16_t             tone_divider;
  u16_t             tone_divider_halfway_point;
  int               tone_value;
  s8_t              sample_last;
  u8_t              amplitude;
} ay_channel_t;


typedef struct {
  u8_t              registers[N_REGISTERS];
  ay_register_t     selected_register;
  ay_channel_t      channels[N_CHANNELS];
  general_latched_t latched;
  u32_t             noise_seed;
  int               noise_value;
  u8_t              noise_divider;
  u8_t              noise_divider_halfway_point;
  int               ticks_div_16;
} self_t;


static self_t self;


int ay_init(void) {
  ay_reset();
  return 0;
}


void ay_finit(void) {
}


void ay_reset(void) {
  int i;

  for (i = A; i <= C; i++) {
    self.channels[i].latched.is_tone_enabled  = 0;
    self.channels[i].latched.is_noise_enabled = 0;
    self.channels[i].sample_last              = 0;
    self.channels[i].amplitude                = 0;
  }

  /* All AY channels are off by default. */
  self.registers[E_AY_REGISTER_ENABLE] = 0xFF;

  self.selected_register = E_AY_REGISTER_ENABLE;
  self.ticks_div_16      = 0;
  self.noise_seed        = 0xFFFF;
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
    case E_AY_REGISTER_CHANNEL_A_TONE_PERIOD_FINE:
    case E_AY_REGISTER_CHANNEL_A_TONE_PERIOD_COARSE:
      self.channels[A].latched.tone_period_counter = self.registers[E_AY_REGISTER_CHANNEL_A_TONE_PERIOD_COARSE] << 8 | self.registers[E_AY_REGISTER_CHANNEL_A_TONE_PERIOD_FINE];
      if (self.channels[A].latched.tone_period_counter == 0) {
        self.channels[A].latched.tone_period_counter = 1;
      }
      break;

    case E_AY_REGISTER_CHANNEL_B_TONE_PERIOD_FINE:
    case E_AY_REGISTER_CHANNEL_B_TONE_PERIOD_COARSE:
      self.channels[B].latched.tone_period_counter = self.registers[E_AY_REGISTER_CHANNEL_B_TONE_PERIOD_COARSE] << 8 | self.registers[E_AY_REGISTER_CHANNEL_B_TONE_PERIOD_FINE];
      if (self.channels[B].latched.tone_period_counter == 0) {
        self.channels[B].latched.tone_period_counter = 1;
      }
      break;

    case E_AY_REGISTER_CHANNEL_C_TONE_PERIOD_FINE:
    case E_AY_REGISTER_CHANNEL_C_TONE_PERIOD_COARSE:
      self.channels[C].latched.tone_period_counter = self.registers[E_AY_REGISTER_CHANNEL_C_TONE_PERIOD_COARSE] << 8 | self.registers[E_AY_REGISTER_CHANNEL_C_TONE_PERIOD_FINE];
      if (self.channels[C].latched.tone_period_counter == 0) {
        self.channels[C].latched.tone_period_counter = 1;
      }
      break;

    case E_AY_REGISTER_NOISE_PERIOD:
      self.latched.noise_period = value & 0x1F;
      if (self.latched.noise_period == 0) {
        self.latched.noise_period = 1;
      }
      break;

    case E_AY_REGISTER_ENABLE:
      self.channels[A].latched.is_tone_enabled  = !(value & 0x01);
      self.channels[B].latched.is_tone_enabled  = !(value & 0x02);
      self.channels[C].latched.is_tone_enabled  = !(value & 0x04);
      self.channels[A].latched.is_noise_enabled = !(value & 0x08);
      self.channels[B].latched.is_noise_enabled = !(value & 0x10);
      self.channels[C].latched.is_noise_enabled = !(value & 0x20);
      break;

    case E_AY_REGISTER_CHANNEL_A_AMPLITUDE:
      self.channels[A].latched.amplitude          = value & 0x0F;
      self.channels[A].latched.is_amplitude_fixed = !(value & 0x010);
      break;

    case E_AY_REGISTER_CHANNEL_B_AMPLITUDE:
      self.channels[B].latched.amplitude          = value & 0x0F;
      self.channels[B].latched.is_amplitude_fixed = !(value & 0x010);
      break;

    case E_AY_REGISTER_CHANNEL_C_AMPLITUDE:
      self.channels[C].latched.amplitude          = value & 0x0F;
      self.channels[C].latched.is_amplitude_fixed = !(value & 0x010);
      break;

    default:
      break;
  }
}


static void ay_noise_step(void) {
  int advance_noise = 0;

  if (self.noise_divider == 0) {
    self.noise_divider               = self.latched.noise_period;
    self.noise_divider_halfway_point = self.noise_divider / 2;
    advance_noise                    = 1;
  } else if (self.noise_divider == self.noise_divider_halfway_point) {
    advance_noise                    = 1;
  }

  if (advance_noise) {
    /* GenNoise (c) Hacker KAY & Sergey Bulba */
    self.noise_seed  = (self.noise_seed * 2 + 1) ^ (((self.noise_seed >> 16) ^ (self.noise_seed >> 13)) & 1);
    self.noise_value = (self.noise_seed >> 16) & 1;
  }

  self.noise_divider--;
}


static void ay_channel_step(int n) {
  ay_channel_t* channel = &self.channels[n];

  if (channel->tone_divider == 0) {
    channel->tone_divider               = channel->latched.tone_period_counter;
    channel->tone_divider_halfway_point = channel->tone_divider / 2;
    channel->tone_value                 = 1;
  } else if (channel->tone_divider == channel->tone_divider_halfway_point) {
    channel->tone_value                 = 0;
  }

  channel->tone_divider--;
}


/* TODO Assuming fixed amplitude for now. */
static void ay_mix(int n) {
  ay_channel_t* channel = &self.channels[n];
  s8_t          sample  = 0;

  /* Toine or noise enabled on this channel? */
  if ((channel->latched.is_tone_enabled  && channel->tone_value) ||
      (channel->latched.is_noise_enabled && self.noise_value)) {
    sample = ay_volume[channel->latched.amplitude];
  }

  if (sample != channel->sample_last) {
    channel->sample_last = sample;
    audio_add_sample(E_AUDIO_SOURCE_AY_1_CHANNEL_A + n, sample);
  }
}


void ay_run(u32_t ticks) {
  u32_t tick;

  for (tick = 0; tick < ticks; tick++) {
    if (++self.ticks_div_16 == 16) {
      self.ticks_div_16 = 0;

      ay_noise_step();

      ay_channel_step(A);
      ay_channel_step(B);
      ay_channel_step(C);

      ay_mix(A);
      ay_mix(B);
      ay_mix(C);
    }
  }
}
