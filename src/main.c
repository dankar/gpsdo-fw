#include "LCD.h"
#include "frequency.h"
#include "main.h"
#include "tim.h"
#include "gps.h"
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "eeprom.h"
#include "menu.h"

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
            if (rotary_get_click()) {
                warmup_time = 0;
                break;
            }
            HAL_Delay(5);
        }
    }
}

uint8_t lcd_backslash[8] = {
	0b00000,
	0b10000,
	0b01000,
	0b00100,
	0b00010,
	0b00001,
	0b00000,
	0b00000
};

void gpsdo(void)
{
    HAL_TIM_Base_Start_IT(&htim2);
    
    EE_Init(&ee_storage, sizeof(ee_storage_t));
    EE_Read();
    if(ee_storage.pwm == 0xffff)
    {
        ee_storage.pwm = 38000;
    }
    TIM1->CCR2 = ee_storage.pwm;

    LCD_Init();

    LCD_CreateChar(1, lcd_backslash);

    warmup();

    LCD_Clear();

    HAL_Delay(100);
    frequency_start();

    HAL_TIM_Base_Start(&htim3);
    HAL_TIM_Encoder_Start(&htim3, TIM_CHANNEL_ALL);

    gps_start_it();

    while (1) {
        gps_read();
        menu_run();
    }
}
