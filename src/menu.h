#ifndef _MENU_H_
#define _MENU_H_

#include <stdbool.h>

extern volatile int menu_printing;

bool rotary_get_click();
void menu_run();

#endif
