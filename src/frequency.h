#ifndef _FREQUENCY_H_
#define _FREQUENCY_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define CIRCULAR_BUFFER_LEN 128

typedef struct circbuf_t {
    size_t  write;
    int32_t buf[CIRCULAR_BUFFER_LEN];
} circbuf_t;

extern volatile circbuf_t circular_buffer;

void    circbuf_add(volatile circbuf_t* circbuf, int32_t val);
int32_t circbuf_sum(volatile circbuf_t* circbuf);

void    frequency_start();
int32_t frequency_get();
int32_t frequency_get_error();
void    frequency_allow_adjustment(bool allow);

// Returns ppb * 100
int32_t frequency_get_ppb();
#endif
