## Alternatve firmware for the BH3SAP GPSDO

This is an alternative firmware for the BH3SAP GPSDO sold on various platforms.

### Theory of operation

Most of this is based on reverse engineering the circuit, and which pins certain things were connected to. The base of the device is a Bluepill board with a STM32F103C8T6 MCU. The oscillator is an Isotemp 143-141 OCXO which can be trimmed with a DC voltage. The output of the oscillator is buffered by a OPA692 and has an output resistor of 51 Ohms. The GPS differs between models, some have an ATGM336H and others a u-blox Neo-6M (perhaps not authentic). The 1PPS output from the GPS is not buffered, so it has very high output impedande.

* The OCXO is connected to the OSC_IN on the Bluepill board. The MCU is configured to be driven by an oscillator, so the crystal mounted on the Bluepill does nothing.
* The PPS is connected to the CH1 input on TIM1.
* The VCO is controlled via PWM from CH2 of TIM1. The PWM signal is sent to a couple of low pass filters, giving a DC voltage.

The MCU will PLL the clock up to 70MHz and then TIM1 is setup to count the internal 70MHz clock, while being gated by the PPS pulse from the GPS module. This means that it continually counts how many cycles on the clock passes between each PPS pulse. This is then used to adjust the VCO.

The VCO is simply adjusted by the error detected between two pulses. If 70000001 clocks are counted, the VCO voltage will drop a bit and so on. This is really simple, but due to the small adjustments, it will average out over time and it should work out since the counter is always running. If we are running at 70 000 000.01 we will get one more clock every 100 seconds, which will then cause a small adjustment (smallest adjustment possible).

It's fairly slow to reach a steady state, and it can probably easily be sped up with a better algorithm.

The displayed PPB error is a long running average. It counts the number of clock ticks over 128 seconds and compares it to the expected (128*SYSCLK)

### Usage

Power on the device with GPS antenna connected. Wait a long while for the PPB to reach close to zero. The used PWM value can then be stored in flash by pressing the encoder twice (a message will be shown after the first press).

This PWM will then be used on the next boot, and if no GPS antenna is connected, it will not be adjusted further.

The original manual for the device talks about running the device without a GPS antenna after calibration, but I would advice against that since the oscillator seems sensitive to both ambient temperature, vibrations and orientation. Best results will be had when the GPS antenna is connected at all times.

### Building and flashing

Clone the repo, update submodules and do the cmake. (Or just download a release) You should not need any other dependencies than arm-none-eabi-gcc. The bluepill can be flashed in multiple ways, check the documentation for it for information. Included is a openocd configuration for connecting to the device via SWD using a JLink adapter.

### Display

The display consists of multiple screens. The main one on bootup show this:

```
|10 1.12
18:27:55
```

Top left corner contains an indicator for PPS pulses. Next to that is the current number of satellites used by the GPS module. To the right of that is the current measured PPB error. The PPB error will show ">=10" if the error is larger than 9.99, this is only due to a lack of space on the display.

Bottom line is the current UTC time from GPS module.

Rotating the encoder will switch to additional screens showing:

1) Full estimated PPB error
2) Current PWM 
3) Uptime of the device, in seconds

### USB

It would be nice to have NMEA output over USB, and the Bluepill dev board in the GPSDO does have a USB connector. It's however difficult to use since it requires a PLLCLK of 48MHz. But since we use 10MHz as input instead of 8MHz this can't be achieved. It should be possible to run the HSI to the PLL and then run the USB off of that. Then run the HSE directly to the peripherals. But then the timers would be running at 10MHz and that would cause the PWM to be slower, and the measurements to have lower resolution.

### GPS passthrough

Instead of using the USB, I have added GPS passthrough on an unused UART. Pins PA2 (TX) and PA3 (RX) can be used to communicate with the GPS module. This is bidirectional, so the GPS can be used by a computer or configured via manufacturer software.

I ended up routing these two pins and ground to an external header on the backside of the device, and then plugging in a serial to USB converter when needed.
