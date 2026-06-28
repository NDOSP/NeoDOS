#ifndef LAPIC_H
#define LAPIC_H

#include <stdint.h>

void lapic_init(void);
void lapic_write(uint32_t reg, uint32_t value);
uint32_t lapic_read(uint32_t reg);
void lapic_sendIPI(uint8_t apicId, uint32_t icrLow);

#define LAPIC_ID_REG   0x020
#define LAPIC_ICR_LO   0x300
#define LAPIC_ICR_HI   0x310

#define ICR_DELIVERY_INIT  0x500
#define ICR_DELIVERY_SIPI  0x600
#define ICR_ASSERT         0x4000
#define ICR_DEST_ALL_EXC   0xC0000

#endif // LAPIC_H