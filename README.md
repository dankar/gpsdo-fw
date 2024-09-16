## Alternatve firmware for the BH3SAP GPSDO

### Reverse engineering/how the firmware works

I took apart the GPSDO and checked the connections, and then wrote my own firmware for it. I can't be sure how the original firmware works fully, but there are some clues:

* The OCXO is connected to the OSC_IN on the Blue Pill board. Funnily enough the original 8MHz xtal is still there, but I suppose the OCXO overrides it.
* The PPS is connected to the CH1 input on TIM1.
* The VCO is controlled via PWM from TIM2.

What I ended up doing in my firmware is to PLL the clock up to 70MHz. I don't think this should hurt anything(?) and even if it's just PLL'd, it should give us some more resolution in the measurements, maybe? Then TIM1 is setup to count the internal 70MHz clock, and it is gated by the PPS pulse from the GPS module. This means that it continually counts how many cycles on the clock passes between each PPS pulse. This is then used to adjust the VCO.

The VCO is simply adjusted by the error detected between two pulses. If 70000001 clocks are counted, the VCO voltage will drop a bit and so on. This is really simple, but due to the small adjustments, it will average out over time and we sort of get "super sampling".

It's fairly slow to reach a steady state, and it can probably easily be sped up. I don't know if my logic is sound, so it might be tuning the OCXO to 9.999998MHz instead of 10MHz.

The displayed PPB error is a long running average. It counts the number of clock ticks over 128 seconds and compares it to the expected (128*SYSCLK)

### Display info

Example of display:

```
*10 1.12
18:27:55
```

Top left corner contains an indicator for PPS pulses. Next to that is the current number of satellites *used* by the GPS module. To the right of that is the current measured PPB error.

Bottom line is the current UTC time from GPS module.

Rotating the encoder one step will switch to showing the full PPB error (times 100 for reasons) and the current PWM output.

The firmware uses an LCD library from https://www.github.com/NimaLTD and I would have loved to just use it as a submodule, but I have to edit it slightly.

### USB

It would be nice to have NMEA output over USB, and the Blue Pill dev board in the GPSDO does have a USB connector. It's however difficult to use since it requires a PLLCLK of 48MHz. But since we use 10MHz as input instead of 8MHz this can't be achieved. It should be possible to run the HSI to the PLL and then run the USB off of that. Then run the HSE directly to the peripherals. But then the timers would be running at 10MHz and I don't know how that would affect the rest of it.
