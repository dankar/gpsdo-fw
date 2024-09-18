#include "int.h"
#include "LCD.h"
#include "frequency.h"
#include "tim.h"
#include "menu.h"
#include <stdlib.h>
#include <string.h>

volatile bool     allow_adjustment = false;
volatile uint32_t previous_capture = 0;
volatile uint32_t frequency        = 0;
volatile uint32_t capture          = 0;
volatile uint32_t num_samples      = 0;
volatile uint32_t timer_overflows  = 0;
volatile uint32_t device_uptime    = 0;
volatile uint8_t  first            = 1;
volatile uint32_t last_pps         = 0;

const char spinner[]   = "|/-\1";
uint8_t    pps_spinner = 0;

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef* htim)
{
    if (htim == &htim1) {
        timer_overflows++;
    } else if (htim == &htim2) {
        device_uptime++;
    }
}

// This gets run each time PPS goes high
void HAL_TIM_IC_CaptureCallback(TIM_HandleTypeDef* htim)
{
    uint32_t current_tick = HAL_GetTick();
    if (htim->Channel == HAL_TIM_ACTIVE_CHANNEL_1) {

        capture = HAL_TIM_ReadCapturedValue(htim, TIM_CHANNEL_1);

        if (allow_adjustment) {
            // Ignore first capture and do a sanity check on elapsed time since previous PPS
            if (!first && current_tick - last_pps < 1300) {
                frequency = capture - previous_capture + (TIM1->ARR + 1) * timer_overflows;

                int32_t current_error = frequency_get_error();

                if (current_error != 0) {
                    // Use error^3 to adjust PWM for larger errors, but preserve sign.
                    // Make even smaller adjustments close to 0.
                    // This is all just guesses and should be investigated more fully.
                    int32_t adjustment = 0;

                    if (abs(current_error) > 10) {
                        adjustment = abs(current_error) * current_error * 2;
                    } else if (abs(current_error) > 2) {
                        adjustment = abs(current_error) * current_error;
                    } else {
                        adjustment = current_error;
                    }

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

        if (!menu_printing) {
            menu_printing = 1;
            LCD_PutCustom(0, 0, spinner[pps_spinner]);
            pps_spinner   = (pps_spinner + 1) % strlen(spinner);
            menu_printing = 0;
        }
    }
}
