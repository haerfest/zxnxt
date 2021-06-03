#include <time.h>
#include "defs.h"
#include "log.h"


/**
 * Implements a DS1307 I2C Real-Time Clock:
 * https://datasheets.maximintegrated.com/en/ds/DS1307.pdf
 */


#define BCD(x)  (((x) / 10) << 4 | ((x) % 10))


typedef struct {
  u8_t index;
} rtc_t;


static rtc_t self;


int rtc_init(void) {
  self.index = 0;
  return 0;
}


void rtc_finit(void) {
}


u8_t rtc_read(void) {
  time_t     clock;
  struct tm* now;
  u8_t       value;

  if (time(&clock) == (time_t) -1) {
    log_err("rtc: could not retrieve current time\n");
    return 0x00;
  }

  now = localtime(&clock);

  switch (self.index) {
    case 0x00:
      /* Seconds. */
      value = BCD(now->tm_sec);
      break;

    case 0x01:
      /* Minutes. */
      value = BCD(now->tm_min);
      break;

    case 0x02:
      /* Hours. */
      value = BCD(now->tm_hour);
      break;

    case 0x03:
      /* Week day */
      value = BCD(now->tm_wday + 1);
      break;

    case 0x04:
      /* Month day. */
      value = BCD(now->tm_mday);
      break;

    case 0x05:
      /* Month. */
      value = BCD(now->tm_mon + 1);
      break;

    case 0x06:
      /* Year. */
      value = BCD(now->tm_year - 100);
      break;

    default:
      value = 0x00;
      break;
  }

  self.index = (self.index + 1) % 64;

  return value;
}


void rtc_write(u8_t value) {
  self.index = value % 64;
}
