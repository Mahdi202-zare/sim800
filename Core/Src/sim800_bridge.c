/*
 * SIM800 UART Bridge - initial development version
 *
 * STM32F303RE
 * USART1 <-> SIM800
 * USART2 <-> serial bridge/debug
 */

#include "stdio.h"
#include "string.h"
#include "stdbool.h"
#include <stdlib.h>

#define BUFFER_SIZE 256
#define LINE_BUFFER_SIZE 256
#define SMS_BUFFER_SIZE 126

typedef struct {
    volatile uint8_t buffer[BUFFER_SIZE];
    volatile uint16_t tail;
    volatile uint16_t head;
    volatile uint16_t count;
    volatile uint8_t overflow;
} RingBuffer_t;

RingBuffer_t usart1_rx_rb;
RingBuffer_t usart1_tx_rb;
RingBuffer_t usart2_tx_rb;
RingBuffer_t usart2_rx_rb;

typedef struct {
    char Phone_Number_1[12];
    char *text;
} SMS_t;

SMS_t strc_Mahdi_Info;

typedef enum {
    _IDLE,
    _SMS_RECEVIE,
    _SMS_TEXT,
} SIM_STATE_t;

SIM_STATE_t courent_sim_state = _IDLE;

typedef enum {
    EV_None,
    EV_CMIT,
    EV_SMS_Header,
    EV_SMS_Text,
    EV_SMS_Complete,
    EV_SMS_Send_Complete,
    EV_SMS_Send_Text,
    EV_Return,
} SimEvent_t;

SimEvent_t simEvent = EV_None;

typedef enum {
    cmd_none,
    cmd_led_ga0_on,
    cmd_led_ga0_off,
    cmd_relay1_on,
    cmd_relay1_off,
    cmd_temp_send,
} Commond_t;

volatile bool usart2_tx_busy = false;
volatile bool usart1_tx_busy = false;
volatile bool flag_send_sms_info = false;

uint8_t usart1_rx_byte;
uint8_t usart1_tx_byte;
uint8_t usart2_tx_byte;
uint8_t usart2_rx_byte;

char phone_nuber[15];
char sms_buffer[SMS_BUFFER_SIZE];
char Line_Buffer[LINE_BUFFER_SIZE];
uint16_t line_index;

void Mahdi_info(SMS_t *info)
{
    strcpy(info->Phone_Number_1, "09162172787");
}

void RingBuffer_Init(RingBuffer_t *rb)
{
    rb->tail = 0;
    rb->head = 0;
    rb->count = 0;
    rb->overflow = 0;
}

uint8_t RingBuffer_Write(RingBuffer_t *rb, uint8_t data)
{
    if (rb->count >= BUFFER_SIZE) {
        rb->overflow = 1;
        return 0;
    }

    rb->buffer[rb->head] = data;
    rb->head++;

    if (rb->head >= BUFFER_SIZE)
        rb->head = 0;

    rb->count++;
    return 1;
}

uint8_t RingBuffer_Read(RingBuffer_t *rb, uint8_t *data)
{
    if (rb->count == 0)
        return 0;

    *data = rb->buffer[rb->tail];
    rb->tail++;

    if (rb->tail >= BUFFER_SIZE)
        rb->tail = 0;

    rb->count--;
    return 1;
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *uart)
{
    if (uart->Instance == USART1) {
        GPIOA->ODR |= (1 << 5);
        RingBuffer_Write(&usart1_rx_rb, usart1_rx_byte);
        HAL_UART_Receive_IT(&huart1, &usart1_rx_byte, 1);
    }

    if (uart->Instance == USART2) {
        RingBuffer_Write(&usart2_rx_rb, usart2_rx_byte);
        HAL_UART_Receive_IT(&huart2, &usart2_rx_byte, 1);
    }
}

void HAL_UART_TxCpltCallback(UART_HandleTypeDef *usart)
{
    if (usart->Instance == USART2) {
        if (RingBuffer_Read(&usart2_tx_rb, &usart2_tx_byte)) {
            HAL_UART_Transmit_IT(&huart2, &usart2_tx_byte, 1);
        } else {
            usart2_tx_busy = 0;
        }
    }

    if (usart->Instance == USART1) {
        if (RingBuffer_Read(&usart1_tx_rb, &usart1_tx_byte)) {
            HAL_UART_Transmit_IT(&huart1, &usart1_tx_byte, 1);
        } else {
            usart1_tx_busy = 0;
        }
    }
}

void Usart2_SendByte(uint8_t data)
{
    if (RingBuffer_Write(&usart2_tx_rb, data)) {
        if (!usart2_tx_busy) {
            if (RingBuffer_Read(&usart2_tx_rb, &usart2_tx_byte)) {
                usart2_tx_busy = 1;
                HAL_UART_Transmit_IT(&huart2, &usart2_tx_byte, 1);
            }
        }
    }
}

void Usart1_SendByte(uint8_t data)
{
    if (RingBuffer_Write(&usart1_tx_rb, data)) {
        if (!usart1_tx_busy) {
            if (RingBuffer_Read(&usart1_tx_rb, &usart1_tx_byte)) {
                usart1_tx_busy = 1;
                HAL_UART_Transmit_IT(&huart1, &usart1_tx_byte, 1);
            }
        }
    }
}

void Usart1_SendString(char *str)
{
    while (*str != '\0') {
        Usart1_SendByte((uint8_t)*str);
        str++;
    }
}

uint8_t sms_index;
uint16_t sms_buffer_index = 0;

void Execut_commond(Commond_t cmd)
{
    if (cmd == cmd_none)
        return;

    switch (cmd) {
        case cmd_led_ga0_on:
            HAL_GPIO_WritePin(GPIOA, GPIO_PIN_0, GPIO_PIN_SET);
            break;

        case cmd_led_ga0_off:
            HAL_GPIO_WritePin(GPIOA, GPIO_PIN_0, GPIO_PIN_RESET);
            break;

        case cmd_relay1_on:
            HAL_GPIO_WritePin(GPIOA, GPIO_PIN_1, GPIO_PIN_SET);
            break;

        case cmd_relay1_off:
            HAL_GPIO_WritePin(GPIOA, GPIO_PIN_1, GPIO_PIN_RESET);
            break;

        default:
            break;
    }
}

Commond_t paser_commond(char *data)
{
    if (strncmp(data, "LED_ON", 6) == 0)
        return cmd_led_ga0_on;

    if (strncmp(data, "LED_OFF", 7) == 0)
        return cmd_led_ga0_off;

    if (strncmp(data, "RELAY_ON", 8) == 0)
        return cmd_relay1_on;

    if (strncmp(data, "RELAY_OFF", 9) == 0)
        return cmd_relay1_off;

    return cmd_none;
}

void build_Respons(Commond_t cmd, SMS_t *sms_info)
{
    switch (cmd) {
        case cmd_led_ga0_on:
            sms_info->text = "LED IS ON";
            break;

        case cmd_led_ga0_off:
            sms_info->text = "LED IS OFF";
            break;

        case cmd_relay1_on:
            sms_info->text = "RELAY IS ON";
            break;

        case cmd_relay1_off:
            sms_info->text = "RELAY IS OFF";
            break;

        case cmd_none:
            sms_info->text = "UNKNOWN COMMAND";
            break;

        default:
            sms_info->text = "UNKNOWN COMMAND";
            break;
    }
}

void Send_SMS_Commond(SMS_t *sms_info)
{
    char cmd[45];

    snprintf(cmd, sizeof(cmd),
             "AT+CMGS=\"%s\"\r\n",
             sms_info->Phone_Number_1);

    for (int i = 0; cmd[i] != '\0'; i++)
        Usart1_SendByte(cmd[i]);
}

void Send_SMS_Info(void)
{
    Usart1_SendString(strc_Mahdi_Info.text);
    Usart1_SendByte(26);
}

void Save_Sms_Text(char *data)
{
    size_t len = strlen(data);

    if ((sms_buffer_index + len + 1) < SMS_BUFFER_SIZE) {
        memcpy(&sms_buffer[sms_buffer_index], data, len);
        sms_buffer_index += len;
        sms_buffer[sms_buffer_index++] = '\n';
        sms_buffer[sms_buffer_index] = '\0';
    }
}

void Found_Number(char *data)
{
    char *start = strchr(data, ',');

    if (start != NULL) {
        start += 2;

        char *end = strchr(start, '"');

        if (end != NULL) {
            size_t len = end - start;

            if (len < sizeof(phone_nuber)) {
                memcpy(phone_nuber, start, len);
                phone_nuber[len] = '\0';
            }
        }
    }
}

void Found_Sms_Index(char *data)
{
    char *comma = strchr(data, ',');

    if (comma != NULL)
        sms_index = atoi(comma + 1);
}

void Open_Sms(void)
{
    char cmd[32];

    snprintf(cmd, sizeof(cmd), "AT+CMGR=%d\r\n", sms_index);

    for (int i = 0; cmd[i] != '\0'; i++)
        Usart1_SendByte(cmd[i]);
}

void Enter_SIM_State(SIM_STATE_t state)
{
    switch (state) {
        case _IDLE:
            if (flag_send_sms_info) {
                Send_SMS_Commond(&strc_Mahdi_Info);
                flag_send_sms_info = false;
            }
            break;

        case _SMS_RECEVIE:
            sms_buffer_index = 0;
            sms_buffer[0] = '\0';
            Open_Sms();
            break;

        case _SMS_TEXT:
            break;
    }
}

void Exid_SIM_State(SIM_STATE_t state)
{
    switch (state) {
        case _IDLE:
            break;

        case _SMS_RECEVIE:
            break;

        case _SMS_TEXT:
            break;
    }
}

void Change_State(SIM_STATE_t new_state)
{
    if (courent_sim_state == new_state)
        return;

    Exid_SIM_State(courent_sim_state);
    courent_sim_state = new_state;
    Enter_SIM_State(courent_sim_state);
}

void Handle_Sim_Event(SimEvent_t ev, char *data)
{
    if (ev == EV_None)
        return;

    switch (ev) {
        case EV_CMIT:
            Found_Sms_Index(data);
            Change_State(_SMS_RECEVIE);
            break;

        case EV_SMS_Header:
            Found_Number(data);
            Change_State(_SMS_TEXT);
            break;

        case EV_SMS_Text:
            Save_Sms_Text(data);
            break;

        case EV_SMS_Complete: {
            Commond_t cmd = paser_commond(sms_buffer);
            build_Respons(cmd, &strc_Mahdi_Info);
            Execut_commond(cmd);
            break;
        }

        case EV_SMS_Send_Text:
            Send_SMS_Info();
            break;

        case EV_Return:
            Change_State(_IDLE);
            break;

        default:
            break;
    }
}

void Process_Line(char *data)
{
    switch (courent_sim_state) {
        case _IDLE:
            if (strncmp(data, "+CMTI:", 6) == 0)
                Handle_Sim_Event(EV_CMIT, data);
            break;

        case _SMS_RECEVIE:
            if (strncmp(data, "+CMGR:", 6) == 0)
                Handle_Sim_Event(EV_SMS_Header, data);
            break;

        case _SMS_TEXT:
            if (strcmp(data, "OK") == 0) {
                Handle_Sim_Event(EV_SMS_Complete, data);
                flag_send_sms_info = true;
                Handle_Sim_Event(EV_Return, NULL);
            } else {
                Handle_Sim_Event(EV_SMS_Text, data);
            }
            break;
    }
}

bool lineBuffer_over = false;

void Linebuffer_ProcessByte(uint8_t data)
{
    if (data == '>') {
        Handle_Sim_Event(EV_SMS_Send_Text, NULL);
        return;
    }

    if (data == '\r')
        return;

    if (data == '\n') {
        Line_Buffer[line_index] = '\0';
        Process_Line(Line_Buffer);
        line_index = 0;
        return;
    }

    if (line_index >= LINE_BUFFER_SIZE - 1) {
        lineBuffer_over = true;
        line_index = 0;
        return;
    }

    Line_Buffer[line_index] = data;
    line_index++;
}

uint8_t data;

void bridg(void)
{
    if (RingBuffer_Read(&usart1_rx_rb, &data)) {
        Usart2_SendByte(data);
        Linebuffer_ProcessByte(data);
    }

    if (RingBuffer_Read(&usart2_rx_rb, &data))
        Usart1_SendByte(data);
}
