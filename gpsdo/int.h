#ifndef _INT_H_
#define _INT_H_

#include <stdbool.h>
#include <stdint.h>

extern volatile bool     allow_adjustment;
extern volatile uint32_t frequency;
extern volatile uint32_t num_samples;
extern volatile uint32_t device_uptime;

#endif
