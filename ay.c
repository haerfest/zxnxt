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
  u16_t tone_period;
  int   is_amplitude_fixed;
  u8_t  amplitude;
} channel_latched_t;


typedef struct {
  channel_latched_t latched;
  u16_t             tone_counter;
  u16_t             tone_period_half;
  int               tone_value;
  s8_t              sample_last;
  u8_t              amplitude;
} ay_channel_t;


typedef struct {
  u8_t  noise_period;
  u16_t envelope_period;
  u8_t  envelope_shape;
} general_latched_t;


typedef struct {
  u8_t              registers[N_REGISTERS];
  ay_register_t     selected_register;
  ay_channel_t      channels[N_CHANNELS];
  general_latched_t latched;
  u32_t             noise_seed;
  int               noise_value;
  u8_t              noise_counter;
  u8_t              noise_period_half;
  int               ticks_div_16;
  int               ticks_div_256;
  u16_t             envelope_counter;
  u16_t             envelope_period_sixteenth;
  u8_t              envelope_counter_sixteenth;
  u8_t              envelope_amplitude;
  s8_t              envelope_delta;
} self_t;


static self_t self;


int ay_init(void) {
  ay_reset();
  return 0;
}


void ay_finit(void) {
}


void ay_reset(void) {
  memset(&self, 0, sizeof(self));

  /* All AY channels are off by default. */
  self.registers[E_AY_REGISTER_ENABLE] = 0xFF;
  self.noise_seed                      = 0xFFFF;
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


static void ay_envelope_reset(void) {
  self.envelope_counter           = MAX(self.latched.envelope_period, 1);
  self.envelope_period_sixteenth  = self.envelope_counter / 16;
  self.envelope_counter_sixteenth = self.envelope_period_sixteenth;

  /* Attack solely determines starting shape of envelope. */
  if (self.latched.envelope_shape & 0x04) {
    self.envelope_amplitude = 0;
    self.envelope_delta     = 1;
  } else {
    self.envelope_amplitude = 15;
    self.envelope_delta     = -1;
  }    
}


static void ay_envelope_step(void) {
  if (self.envelope_counter == 0) {
    /* One cycle complete. */
    self.envelope_counter           = MAX(self.latched.envelope_period, 1);
    self.envelope_period_sixteenth  = self.envelope_counter / 16;
    self.envelope_counter_sixteenth = self.envelope_period_sixteenth;

    /* See Fig. 7 ENVELOPE SHAPE/CYCLE CONTROL */
    switch (self.latched.envelope_shape & 0x0F) {
      case 0x00:  /* 0 0 x x */
      case 0x01:
      case 0x02:
      case 0x03:
      case 0x04:  /* 0 1 x x */
      case 0x05:
      case 0x06:
      case 0x07:
      case 0x09:  /* 1 0 0 1 */
      case 0x0F:  /* 1 1 1 1 */
        self.envelope_amplitude = 0;
        self.envelope_delta     = 0;
        break;

      case 0x08:  /* 1 0 0 0 */
        self.envelope_amplitude = 15;
        self.envelope_delta     = -1;
        break;

      case 0x0A:  /* 1 0 1 0 */
      case 0x0E:  /* 1 1 1 0 */
        self.envelope_delta = -self.envelope_delta;
        break;

      case 0x0B:  /* 1 0 1 1 */
      case 0x0D:  /* 1 1 0 1 */
        self.envelope_amplitude = 15;
        self.envelope_delta     = 0;
        break;

      case 0x0C:  /* 1 1 0 0 */
        self.envelope_amplitude = 0;
        self.envelope_delta     = 1;
        break;
    }
  } else if (self.envelope_counter_sixteenth == 0)  {
    /* One step within cycle complete. */
    if ((self.envelope_delta < 0 && self.envelope_amplitude > 0) ||
        (self.envelope_delta > 0 && self.envelope_amplitude < 15)) {
      self.envelope_amplitude += self.envelope_delta;
    }
    self.envelope_counter_sixteenth = self.envelope_period_sixteenth;
  }

  self.envelope_counter_sixteenth--;
  self.envelope_counter--;
}


static void ay_noise_step(void) {
  int advance = 0;

  if (self.noise_counter == 0) {
    self.noise_counter     = MAX(self.latched.noise_period, 1);
    self.noise_period_half = self.noise_counter / 2;
    advance                = 1;
  } else if (self.noise_counter == self.noise_period_half) {
    advance                = 1;
  }

  if (advance) {
    /* GenNoise (c) Hacker KAY & Sergey Bulba */
    self.noise_seed  = (self.noise_seed * 2 + 1) ^ (((self.noise_seed >> 16) ^ (self.noise_seed >> 13)) & 1);
    self.noise_value = (self.noise_seed >> 16) & 1;
  }

  self.noise_counter--;
}


static void ay_channel_step(int n) {
  ay_channel_t* channel = &self.channels[n];

  if (channel->tone_counter == 0) {
    channel->tone_counter     = MAX(channel->latched.tone_period, 1);
    channel->tone_period_half = channel->tone_counter / 2;
    channel->tone_value       = 1;
  } else if (channel->tone_counter == channel->tone_period_half) {
    channel->tone_value       = 0;
  }

  channel->tone_counter--;
}


static void ay_mix(int n) {
  ay_channel_t* channel = &self.channels[n];
  s8_t          sample  = 0;

  /* Tone or noise enabled on this channel? */
  if ((channel->latched.is_tone_enabled  && channel->tone_value) ||
      (channel->latched.is_noise_enabled && self.noise_value)) {
    const u8_t amplitude = channel->latched.is_amplitude_fixed ? channel->latched.amplitude : self.envelope_amplitude;
    sample = ay_volume[amplitude];
  }

  if (sample != channel->sample_last) {
    channel->sample_last = sample;
    audio_add_sample(E_AUDIO_SOURCE_AY_1_CHANNEL_A + n, sample);
  }
}


void ay_run(u32_t ticks) {
  u32_t tick;

  for (tick = 0; tick < ticks; tick++) {
    if (++self.ticks_div_256 == 256) {
      self.ticks_div_256 = 0;

      ay_envelope_step();
    }

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


void ay_register_write(u8_t value) {
  if (self.selected_register >= sizeof(self.registers)) {
    log_wrn("ay: write of $%02X to unknown register $%02X\n", value, self.selected_register);
    return;
  }

  self.registers[self.selected_register] = value;

  switch (self.selected_register) {
    case E_AY_REGISTER_CHANNEL_A_TONE_PERIOD_FINE:
    case E_AY_REGISTER_CHANNEL_A_TONE_PERIOD_COARSE:
      self.channels[A].latched.tone_period = (self.registers[E_AY_REGISTER_CHANNEL_A_TONE_PERIOD_COARSE] & 0x0F) << 8 | self.registers[E_AY_REGISTER_CHANNEL_A_TONE_PERIOD_FINE];
      break;

    case E_AY_REGISTER_CHANNEL_B_TONE_PERIOD_FINE:
    case E_AY_REGISTER_CHANNEL_B_TONE_PERIOD_COARSE:
      self.channels[B].latched.tone_period = (self.registers[E_AY_REGISTER_CHANNEL_B_TONE_PERIOD_COARSE] & 0x0F) << 8 | self.registers[E_AY_REGISTER_CHANNEL_B_TONE_PERIOD_FINE];
      break;

    case E_AY_REGISTER_CHANNEL_C_TONE_PERIOD_FINE:
    case E_AY_REGISTER_CHANNEL_C_TONE_PERIOD_COARSE:
      self.channels[C].latched.tone_period = (self.registers[E_AY_REGISTER_CHANNEL_C_TONE_PERIOD_COARSE] & 0x0F) << 8 | self.registers[E_AY_REGISTER_CHANNEL_C_TONE_PERIOD_FINE];
      break;

    case E_AY_REGISTER_NOISE_PERIOD:
      self.latched.noise_period = value & 0x1F;
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

    case E_AY_REGISTER_ENVELOPE_PERIOD_FINE:
    case E_AY_REGISTER_ENVELOPE_PERIOD_COARSE:
      self.latched.envelope_period = self.registers[E_AY_REGISTER_ENVELOPE_PERIOD_COARSE] << 8 | self.registers[E_AY_REGISTER_ENVELOPE_PERIOD_FINE];
      break;

    case E_AY_REGISTER_ENVELOPE_SHAPE_CYCLE:
      self.latched.envelope_shape = value;
      ay_envelope_reset();
      break;

    default:
      break;
  }
}
