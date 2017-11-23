NeoMaple
========
A Neopixel (WS2811/12) library for the maple microcontroller family running STM32Duino (Arduino for STM32 devices). This library is a port of Elia Ritterbusch's work and the Adafruit Neopixel library.

The library supports controlling WS2812 based LED chips and the awesome Adafruit NeoPixel strips and rings.

Usage
-----

The current library is tied to Timer2 pin D11 (PA0) for the output signal. Simple hook it up and specify the number of LEDS and away you go.

It currently uses a big chunk of RAM per LED (24Bytes), which will hopefully be reduced in the future.






Links
-----
STM32Duino: https://github.com/rogerclarkmelbourne/Arduino_STM32

Adafruit NeoPixel Library: https://github.com/adafruit/Adafruit_NeoPixel

Elia Ritterbusch 0xWS2812: https://github.com/devthrash/0xWS2812
