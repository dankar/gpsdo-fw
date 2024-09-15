#include "LCD.h"
#include "frequency.h"
#include "stm32f1xx_hal.h"
#include "stm32f1xx_hal_gpio.h"
#include "stm32f1xx_hal_tim.h"
#include "stm32f1xx_hal_uart.h"
#include "tim.h"
#include "usart.h"
#include "gps.h"
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

volatile int printing = 0;

void warmup()
{
    char     buf[9];
    uint32_t warmup_time = 300;
    LCD_Clear();
    while (warmup_time != 0) {
        LCD_Puts(0, 0, " WARMUP");
        sprintf(buf, "  %ld  ", warmup_time);
        LCD_Puts(0, 1, buf);
        warmup_time--;

        for (int i = 0; i < 200; i++) {
            if (HAL_GPIO_ReadPin(ROTARY_PRESS_GPIO_Port, ROTARY_PRESS_Pin) == GPIO_PIN_RESET) {
                warmup_time = 0;
                break;
            }
            HAL_Delay(5);
        }
    }
}

void gpsdo(void)
{
    TIM1->CCR2 = 38458;

    LCD_Init();

    //    warmup();

    LCD_Clear();

    circbuf_init(&circular_buffer);
    HAL_Delay(100);
    frequency_start();

    HAL_TIM_Base_Start(&htim3);
    HAL_TIM_Encoder_Start(&htim3, TIM_CHANNEL_ALL);

    char screen_buffer[9];

    uint16_t view = 0;
    // uint16_t prev_encoder = 0;

    uint32_t last_print = 0;
    uint32_t seconds_passed = 0;

    while (1) {
        // Select view based on rotary encoder value
        uint16_t new_view = TIM3->CCR1 / 2 % 2;
        if(new_view != view)
        {
            view = new_view;
            last_print = 0;
        }

        gps_read();

        uint32_t now = HAL_GetTick();

        if (now - last_print > 1000) {
            if(seconds_passed == 3)
            {
                // Start adjusting the VCO after some time
                frequency_allow_adjustment(true);
            }
            last_print = now;
            
            while(printing == 1);
            printing = 1;
            
            if (view == 0) {
                char     ppb_string[5];
                uint32_t ppb = abs(frequency_get_ppb());

                if (ppb > 999) {
                    strcpy(ppb_string, ">=10");
                } else {
                    sprintf(ppb_string, "%ld.%02ld", ppb / 100, ppb % 100);
                }

                sprintf(screen_buffer, "%02d %s", num_sats, ppb_string);
                LCD_Puts(1, 0, screen_buffer);
                LCD_Puts(0, 1, gps_time);
            } else {
                LCD_Puts(1, 0, "       ");
                LCD_Puts(0, 1, "        ");
                sprintf(screen_buffer, "%ld", frequency_get_ppb());
                LCD_Puts(1, 0, screen_buffer);
                sprintf(screen_buffer, "PWM%ld", TIM1->CCR2);
                LCD_Puts(0, 1, screen_buffer);
            }
            printing = 0;
            
            seconds_passed++;
        }
    }
}
