# Day 1: Introduction to STM32

## Notes 

- Installed and became familiar with STM32 tools: STM32CubeIDE and STM32CubeMX
- Looked at the STM32CubeF4 package, which contains HAL drivers, LL drivers, middleware libraries
- Looked at an STM32 NUCLEO-F446RE LED blinky example.
- Learned that `PA5` means pin 5 on GPIO port A.

## Important Files And Tools

### STM32CubeIDE

STM32CubeIDE is the main development environment used to write, build, flash, and debug STM32 firmware.

### STM32CubeMX

STM32CubeMX is the graphical configuration tool used to configure pins, clocks, peripherals, middleware, and project settings.

### `.ioc` File

The `.ioc` file is the STM32CubeMX configuration file. In this project, the file is:

```text
fan-controller-cubemx.ioc
```

It stores information such as:

- Which STM32 board or chip is used.
- Which pins are configured as GPIO, UART, I2C, PWM, and other peripherals.
- Clock configuration.
- Enabled peripherals.
- Middleware such as FreeRTOS.
- Project generation settings.

Examples from this project:

- `STM32F446RETx` is the selected MCU.
- `USART2` is enabled.
- `PA5` is the LD2 green LED.
- `PC13` is the B1 blue button.
- `PA2` and `PA3` are UART TX and RX.

## GPIO Output

### Toggle LD2

This toggles LD2 on and off every 1000 ms.

```c
HAL_GPIO_TogglePin(LD2_GPIO_Port, LD2_Pin);
HAL_Delay(1000);
```

`LD2` is connected to `PA5`, which means pin 5 on GPIO port A.

### Explicit On And Off

This turns LD2 on for 200 ms, then off for 800 ms.

```c
HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, GPIO_PIN_SET);
HAL_Delay(200);
HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, GPIO_PIN_RESET);
HAL_Delay(800);
```

`GPIO_PIN_SET` means the pin is driven HIGH.

`GPIO_PIN_RESET` means the pin is driven LOW.

For LD2 on the Nucleo board:

```text
GPIO_PIN_SET   = LED on
GPIO_PIN_RESET = LED off
```

## GPIO Input

### Button Controls LED

This turns LD2 on while the blue button is pressed.

```c
if (HAL_GPIO_ReadPin(B1_GPIO_Port, B1_Pin) == GPIO_PIN_RESET)
{
    HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, GPIO_PIN_SET);
}
else
{
    HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, GPIO_PIN_RESET);
}
```

The blue button `B1` is connected to `PC13`.

On this board, the button is active low:

```text
not pressed = GPIO_PIN_SET
pressed     = GPIO_PIN_RESET
```

## Button Toggle LED

This version toggles LD2 once when the button is newly pressed.

```c
GPIO_PinState currentButtonState = HAL_GPIO_ReadPin(B1_GPIO_Port, B1_Pin);

if ((lastButtonState == GPIO_PIN_SET) && (currentButtonState == GPIO_PIN_RESET))
{
    HAL_GPIO_TogglePin(LD2_GPIO_Port, LD2_Pin);
    HAL_Delay(50);
}

lastButtonState = currentButtonState;
```

This condition:

```c
(lastButtonState == GPIO_PIN_SET) && (currentButtonState == GPIO_PIN_RESET)
```

means:

```text
last time: button was not pressed
now:       button is pressed
```

That detects a new button press.

## UART Print

This sends text from the STM32 to the PC over UART.

```c
uint8_t msg[] = "Hello Nghi\r\n";
HAL_UART_Transmit(&huart2, msg, sizeof(msg) - 1, HAL_MAX_DELAY);
HAL_Delay(1000);
```

`sizeof(msg) - 1` is used because C strings include a hidden null terminator `\0`. The null terminator marks the end of the string in C, but it does not need to be sent over UART.

`\r\n` is a line ending:

```text
\r = carriage return, move to start of line
\n = newline, move to next line
```

## Questions And Answers

### What Is HAL?

HAL means Hardware Abstraction Layer. It is an STM32 library that gives higher-level functions for controlling peripherals.

Example:

```c
HAL_GPIO_TogglePin(LD2_GPIO_Port, LD2_Pin);
```

Instead of writing directly to hardware registers, HAL gives readable functions.

### What Is LL?

LL means Low-Layer driver. LL is closer to the STM32 hardware registers than HAL.

Simple difference:

```text
HAL = easier and higher-level
LL  = lower-level and more direct
```

### What Is ST-LINK?

ST-LINK is the programmer and debugger built into many STM32 Nucleo boards.

It lets CubeIDE:

- Flash compiled code into the STM32 microcontroller.
- Debug the program using breakpoints and variable inspection.
- Use the ST-LINK Virtual COM Port for serial communication.

### What Does `GPIO_PinState` Mean?

`GPIO_PinState` is an enum from the STM32 HAL GPIO driver.

```c
typedef enum
{
  GPIO_PIN_RESET = 0,
  GPIO_PIN_SET
} GPIO_PinState;
```

So a pin state can be:

```text
GPIO_PIN_RESET = LOW  = 0
GPIO_PIN_SET   = HIGH = 1
```

### What Does Active Low Mean?

Active low means the signal is considered active when the pin is LOW.

For the blue button:

```text
not pressed = HIGH = GPIO_PIN_SET
pressed     = LOW  = GPIO_PIN_RESET
```

So pressing the button gives `GPIO_PIN_RESET`.

### What Is `HAL_MAX_DELAY`?

`HAL_MAX_DELAY` means wait as long as possible before timing out.

It is defined in the HAL as:

```c
#define HAL_MAX_DELAY 0xFFFFFFFFU
```

In this UART example:

```c
HAL_UART_Transmit(&huart2, msg, sizeof(msg) - 1, HAL_MAX_DELAY);
```

the STM32 waits until the UART message is transmitted.

### What Are `USER CODE BEGIN` And `USER CODE END`?

These are special comments used by CubeMX.

```c
/* USER CODE BEGIN 3 */

/* USER CODE END 3 */
```

CubeMX preserves code written between these markers when code is regenerated.

Simple rule:

```text
USER CODE BEGIN 2 = setup code, runs once
USER CODE BEGIN 3 = loop code, runs forever
```

## Step-By-Step: How To Run STM32 Code

1. Open the project in STM32CubeIDE.
2. Open the `.ioc` file if pin or peripheral configuration needs to be changed.
3. Generate code if CubeMX settings were changed.
4. Write user code only inside `USER CODE BEGIN` and `USER CODE END` sections.
5. Build the project using the hammer icon.
6. Connect the STM32 board using the ST-LINK USB port.
7. Flash the program using Run or Debug.
8. Use the debugger, LED behavior, and UART output to test the program.

## Next Learning Steps

1. Button controls LED.
2. Button toggles LED on and off.
3. Debounce the button.
4. Print button messages over UART.
5. Use timers instead of `HAL_Delay()`.
6. Generate PWM for fan speed control.
