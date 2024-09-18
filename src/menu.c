#include "frequency.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "LCD.h"
#include "eeprom.h"
#include "gps.h"
#include "stm32f1xx_hal_gpio.h"
#include "int.h"

/// All times in ms
#define DEBOUNCE_TIME        100
#define SCREEN_REFRESH_TIME  500
#define VCO_ADJUSTMENT_DELAY 3000

volatile uint32_t rotary_down_time      = 0;
volatile uint32_t rotary_up_time        = 0;
volatile bool     rotary_press_detected = 0;
volatile int      menu_printing         = 0;

bool rotary_is_down() { return rotary_down_time > rotary_up_time && (HAL_GetTick() - rotary_down_time) > DEBOUNCE_TIME; }

bool rotary_get_click()
{
    bool is_down = rotary_is_down();

    if (is_down && !rotary_press_detected) {
        rotary_press_detected = true;
        return true;
    } else if (!is_down) {
        rotary_press_detected = false;
    }

    return false;
}

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    if (GPIO_Pin == ROTARY_PRESS_Pin) {
        if (HAL_GPIO_ReadPin(ROTARY_PRESS_GPIO_Port, ROTARY_PRESS_Pin) == GPIO_PIN_SET) {
            rotary_up_time = HAL_GetTick();
        } else {
            rotary_down_time = HAL_GetTick();
        }
    }
}

typedef enum { SCREEN_MAIN, SCREEN_PPB, SCREEN_PWM, SCREEN_UPTIME, SCREEN_MAX } menu_screen;

static menu_screen current_menu_screen = SCREEN_MAIN;
static uint32_t    last_screen_refresh = 0;
static bool        cal_save_screen     = false;

static void menu_force_redraw() { last_screen_refresh = 0; }

static void menu_draw()
{
    char    screen_buffer[13];
    char    ppb_string[5];
    int32_t ppb;

    switch (current_menu_screen) {
    default:
    case SCREEN_MAIN:
        // Main screen with satellites, ppb and UTC time
        ppb = abs(frequency_get_ppb());

        if (ppb > 999) {
            strcpy(ppb_string, ">=10");
        } else {
            sprintf(ppb_string, "%ld.%02ld", ppb / 100, ppb % 100);
        }

        sprintf(screen_buffer, "%02d %s", num_sats, ppb_string);
        LCD_Puts(1, 0, screen_buffer);
        LCD_Puts(0, 1, gps_time);
        break;
    case SCREEN_PPB:
        // Screen with ppb
        ppb = frequency_get_ppb();
        LCD_Puts(1, 0, "PPB    ");
        LCD_Puts(0, 1, "        ");
        sprintf(screen_buffer, "%ld.%02d", ppb / 100, abs(ppb) % 100);
        LCD_Puts(0, 1, screen_buffer);
        break;
    case SCREEN_PWM:
        // Screen with current PPM
        LCD_Puts(1, 0, "PWM    ");
        LCD_Puts(0, 1, "        ");
        sprintf(screen_buffer, "%ld", TIM1->CCR2);
        LCD_Puts(0, 1, screen_buffer);
        break;
    case SCREEN_UPTIME:
        LCD_Puts(1, 0, "UPTIME ");
        LCD_Puts(0, 1, "        ");
        sprintf(screen_buffer, "%ld", device_uptime);
        LCD_Puts(0, 1, screen_buffer);
        break;
    }
}

void menu_run()
{

    // Select view based on rotary encoder value
    menu_screen new_view = TIM3->CCR1 / 2 % SCREEN_MAX;
    if (new_view != current_menu_screen) {
        current_menu_screen = new_view;
        LCD_Clear();
        menu_force_redraw();
    }

    uint32_t now = HAL_GetTick();

    if (rotary_get_click()) {
        if (!cal_save_screen) {
            cal_save_screen = true;
            LCD_Clear();
        } else {
            ee_storage.pwm = TIM1->CCR2;
            EE_Write();
            cal_save_screen = false;
            LCD_Clear();
        }

        menu_force_redraw();
    }

    if (now - last_screen_refresh > SCREEN_REFRESH_TIME) {

        // Move this to some other place, the menu system
        // shouldn't be in charge of this
        if (now >= VCO_ADJUSTMENT_DELAY) {
            // Start adjusting the VCO after some time
            frequency_allow_adjustment(true);
        }

        last_screen_refresh = now;

        // Not very effective spinlock...
        while (menu_printing == 1)
            ;
        menu_printing = 1;

        if (cal_save_screen) {
            LCD_Puts(0, 0, " PRESS ");
            LCD_Puts(0, 1, "TO SAVE");
        } else {
            menu_draw();
        }

        menu_printing = 0;
    }
}
