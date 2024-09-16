#include "frequency.h"
#include "LCD.h"
#include "stm32f1xx_hal.h"
#include "stm32f1xx_hal_rcc.h"
#include "tim.h"
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

volatile uint32_t  timer_overflows  = 0;
volatile uint32_t  capture          = 0;
volatile uint32_t  previous_capture = 0;
volatile uint32_t  frequency        = 0;
volatile uint8_t   first            = 1;
volatile uint32_t  last_pps         = 0;
volatile uint32_t  num_samples      = 0;
volatile circbuf_t circular_buffer;
volatile bool      allow_adjustment = false;

// Quick hack to stop the interrupt from printing at the same time as the main loop.
// This should probably be done with atomic operations, or instead use signalling from the
// interrupt and let the main loop print the PPS indicator.
extern volatile int printing;

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

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef* htim)
{
    if (htim == &htim1) {
        timer_overflows++;
    }
}

// This gets run each time PPS goes high
void HAL_TIM_IC_CaptureCallback(TIM_HandleTypeDef* htim)
{
    static bool pps_indicator = false;

    uint32_t current_tick = HAL_GetTick();
    if (htim->Channel == HAL_TIM_ACTIVE_CHANNEL_1) {

        capture = HAL_TIM_ReadCapturedValue(htim, TIM_CHANNEL_1);

        if(allow_adjustment)
        {
            // Ignore first capture and do a sanity check on elapsed time since previous PPS
            if (!first && current_tick - last_pps < 1300) {
                frequency = capture - previous_capture + (TIM1->ARR + 1) * timer_overflows;

                int32_t current_error = frequency_get_error();

                if (current_error != 0) {
                    // Use error^2 to adjust PWM for larger errors, but preserve sign.
                    // Make even smaller adjustments close to 0.
                    // This is all just guesses and should be investigated more fully.
                    int32_t adjustment = current_error > 3 ? abs(current_error) * current_error : current_error / 2;

                    // Apply it
                    TIM1->CCR2 -= adjustment;
                }

                circbuf_add(&circular_buffer, frequency_get_error());
                if (num_samples < CIRCULAR_BUFFER_LEN)
                    num_samples++;
            }

            previous_capture = capture;
            timer_overflows  = 0;
            last_pps         = current_tick;
            first            = 0;
        }

        if (!printing) {
            printing = 1;
            if (pps_indicator) {
                LCD_Puts(0, 0, "*");
            } else {
                LCD_Puts(0, 0, "-");
            }
            pps_indicator = !pps_indicator;
            printing      = 0;
        }
    }
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
        if(error > 2000 || error < -2000)
        {
            return 0;
        }
        else
        {
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
