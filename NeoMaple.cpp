/*-------------------------------------------------------------------------
  A STM32Duino library to control a wide variety of WS2811- and WS2812-based RGB
  LED devices such as Adafruit FLORA RGB Smart Pixels and NeoPixel strips.
  This library supports the Maple family and other STM32 based devices running
  STM32Duino, based off of LeafLabs Arduino port.
  
  STM32duino port by Fergus Leahy.
  Originally written by Phil Burgess / Paint Your Dragon for Adafruit Industries,
  contributions by PJRC, Michael Miller and other members of the open
  source community.

  Adafruit invests time and resources providing this open source code,
  please support Adafruit and open-source hardware by purchasing products
  from Adafruit!

  -------------------------------------------------------------------------
  This file is part of the NeoMaple library.

  NeoMaple is free software: you can redistribute it and/or modify
  it under the terms of the GNU Lesser General Public License as
  published by the Free Software Foundation, either version 3 of
  the License, or (at your option) any later version.

  NeoMaple is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with NeoMaple.  If not, see
  <http://www.gnu.org/licenses/>.
  -------------------------------------------------------------------------*/

#include "NeoMaple.h"
#include "neomaple_hardware.h"

// Constructor when length, pin and type are known at compile-time:
NeoMaple::NeoMaple(uint16_t n, uint8_t t) :
  brightness(0), pixels(NULL), begun(false)
{
  updateLength(n);
  updateType(t);
}

// via Michael Vogt/neophob: empty constructor is used when strand length
// isn't known at compile-time; situations where program config might be
// read from internal flash memory or an SD card, or arrive via serial
// command.  If using this constructor, MUST follow up with updateLength(),
// etc. to establish the strand length, type and pin number!
NeoMaple::NeoMaple() :
  brightness(0), pixels(NULL), begun(false),
  numLEDs(0), numBytes(0), type(NEO_GRB + NEO_KHZ800) { }

NeoMaple::~NeoMaple() {
  if(pixels)   free(pixels);
}

void NeoMaple::begin(void) {
  begun = true;
  neomaple_hard_init(pixels);
}

void NeoMaple::updateLength(uint16_t n) {
  if(pixels) free(pixels); // Free existing data (if any)

  // Allocate new data -- note: ALL PIXELS ARE CLEARED
  numBytes = n * 24;
  if((pixels = (uint8_t *)malloc(numBytes))) {
    memset(pixels, 0, numBytes);
    numLEDs = n;
  } else {
    numLEDs = numBytes = 0;
  }
}

void NeoMaple::updateType(uint8_t t) {
  type = t;
  if(t & NEO_GRB) { // GRB vs RGB; might add others if needed
    rOffset = 1;
    gOffset = 0;
    bOffset = 2;
  } else if (t & NEO_BRG) {
    rOffset = 1;
    gOffset = 2;
    bOffset = 0;
  } else if (t & NEO_RBG) {
    rOffset = 0;
    gOffset = 2;
    bOffset = 1;    
  } else {
    rOffset = 0;
    gOffset = 1;
    bOffset = 2;
  }
}


void NeoMaple::show(void) {
  if(!pixels) return;
  neomaple_hard_send(pixels, numBytes);
}

// Set pixel color from separate R,G,B components:
void NeoMaple::setPixelColor(
 uint16_t n, uint8_t r, uint8_t g, uint8_t b) {
  if(n < numLEDs) {
    if(brightness) { // See notes in setBrightness()
      r = (r * brightness) >> 8;
      g = (g * brightness) >> 8;
      b = (b * brightness) >> 8;
    }
    /* Split each bit into a byte to transfer */
    uint8_t i;
    for (i = 0; i < 8; i++) {
      uint32_t offset = (n * 24) + i;
      pixels[offset] = (((g << i) & 0x80) >> 7);
      pixels[offset + 8] = (((r << i) & 0x80) >> 7);
      pixels[offset + 16] = (((b << i) & 0x80) >> 7);
    }
  }
}

// Set pixel color from 'packed' 32-bit RGB color:
void NeoMaple::setPixelColor(uint16_t n, uint32_t c) {
  if(n < numLEDs) {
    uint8_t
      r = (uint8_t)(c >> 16),
      g = (uint8_t)(c >>  8),
      b = (uint8_t)c;
    setPixelColor(n, r, g, b);
  }
}

// Convert separate R,G,B into packed 32-bit RGB color.
// Packed format is always RGB, regardless of LED strand color order.
uint32_t NeoMaple::Color(uint8_t r, uint8_t g, uint8_t b) {
  return ((uint32_t)r << 16) | ((uint32_t)g <<  8) | b;
}

// Query color from previously-set pixel (returns packed 32-bit RGB value)
uint32_t NeoMaple::getPixelColor(uint16_t n) const {
  if(n >= numLEDs) {
    // Out of bounds, return no color.
    return 0;
  }
  uint8_t *p = &pixels[n * 3];
  uint32_t c = ((uint32_t)p[rOffset] << 16) |
               ((uint32_t)p[gOffset] <<  8) |
                (uint32_t)p[bOffset];
  // Adjust this back up to the true color, as setting a pixel color will
  // scale it back down again.
  if(brightness) { // See notes in setBrightness()
    //Cast the color to a byte array
    uint8_t * c_ptr = reinterpret_cast<uint8_t*>(&c);
    c_ptr[0] = (c_ptr[0] << 8)/brightness;
    c_ptr[1] = (c_ptr[1] << 8)/brightness;
    c_ptr[2] = (c_ptr[2] << 8)/brightness;
  }
  return c; // Pixel # is out of bounds
}

// Returns pointer to pixels[] array.  Pixel data is stored in device-
// native format and is not translated here.  Application will need to be
// aware whether pixels are RGB vs. GRB and handle colors appropriately.
uint8_t *NeoMaple::getPixels(void) const {
  return pixels;
}

uint16_t NeoMaple::numPixels(void) const {
  return numLEDs;
}

// Adjust output brightness; 0=darkest (off), 255=brightest.  This does
// NOT immediately affect what's currently displayed on the LEDs.  The
// next call to show() will refresh the LEDs at this level.  However,
// this process is potentially "lossy," especially when increasing
// brightness.  The tight timing in the WS2811/WS2812 code means there
// aren't enough free cycles to perform this scaling on the fly as data
// is issued.  So we make a pass through the existing color data in RAM
// and scale it (subsequent graphics commands also work at this
// brightness level).  If there's a significant step up in brightness,
// the limited number of steps (quantization) in the old data will be
// quite visible in the re-scaled version.  For a non-destructive
// change, you'll need to re-render the full strip data.  C'est la vie.
void NeoMaple::setBrightness(uint8_t b) {
  // Stored brightness value is different than what's passed.
  // This simplifies the actual scaling math later, allowing a fast
  // 8x8-bit multiply and taking the MSB.  'brightness' is a uint8_t,
  // adding 1 here may (intentionally) roll over...so 0 = max brightness
  // (color values are interpreted literally; no scaling), 1 = min
  // brightness (off), 255 = just below max brightness.
  uint8_t newBrightness = b + 1;
  if(newBrightness != brightness) { // Compare against prior value
    // Brightness has changed -- re-scale existing data in RAM
    uint8_t  c,
            *ptr           = pixels,
             oldBrightness = brightness - 1; // De-wrap old brightness value
    uint16_t scale;
    if(oldBrightness == 0) scale = 0; // Avoid /0
    else if(b == 255) scale = 65535 / oldBrightness;
    else scale = (((uint16_t)newBrightness << 8) - 1) / oldBrightness;
    for(uint16_t i=0; i<numBytes; i++) {
      c      = *ptr;
      *ptr++ = (c * scale) >> 8;
    }
    brightness = newBrightness;
  }
}

//Return the brightness value
uint8_t NeoMaple::getBrightness(void) const {
  return brightness - 1;
}

void NeoMaple::clear() {
  memset(pixels, 0, numBytes);
}
