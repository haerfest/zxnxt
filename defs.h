#ifndef __DEFS_H
#define __DEFS_H


#define FRAME_BUFFER_WIDTH  (32 + 256 + 64)
#define FRAME_BUFFER_HEIGHT 312


#define AUDIO_SAMPLE_RATE    44100
#define AUDIO_BUFFER_LENGTH   2048


typedef enum {
  E_MACHINE_TYPE_CONFIG_MODE = 0,
  E_MACHINE_TYPE_ZX_48K,
  E_MACHINE_TYPE_ZX_128K_PLUS2,
  E_MACHINE_TYPE_ZX_PLUS2A_PLUS2B_PLUS3,
  E_MACHINE_TYPE_PENTAGON
} machine_type_t;


typedef unsigned char      u8_t;
typedef unsigned short     u16_t;
typedef unsigned int       u32_t;
typedef unsigned long long u64_t;

typedef signed char        s8_t;
typedef signed short       s16_t;
typedef signed int         s32_t;
typedef signed long long   s64_t;


#endif  /* __DEFS_H */
