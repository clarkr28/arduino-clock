# arduino-clock
Code for an arduino-based clock that uses 72 RBG LEDs to display the time. 

### Motivation

I had a clock that I wanted to put in my office, but I didn't want to be bothered by the ticking noise that it 
would make. So, I replaced the clock hands with two rings of RBGW LEDs. The inner ring has 12 LEDs to show
the hour, and the outer ring has 60 LEDs to show the minutes and seconds. The microcontroller uses WiFi to 
determine the time on startup (and resync the time every so often because microcontrollers are pretty bad at
keeping time).  

### Basic Parts
- [ESP32 Board](https://www.adafruit.com/product/3591)
- [Small NeoPixel LED ring](https://www.adafruit.com/product/2852)
- [Large NeoPixel LED 1/4 ring](https://www.adafruit.com/product/2874)
- [Rotary Dial](https://www.adafruit.com/product/377) and [Rotary Encoder](https://www.adafruit.com/product/4991)

### Features
- [x] Be able to display the current time
- [x] Wrapper for time functions to account for the microcontroller's inaccuracy
- [x] Change the seconds and minutes display to a smooth animation
- [ ] Animation to play when the hour changes
- [ ] Utilize multiple wifi connection options
- [ ] Use a background process to sync time instead of waiting in the foreground
- [ ] Second microcontroller with rotary encoder to set timers
