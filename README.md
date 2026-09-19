# SIM800 UART Bridge

STM32F303RE firmware project for interfacing with a SIM800 module.

## Current features

- USART interrupt-based RX
- TX using `HAL_UART_Transmit_IT`
- Ring buffers for USART1 and USART2
- UART bridge between SIM800 and another serial device
- Line-based SIM800 response parser
- SIM800 state machine
- SMS notification handling via `+CMTI`
- SMS reading via `AT+CMGR`
- SMS command parsing
- GPIO command execution
- SMS response generation

## Architecture

```
SIM800
  |
USART1 RX interrupt
  |
Ring Buffer
  |
Line Parser
  |
SIM State Machine
  |
SMS Command Parser
  |
Command Execution
```

USART2 is used as a serial bridge/debug interface.

## Status

Initial working version. The project is intentionally under development.

### Planned improvements

- UART DMA
- More robust ring-buffer concurrency
- UART error and timeout handling
- More robust SIM800 response parsing
- Better module separation into `.c/.h` files
- Improved command/event handling
- Testing and error recovery

## Hardware

- STM32F303RE Nucleo
- SIM800 module
- UART connection to a second serial interface

## Note

This repository contains the current development version and is not intended to represent production-ready firmware.
