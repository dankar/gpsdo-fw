#include "frequency.h"
#include "LCD.h"
#include "stm32f1xx_hal.h"
#include "stm32f1xx_hal_gpio.h"
#include "stm32f1xx_hal_rcc.h"
#include "tim.h"
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include "int.h"

volatile circbuf_t circular_buffer;

// Quick hack to stop the interrupt from printing at the same time as the main loop.
// This should probably be done with atomic operations, or instead use signalling from the
// interrupt and let the main loop print the PPS indicator.
extern volatile int menu_printing;

// Quick and dirty circular buffer
void circbuf_init(volatile circbuf_t* circbuf)
{
    circbuf->write = 0;
    memset((void*)circbuf->buf, 0, CIRCULAR_BUFFER_LEN * sizeof(int32_t));
}

void circbuf_add(volatile circbuf_t* circbuf, int32_t val)
{
    circbuf->buf[circbuf->write] = val;
    circbuf->write               = (circbuf->write + 1) % CIRCULAR_BUFFER_LEN;
}

int32_t circbuf_sum(volatile circbuf_t* circbuf)
{
    int32_t sum = 0;
    for (size_t i = 0; i < CIRCULAR_BUFFER_LEN; i++) {
        sum += circbuf->buf[i];
    }
    return sum;
}

void frequency_start()
{
    HAL_TIM_Base_Start_IT(&htim1);
    HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_2);
    HAL_TIM_IC_Start_IT(&htim1, TIM_CHANNEL_1);
}

void frequency_allow_adjustment(bool allow) { allow_adjustment = allow; }

int32_t frequency_get() { return frequency; }

int32_t frequency_get_error()
{
    if (!frequency) {
        return 0;
    } else {
        int32_t error = frequency - HAL_RCC_GetHCLKFreq();
        // Filter out obvious glitches, the OCXO should never be this far from the target frequency
        if (error > 2000 || error < -2000) {
            return 0;
        } else {
            return error;
        }
    }
}

int32_t frequency_get_ppb()
{
    if (num_samples == 0) {
        return 0xFFFF;
    }

    // Get ratio of cumulative error / expected number of cycles. Multiply by 1e9 for PPB and by
    // 100 to get additional digits without using floats.
    // This will be a running average over 128 seconds of the error in PPB*100
    return (int64_t)circbuf_sum(&circular_buffer) * 1000000000 * 100 / ((int64_t)HAL_RCC_GetHCLKFreq() * num_samples);
}
