#include "gps.h"
#include "LCD.h"
#include "main.h"
#include "stm32f1xx_hal_uart.h"
#include "usart.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_GPS_LINE 512

char   gps_line[MAX_GPS_LINE];
char   gps_time[9]  = { '\0' };
char   num_sats     = 0;
size_t gps_line_len = 0;

#define FIFO_BUFFER_SIZE 256

typedef struct {
    uint8_t buffer[FIFO_BUFFER_SIZE];
    size_t  read;
    size_t  write;
} fifo_buffer_t;

typedef enum { FIFO_WRITE, FIFO_READ } fifo_operation;

volatile fifo_buffer_t fifo_buffer_gps  = { 0 };
volatile fifo_buffer_t fifo_buffer_comm = { 0 };

size_t fifo_next(volatile const fifo_buffer_t* fifo, fifo_operation op)
{
    if (op == FIFO_WRITE) {
        return (fifo->write + 1) % FIFO_BUFFER_SIZE;
    } else {
        return (fifo->read + 1) % FIFO_BUFFER_SIZE;
    }
}

bool fifo_write(volatile fifo_buffer_t* fifo, const uint8_t c)
{
    size_t next = fifo_next(fifo, FIFO_WRITE);
    if (next == fifo->read) {
        return false;
    }
    fifo->buffer[fifo->write] = c;
    fifo->write               = next;
    return true;
}

bool fifo_read(volatile fifo_buffer_t* fifo, uint8_t* c)
{
    if (fifo->read == fifo->write) {
        return false;
    }
    *c         = fifo->buffer[fifo->read];
    fifo->read = fifo_next(fifo, FIFO_READ);

    return true;
}

#define GPS_RX_BUFFER_SIZE  20
#define COMM_RX_BUFFER_SIZE 1

volatile uint8_t gps_it_buf[GPS_RX_BUFFER_SIZE];
volatile uint8_t comm_it_buf[COMM_RX_BUFFER_SIZE];

static void gps_start_gps_rx()
{
    if (HAL_UART_Receive_DMA(&huart3, (uint8_t*)gps_it_buf, GPS_RX_BUFFER_SIZE) != HAL_OK) {
        Error_Handler();
    }
}
static void gps_start_comm_rx()
{
    if (HAL_UART_Receive_DMA(&huart2, (uint8_t*)comm_it_buf, COMM_RX_BUFFER_SIZE) != HAL_OK) {
        Error_Handler();
    }
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef* huart)
{
    if (huart == &huart3) {
        for (size_t i = 0; i < GPS_RX_BUFFER_SIZE; i++) {
            fifo_write(&fifo_buffer_gps, gps_it_buf[i]);
        }
        gps_start_gps_rx();
    } else if (huart == &huart2) {
        for (size_t i = 0; i < COMM_RX_BUFFER_SIZE; i++) {
            fifo_write(&fifo_buffer_comm, comm_it_buf[i]);
        }
        gps_start_comm_rx();
    }
}

void gps_start_it()
{
    gps_start_gps_rx();
    gps_start_comm_rx();
}

// Maybe use X-CUBE-GNSS here?
void gps_parse(char* line)
{
    if (strstr(line, "GGA") == line+3) {
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

// Small risk of overrun here...
#define SEND_BUFFER_SIZE 128
uint8_t send_buf[SEND_BUFFER_SIZE];
uint8_t gps_send_buf[SEND_BUFFER_SIZE];
uint8_t comm_send_buf[SEND_BUFFER_SIZE];
size_t  send_size;

void gps_read()
{
    send_size = 0;
    uint8_t c;
    while (fifo_read(&fifo_buffer_gps, &c)) {
        gps_line[gps_line_len++] = c;
        send_buf[send_size++]    = c;
        if (c == '\n') {
            gps_line[gps_line_len] = '\0';
            gps_parse(gps_line);
            gps_line_len = 0;
            continue;
        }
        if (gps_line_len >= MAX_GPS_LINE) {
            gps_line_len = 0;
            return;
        }
    }

    if (send_size) {
        while (huart2.gState != HAL_UART_STATE_READY)
            ;
        memcpy(gps_send_buf, send_buf, SEND_BUFFER_SIZE);
        HAL_UART_Transmit_IT(&huart2, gps_send_buf, send_size);
    }

    send_size = 0;
    while (fifo_read(&fifo_buffer_comm, &c)) {
        send_buf[send_size++] = c;
    }
    
    if (send_size) {
        while (huart3.gState != HAL_UART_STATE_READY)
            ;
        memcpy(comm_send_buf, send_buf, SEND_BUFFER_SIZE);
        HAL_UART_Transmit_IT(&huart3, comm_send_buf, send_size);
    }
    
}
