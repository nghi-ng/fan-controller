# FreeRTOS Smart Fan Controller Using STM32

This project is a FreeRTOS-based smart fan controller implemented on the STM32 NUCLEO-F446RE board. The system controls a 5V DC fan using PWM, supports manual and automatic fan control, reads temperature and occupancy inputs, displays status on an LCD, and logs debug and measurement data over UART using DMA.

## Video

Watch the project demo video here: [FreeRTOS Smart Fan Controller Demo](https://drive.google.com/file/d/1gD9iBU9XO30GkgpWx3FDsAgzYrkc6Ngk/view?usp=sharing)

## Features

- FreeRTOS task-based architecture with clear task priorities
- PWM fan control using TIM3 CH1 on PA6
- MOSFET low-side fan driver with flyback diode protection
- Manual fan speed control using a rotary encoder
- Automatic fan control using temperature and PIR occupancy detection
- Four fan states: OFF, SPEED1, SPEED2, and SPEED3
- LED status indication for each fan state
- BMP280 temperature sensor over I2C
- Grove 16x2 LCD dashboard over I2C
- HC-SR501 PIR motion sensor for occupancy detection
- TIM2 interrupt-to-task latency measurement
- DWT cycle counter timestamping
- UART debug logging through USART2 TX DMA
- Optional CPU load generator for latency experiments

## Controls

- Rotary encoder:
  - Manual mode: increase or decrease fan speed
  - Automatic mode: encoder events are ignored
- Blue onboard button B1:
  - Switches between MANUAL and AUTO mode
- PIR sensor:
  - Manual mode: ignored for fan control
  - Automatic mode: allows temperature-based fan control only when occupied
  - No motion for 30 seconds: fan is forced OFF

## Project Structure

- `fan-controller-cubemx.ioc`: STM32CubeMX peripheral configuration
- `Core/Src/`: application source files and generated STM32 source files
- `Core/Inc/`: application headers and generated STM32 headers
- `Drivers/`: STM32 HAL and CMSIS drivers
- `Middlewares/`: FreeRTOS middleware
- `notes_code/`: supporting Markdown notes about STM32 basics, timers, interrupts, DMA, UART, and DWT
- `Debug/`: STM32CubeIDE debug build output

## Main Modules

- `app.c/.h`: application startup and module initialization
- `app_config.h`: test configuration switches
- `logger.c/.h`: queued UART logging using USART2 DMA
- `rt_measure.c/.h`: TIM2 ISR-to-task latency measurement
- `control.c/.h`: mode state machine, automatic/manual control, PIR logic
- `fan_control.c/.h`: TIM3 PWM fan output
- `encoder.c/.h`: rotary encoder polling
- `sensor.c/.h`: periodic temperature sensor task
- `bmp280.c/.h`: BMP280 temperature driver
- `i2c_bus.c/.h`: shared I2C access with FreeRTOS mutex
- `lcd.c/.h`: LCD commands and text output
- `display.c/.h`: LCD dashboard formatting
- `leds.c/.h`: fan state LED control
- `load.c/.h`: optional CPU load generation

## FreeRTOS Task Priorities

Task priority levels:

| Priority | Task |
|---|---|
| 4 | MeasurementTask |
| 3 | EncoderTask, ControlTask |
| 2 | FanTask |
| 1 | SensorTask, DisplayTask, DebugTask |
| 0 | LoadTask, defaultTask |

The low-priority measurement test can be enabled in `Core/Inc/app_config.h`:

```c
#define RT_MEASURE_LOW_PRIORITY_TEST  1U
```

For the final demo build, use:

```c
#define RT_MEASURE_LOW_PRIORITY_TEST  0U
```

## UART Logging

UART logging is handled by `DebugTask`. Other tasks send log messages to a queue. `DebugTask` starts transmission with:

```c
HAL_UART_Transmit_DMA(...)
```

When the DMA transfer completes, the UART callback wakes the debug task. This avoids blocking the application tasks while UART data is being transmitted.

Latency CSV logging can be enabled in `Core/Inc/app_config.h`:

```c
#define RT_MEASURE_LOG_CSV  1U
```

Example latency CSV format:

```text
event,latency_us,min_us,avg_us,max_us,jitter_us,missed,overflows,load_level
37400,12,12,12,23,0,0,0,0
```

## Build and Run

1. Open the project in STM32CubeIDE.
2. Open `fan-controller-cubemx.ioc` if pin or peripheral changes are needed.
3. Generate code from CubeMX if configuration was changed.
4. Clean and build the project.
5. Flash the STM32 NUCLEO-F446RE board.
6. Open Tera Term or another serial terminal.
7. Connect to the Nucleo ST-LINK virtual COM port.
