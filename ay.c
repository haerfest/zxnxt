#include "audio.h"
#include "ay.h"
#include "defs.h"
#include "log.h"

#define MAX(a, b)       ((a) > (b) ? (a) : (b))
#define LEVEL(voltage)  ((s8_t) (AUDIO_MAX_VOLUME * voltage))

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
  audio_source_t    source;
  u8_t              registers[N_REGISTERS];
  ay_register_t     selected_register;
  ay_channel_t      channels[N_CHANNELS];
  general_latched_t latched;
  u32_t             noise_seed;
  int               noise_value;
  u8_t              noise_counter;
  u16_t             envelope_counter;
  u16_t             envelope_period_sixteenth;
  u16_t             envelope_counter_sixteenth;
  u8_t              envelope_amplitude;
  s8_t              envelope_delta;
  int               envelope_cycle_state;
  int               is_envelope_first_cycle;
  int               is_left_enabled;
  int               is_right_enabled;
} ay_t;


typedef struct {
  u8_t selected_ay;
  ay_t ays[3];
  int  ticks_div_16;
  int  ticks_div_256;
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

  memset(&self, 0, sizeof(self));

  /* All AY channels are off by default. */
  for (i = 0; i < 3; i++) {
    self.ays[i].registers[E_AY_REGISTER_ENABLE] = 0xFF;
    self.ays[i].noise_seed                      = 0xFFFF;
  }

  self.ays[0].source = E_AUDIO_SOURCE_AY_1_CHANNEL_A;
  self.ays[1].source = E_AUDIO_SOURCE_AY_2_CHANNEL_A;
  self.ays[2].source = E_AUDIO_SOURCE_AY_3_CHANNEL_A;
}


void ay_register_select(u8_t value) {
  if ((value & 0x9C) == 0x9C) {
    const u8_t n = 3 - (value & 0x03);
    if (n != 3) {
      self.selected_ay = n;
      self.ays[n].is_left_enabled  = (value & 0x40) >> 6;
      self.ays[n].is_right_enabled = (value & 0x20) >> 5;
      return;
    }
  } else if (value <= E_AY_REGISTER_ENVELOPE_IO_PORT_B_DATA_STORE) {
    self.ays[self.selected_ay].selected_register = value;
    return;
  }
  
  log_err("ay: invalid selected register $%02X\n", value);
}


u8_t ay_register_read(void) {
  const ay_t* ay = &self.ays[self.selected_ay];
  
  return ay->registers[ay->selected_register];
}


static void ay_envelope_step(ay_t* ay) {
  if (--ay->envelope_counter == 0) {
    /* One cycle complete. */
    ay->envelope_counter           = MAX(ay->latched.envelope_period, 1);
    ay->envelope_period_sixteenth  = (ay->envelope_counter + 15) / 16;
    ay->envelope_counter_sixteenth = ay->envelope_period_sixteenth;
    ay->envelope_cycle_state       = 1 - ay->envelope_cycle_state;

    if (ay->is_envelope_first_cycle) {
      ay->is_envelope_first_cycle = 0;

      /* Attack solely determines first cycle. */
      if (ay->latched.envelope_shape & 0x04) {
        ay->envelope_amplitude = 0;
        ay->envelope_delta     = 1;
      } else {
        ay->envelope_amplitude = 15;
        ay->envelope_delta     = -1;
      }
    } else {
      /* See Fig. 7 ENVELOPE SHAPE/CYCLE CONTROL */
      switch (ay->latched.envelope_shape & 0x0F) {
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
          ay->envelope_amplitude = 0;
          ay->envelope_delta     = 0;
          break;

        case 0x08:  /* 1 0 0 0 */
          ay->envelope_amplitude = 15;
          ay->envelope_delta     = -1;
          break;

        case 0x0A:  /* 1 0 1 0 */
          if (ay->envelope_cycle_state) {
            ay->envelope_amplitude = 15;
            ay->envelope_delta     = -1;
          } else {
            ay->envelope_amplitude = 0;
            ay->envelope_delta     = 1;
          }
          break;
          
        case 0x0E:  /* 1 1 1 0 */
          if (ay->envelope_cycle_state) {
            ay->envelope_amplitude = 0;
            ay->envelope_delta     = 1;
          } else {
            ay->envelope_amplitude = 15;
            ay->envelope_delta     = -1;
          }
          break;

        case 0x0B:  /* 1 0 1 1 */
        case 0x0D:  /* 1 1 0 1 */
          ay->envelope_amplitude = 15;
          ay->envelope_delta     = 0;
          break;

        case 0x0C:  /* 1 1 0 0 */
          ay->envelope_amplitude = 0;
          ay->envelope_delta     = 1;
          break;
      }
    }
  } else if (--ay->envelope_counter_sixteenth == 0) {
    /* One step within cycle complete. */
    ay->envelope_amplitude += ay->envelope_delta;
    ay->envelope_counter_sixteenth = ay->envelope_period_sixteenth;
  }
}


static void ay_noise_step(ay_t* ay) {
  if (--ay->noise_counter == 0) {
    ay->noise_counter = MAX(ay->latched.noise_period, 1);

    /* GenNoise (c) Hacker KAY & Sergey Bulba */
    ay->noise_seed  = (ay->noise_seed * 2 + 1) ^ (((ay->noise_seed >> 16) ^ (ay->noise_seed >> 13)) & 1);
    ay->noise_value = (ay->noise_seed >> 16) & 1;
  }
}


static void ay_channel_step(ay_t* ay, int n) {
  ay_channel_t* channel = &ay->channels[n];

  if (--channel->tone_counter == 0) {
    channel->tone_counter     = channel->latched.tone_period + 1;
    channel->tone_period_half = channel->tone_counter / 2;
    channel->tone_value       = 1;
  } else if (channel->tone_counter == channel->tone_period_half) {
    channel->tone_value       = 0;
  }
}


static void ay_mix(ay_t* ay, int n) {
  ay_channel_t* channel = &ay->channels[n];
  s8_t          sample  = 0;

  /* Tone or noise enabled on this channel? */
  if ((channel->latched.is_tone_enabled  && channel->tone_value) ||
      (channel->latched.is_noise_enabled && ay->noise_value)) {
    const u8_t amplitude = channel->latched.is_amplitude_fixed ? channel->latched.amplitude : ay->envelope_amplitude;
    sample = ay_volume[amplitude];
  }

  if (sample != channel->sample_last) {
    channel->sample_last = sample;
    audio_add_sample(ay->source + n, sample);
  }
}


void ay_run(u32_t ticks) {
  u32_t tick;

  for (tick = 0; tick < ticks; tick++) {
    if (++self.ticks_div_256 == 256) {
      self.ticks_div_256 = 0;

      ay_envelope_step(&self.ays[0]);
      ay_envelope_step(&self.ays[1]);
      ay_envelope_step(&self.ays[2]);
    }

    if (++self.ticks_div_16 == 16) {
      self.ticks_div_16 = 0;

      ay_noise_step(&self.ays[0]);
      ay_noise_step(&self.ays[1]);
      ay_noise_step(&self.ays[2]);

      ay_channel_step(&self.ays[0], A);
      ay_channel_step(&self.ays[0], B);
      ay_channel_step(&self.ays[0], C);

      ay_channel_step(&self.ays[1], A);
      ay_channel_step(&self.ays[1], B);
      ay_channel_step(&self.ays[1], C);

      ay_channel_step(&self.ays[2], A);
      ay_channel_step(&self.ays[2], B);
      ay_channel_step(&self.ays[2], C);

      ay_mix(&self.ays[0], A);
      ay_mix(&self.ays[0], B);
      ay_mix(&self.ays[0], C);

      ay_mix(&self.ays[1], A);
      ay_mix(&self.ays[1], B);
      ay_mix(&self.ays[1], C);

      ay_mix(&self.ays[2], A);
      ay_mix(&self.ays[2], B);
      ay_mix(&self.ays[2], C);
    }
  }
}


void ay_register_write(u8_t value) {
  ay_t* ay = &self.ays[self.selected_ay];

  ay->registers[ay->selected_register] = value;

  switch (ay->selected_register) {
    case E_AY_REGISTER_CHANNEL_A_TONE_PERIOD_FINE:
    case E_AY_REGISTER_CHANNEL_A_TONE_PERIOD_COARSE:
      ay->channels[A].latched.tone_period = (ay->registers[E_AY_REGISTER_CHANNEL_A_TONE_PERIOD_COARSE] & 0x0F) << 8 | ay->registers[E_AY_REGISTER_CHANNEL_A_TONE_PERIOD_FINE];
      break;

    case E_AY_REGISTER_CHANNEL_B_TONE_PERIOD_FINE:
    case E_AY_REGISTER_CHANNEL_B_TONE_PERIOD_COARSE:
      ay->channels[B].latched.tone_period = (ay->registers[E_AY_REGISTER_CHANNEL_B_TONE_PERIOD_COARSE] & 0x0F) << 8 | ay->registers[E_AY_REGISTER_CHANNEL_B_TONE_PERIOD_FINE];
      break;

    case E_AY_REGISTER_CHANNEL_C_TONE_PERIOD_FINE:
    case E_AY_REGISTER_CHANNEL_C_TONE_PERIOD_COARSE:
      ay->channels[C].latched.tone_period = (ay->registers[E_AY_REGISTER_CHANNEL_C_TONE_PERIOD_COARSE] & 0x0F) << 8 | ay->registers[E_AY_REGISTER_CHANNEL_C_TONE_PERIOD_FINE];
      break;

    case E_AY_REGISTER_NOISE_PERIOD:
      ay->latched.noise_period = value & 0x1F;
      break;

    case E_AY_REGISTER_ENABLE:
      ay->channels[A].latched.is_tone_enabled  = !(value & 0x01);
      ay->channels[B].latched.is_tone_enabled  = !(value & 0x02);
      ay->channels[C].latched.is_tone_enabled  = !(value & 0x04);
      ay->channels[A].latched.is_noise_enabled = !(value & 0x08);
      ay->channels[B].latched.is_noise_enabled = !(value & 0x10);
      ay->channels[C].latched.is_noise_enabled = !(value & 0x20);
      break;

    case E_AY_REGISTER_CHANNEL_A_AMPLITUDE:
      ay->channels[A].latched.amplitude          = value & 0x0F;
      ay->channels[A].latched.is_amplitude_fixed = !(value & 0x010);
      break;

    case E_AY_REGISTER_CHANNEL_B_AMPLITUDE:
      ay->channels[B].latched.amplitude          = value & 0x0F;
      ay->channels[B].latched.is_amplitude_fixed = !(value & 0x010);
      break;

    case E_AY_REGISTER_CHANNEL_C_AMPLITUDE:
      ay->channels[C].latched.amplitude          = value & 0x0F;
      ay->channels[C].latched.is_amplitude_fixed = !(value & 0x010);
      break;

    case E_AY_REGISTER_ENVELOPE_PERIOD_FINE:
    case E_AY_REGISTER_ENVELOPE_PERIOD_COARSE:
      ay->latched.envelope_period = ay->registers[E_AY_REGISTER_ENVELOPE_PERIOD_COARSE] << 8 | ay->registers[E_AY_REGISTER_ENVELOPE_PERIOD_FINE];
      break;

    case E_AY_REGISTER_ENVELOPE_SHAPE_CYCLE:
      ay->latched.envelope_shape  = value;
      ay->is_envelope_first_cycle = 1;
      break;

    default:
      break;
  }
}
