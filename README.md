# STM32 I2C v2 Timing Calculator

A command-line utility to compute timing values for the STM32 I2C v2 peripheral (`I2C_TIMINGR` register).

Supports Standard Mode (100 kHz), Fast Mode (400 kHz), and Fast Mode Plus (1 MHz), taking into account analog filter settings and the I2C peripheral input clock.

> [!] For STM32 microcontrollers with I2C v2 peripheral (e.g. STM32F0, STM32F3, STM32L0, STM32L4).

---

## Source

Timing calculation logic adapted from the [Zephyr RTOS project](https://github.com/zephyrproject-rtos/zephyr/blob/7efa5c87dde1bf82ab8cf6cff300025c04daee76/drivers/i2c/i2c_ll_stm32_v2.c).

---

## 🛠️ Build

```bash
cmake -B build
cmake --build build
````

The resulting binary will be located at:

```
build/stm32_i2c_v2_timing_calc
```

## Usage

```bash
stm32_i2c_v2_timing_calc [options]
```

### Options

| Option                      | Description                       | Default   |
| --------------------------- | --------------------------------- | --------- |
| `-h`, `--help`              | Show help message                 | —         |
| `-b`, `--bus-clock` FILE    | I2C peripheral input clock (Hz)   | `8000000` |
| `-s`, `--speed` FILE        | Desired I2C bus speed (Hz)        | `100000`  |
| `-a`, `--use-analog-filter` | Enable analog filter compensation | `false`   |

---

## Example

```bash
$ ./stm32_i2c_v2_timing_calc -b 48000000 -s 1000000 -a
Use analog filter: true
I2C bus clock: 48000000 Hz
I2C speed: 1000000 Hz
------------------------------------
I2C_TIMINGR: 00500A13
Prescaler: 0
SCL low period: 19
SCL high period: 10
SDA delay (data hold time): 0
SCL delay (data setup time): 5
```

This value can be used directly in your STM32 firmware:

```c
I2C_TIMINGR(I2C1) = 0x00500A13;
```

or

```c
i2c_set_prescaler(I2C1, 0);
i2c_set_scl_low_period(I2C1, 19);
i2c_set_scl_high_period(I2C1, 10);
i2c_set_data_hold_time(I2C1, 0);
i2c_set_data_setup_time(I2C1, 5);
```
