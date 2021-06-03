#ifndef __RTC_H
#define __RTC_H


#include "defs.h"


int  rtc_init(void);
void rtc_finit(void);
u8_t rtc_read(void);
void rtc_write(u8_t value);


#endif  /* __RTC_H */
