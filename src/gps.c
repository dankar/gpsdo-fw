#include "gps.h"
#include "usart.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "LCD.h"

#define MAX_GPS_LINE 512

char   gps_line[MAX_GPS_LINE];
char   gps_time[9]  = { '\0' };
char   num_sats     = 0;
size_t gps_line_len = 0;

#define FIFO_BUFFER_SIZE 128

typedef struct  {
    uint8_t buffer[FIFO_BUFFER_SIZE];
    size_t read;
    size_t write;
} fifo_buffer_t;

typedef enum {
    FIFO_WRITE,
    FIFO_READ
} fifo_operation;

volatile fifo_buffer_t fifo_buffer_gps = { 0 };
volatile fifo_buffer_t fifo_buffer_gps_ext = { 0 };

size_t fifo_next(volatile const fifo_buffer_t *fifo, fifo_operation op)
{
    if(op == FIFO_WRITE)
    {
        return (fifo->write + 1) % FIFO_BUFFER_SIZE;
    }
    else
    {
        return (fifo->read + 1) % FIFO_BUFFER_SIZE;
    }
}
    

bool fifo_write(volatile fifo_buffer_t *fifo, const uint8_t c)
{
    size_t next = fifo_next(fifo, FIFO_WRITE);
    if(next == fifo->read)
    {
        return false;
    }
    fifo->buffer[fifo->write] = c;
    fifo->write = next;
    return true;
}

bool fifo_read(volatile fifo_buffer_t *fifo, uint8_t *c)
{
    size_t next = fifo_next(fifo, FIFO_READ);
    if(next == fifo->write)
    {
        return false;
    }
    *c = fifo->buffer[fifo->read];
    fifo->read = next;

    return true;
}

volatile uint8_t it_buf;


void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if(huart == &huart3)
    {
        fifo_write(&fifo_buffer_gps, it_buf);
        HAL_UART_Receive_IT(&huart3, (uint8_t*)&it_buf, 1);
    } else if(huart == &huart2) {
        fifo_write(&fifo_buffer_gps_ext, it_buf);
        HAL_UART_Receive_IT(&huart2, (uint8_t*)&it_buf, 1);
    }   
}

void gps_start_it()
{
    HAL_UART_Receive_IT(&huart2, (uint8_t*)&it_buf, 1);
    HAL_UART_Receive_IT(&huart3, (uint8_t*)&it_buf, 1);
}
    
// Maybe use X-CUBE-GNSS here?
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
    uint8_t c;
    while (fifo_read(&fifo_buffer_gps, &c)) {
        gps_line[gps_line_len] = c;

        while(HAL_UART_Transmit_IT(&huart2, &c, 1) != HAL_OK);

        if (gps_line[gps_line_len] == '\n') {
            gps_line[++gps_line_len] = '\0';
            gps_parse(gps_line);
            gps_line_len = 0;
            continue;
        }
        gps_line_len++;
        if (gps_line_len >= MAX_GPS_LINE) {
            gps_line_len = 0;
            return;
        }
    }

    while(fifo_read(&fifo_buffer_gps_ext, &c)) {
        while(HAL_UART_Transmit_IT(&huart3, &c, 1) != HAL_OK);
    }
}
