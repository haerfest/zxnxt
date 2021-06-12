#ifndef __COPPER_H
#define __COPPER_H


int  copper_init(void);
void copper_finit(void);
void copper_data_8bit_write(u8_t value);
void copper_data_16bit_write(u8_t value);
void copper_address_write(u8_t value);
void copper_control_write(u8_t value);


#endif  /* __COPPER_H */
