#include "gps.h"
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "usart.h"

#define MAX_GPS_LINE 512

char   gps_line[MAX_GPS_LINE];
char   gps_time[9]  = { '\0' };
char   num_sats     = 0;
size_t gps_line_len = 0;

void gps_parse(char* line)
{
    if (strstr(line, "$GNGGA") == line) {
        char* pch = strtok(line, ",");

        pch = strtok(NULL, ","); // Time

        gps_time[0] = pch[0];
        gps_time[1] = pch[1];
        gps_time[2] = ':';
        gps_time[3] = pch[2];
        gps_time[4] = pch[3];
        gps_time[5] = ':';
        gps_time[6] = pch[4];
        gps_time[7] = pch[5];
        gps_time[8] = '\0';

        strtok(NULL, ",");
        strtok(NULL, ",");
        strtok(NULL, ",");
        strtok(NULL, ",");
        strtok(NULL, ","); // Fix

        num_sats = atoi(strtok(NULL, ",")); // Num sats used
    }
}

void gps_read()
{
    while (HAL_UART_Receive(&huart3, (uint8_t*)&gps_line[gps_line_len], 1, 1) == HAL_OK) {
        if (gps_line[gps_line_len] == '\n') {
            gps_line[++gps_line_len] = '\0';
            gps_parse(gps_line);
            gps_line_len             = 0;
            continue;
        }
        gps_line_len++;
        if (gps_line_len >= MAX_GPS_LINE) {
            gps_line_len = 0;
            return;
        }
    }
}
