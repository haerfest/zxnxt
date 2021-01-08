#include "defs.h"
#include "log.h"


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


#define N_REGISTERS  (E_AY_REGISTER_ENVELOPE_IO_PORT_B_DATA_STORE - E_AY_REGISTER_CHANNEL_A_TONE_PERIOD_FINE + 1)
#define A            0
#define B            1
#define C            2
#define N_CHANNELS   3


typedef struct {
  int   is_tone_enabled;
  u16_t tone_period;
  int   is_noise_enabled;
  int   is_amplitude_fixed;
  u8_t  fixed_amplitude;
} ay_channel_t;


typedef struct {
  u8_t          register_values[N_REGISTERS];
  ay_register_t selected_register;
  ay_channel_t  channels[N_CHANNELS];
  u8_t          noise_period;
  u16_t         envelope_period;
  int           do_envelope_hold;
  int           do_envelope_alternate;
  int           do_envelope_attack;
  int           do_envelope_continue;
} self_t;


static self_t self;


int ay_init(void) {
  self.selected_register = E_AY_REGISTER_ENABLE;
  return 0;
}


void ay_finit(void) {
}


void ay_register_select(u8_t value) {
  self.selected_register = value;
}


u8_t ay_register_read(void) {
  if (self.selected_register < sizeof(self.register_values)) {
    return self.register_values[self.selected_register];
  }

  log_wrn("ay: unimplemented read from REGISTER port\n");
  return 0xFF;
}


void ay_register_write(u8_t value) {
  switch (self.selected_register) {
    case E_AY_REGISTER_CHANNEL_A_TONE_PERIOD_FINE:
      self.channels[A].tone_period = (self.channels[A].tone_period & 0xF0) | value;
      break;

    case E_AY_REGISTER_CHANNEL_A_TONE_PERIOD_COARSE:
      self.channels[A].tone_period = (self.channels[A].tone_period & 0x0F) | ((value & 0x0F) << 8);
      break;

    case E_AY_REGISTER_CHANNEL_B_TONE_PERIOD_FINE:
      self.channels[B].tone_period = (self.channels[B].tone_period & 0xF0) | value;
      break;

    case E_AY_REGISTER_CHANNEL_B_TONE_PERIOD_COARSE:
      self.channels[B].tone_period = (self.channels[B].tone_period & 0x0F) | ((value & 0x0F) << 8);
      break;

    case E_AY_REGISTER_CHANNEL_C_TONE_PERIOD_FINE:
      self.channels[C].tone_period = (self.channels[C].tone_period & 0xF0) | value;
      break;

    case E_AY_REGISTER_CHANNEL_C_TONE_PERIOD_COARSE:
      self.channels[C].tone_period = (self.channels[C].tone_period & 0x0F) | ((value & 0x0F) << 8);
      break;

    case E_AY_REGISTER_NOISE_PERIOD:
      self.noise_period = value & 0x1F;
      break;
      
    case E_AY_REGISTER_ENABLE:
      self.channels[A].is_tone_enabled  = !(value & 0x01);
      self.channels[B].is_tone_enabled  = !(value & 0x02);
      self.channels[C].is_tone_enabled  = !(value & 0x04);
      self.channels[A].is_noise_enabled = !(value & 0x08);
      self.channels[B].is_noise_enabled = !(value & 0x10);
      self.channels[C].is_noise_enabled = !(value & 0x20);
      break;

    case E_AY_REGISTER_CHANNEL_A_AMPLITUDE:
      self.channels[A].is_amplitude_fixed = value & 0x10;
      self.channels[A].fixed_amplitude    = value & 0x0F;
      break;

    case E_AY_REGISTER_CHANNEL_B_AMPLITUDE:
      self.channels[B].is_amplitude_fixed = value & 0x10;
      self.channels[B].fixed_amplitude    = value & 0x0F;
      break;

    case E_AY_REGISTER_CHANNEL_C_AMPLITUDE:
      self.channels[C].is_amplitude_fixed = value & 0x10;
      self.channels[C].fixed_amplitude    = value & 0x0F;
      break;

    case E_AY_REGISTER_ENVELOPE_PERIOD_FINE:
      self.envelope_period = (self.envelope_period & 0xF0) | value;
      break;
      
    case E_AY_REGISTER_ENVELOPE_PERIOD_COARSE:
      self.envelope_period = (self.envelope_period & 0x0F) | (value << 8);
      break;

    case E_AY_REGISTER_ENVELOPE_SHAPE_CYCLE:
      self.do_envelope_hold      = value & 0x01;
      self.do_envelope_alternate = value & 0x02;
      self.do_envelope_attack    = value & 0x04;
      self.do_envelope_continue  = value & 0x08;
      break;

    case E_AY_REGISTER_ENVELOPE_IO_PORT_A_DATA_STORE:
      break;

    case E_AY_REGISTER_ENVELOPE_IO_PORT_B_DATA_STORE:
      break;

    default:
      log_wrn("ay: write of $%02X to unknown register $%02X\n", value, self.selected_register);
      return;
  }

  /* Latch last written value so we can be easily read back. */
  self.register_values[self.selected_register] = value;
}


void ay_run(u32_t ticks) {
  
}
