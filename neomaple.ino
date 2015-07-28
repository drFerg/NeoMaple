/* 0xWS2812 16-Channel WS2812 interface library
 * 
 * Copyright (c) 2014 Elia Ritterbusch, http://eliaselectronics.com
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

//#include <stm32f10x.h>
#include "libmaple/gpio.h"
#include "libmaple/dma.h"
/* this define sets the number of TIM2 overflows
 * to append to the data frame for the LEDs to 
 * load the received data into their registers */
#define WS2812_DEADPERIOD 19
#define LED_PIN 33
uint16_t WS2812_IO_High = 0xFFFF;
uint16_t WS2812_IO_Low = 0x0000;

volatile uint8_t WS2812_TC = 1;
volatile uint8_t TIM2_overflows = 0;

/* WS2812 framebuffer
 * buffersize = (#LEDs / 16) * 24 */
uint16_t WS2812_IO_framedata[19 * 24];

/* Array defining 12 color triplets to be displayed */
uint8_t colors[12][3] = 
{
  {0xFF, 0x00, 0x00},
  {0xFF, 0x80, 0x00},
  {0xFF, 0xFF, 0x00},
  {0x80, 0xFF, 0x00},
  {0x00, 0xFF, 0x00},
  {0x00, 0xFF, 0x80},
  {0x00, 0xFF, 0xFF},
  {0x00, 0x80, 0xFF},
  {0x00, 0x00, 0xFF},
  {0x80, 0x00, 0xFF},
  {0xFF, 0x00, 0xFF},
  {0xFF, 0x00, 0x80}
};

void GPIO_init(void)
{
  // GPIOA Periph clock enable
  rcc_clk_enable(RCC_GPIOA);
  // GPIOA pins WS2812 data outputs
  gpio_set_mode(GPIOA, 0, GPIO_OUTPUT_PP);

}

void TIM2_init() {
  uint32_t SystemCoreClock = 72000000;
  uint16_t prescalerValue = (uint16_t) (SystemCoreClock / 24000000) - 1;
  rcc_clk_enable(RCC_TIMER2);
  /* Time base configuration */
  timer_pause(TIMER2);
  timer_set_prescaler(TIMER2, prescalerValue);
  timer_set_reload(TIMER2, 29); // 800kHz
  
  /* Timing Mode configuration: Channel 1 */
  timer_set_mode(TIMER2, 1, TIMER_OUTPUT_COMPARE);
  timer_set_compare(TIMER2, 1, 8);
  timer_oc_set_mode(TIMER2, 1, TIMER_OC_MODE_FROZEN, ~TIMER_OC_PE);

  /* Timing Mode configuration: Channel 2 */
  timer_set_mode(TIMER2, 2, TIMER_OUTPUT_COMPARE);
  timer_set_compare(TIMER2, 2, 17);
  timer_oc_set_mode(TIMER2, 2, TIMER_OC_MODE_PWM_1, ~TIMER_OC_PE);
  //timer_resume(TIMER2);


  timer_attach_interrupt(TIMER2, TIMER_UPDATE_INTERRUPT, TIM2_IRQHandler);
  /* configure TIM2 interrupt */
  nvic_irq_set_priority(NVIC_TIMER2, 2);
  nvic_irq_enable(NVIC_TIMER2);
}

void DMA_init(void) {
  dma_init(DMA1);
  // TIM2 Update event
  /* DMA1 Channel2 configuration ----------------------------------------------*/
  dma_setup_transfer(DMA1, DMA_CH2, (volatile void*) &(GPIOA->regs->ODR), DMA_SIZE_32BITS, 
                                    (volatile void*) &(WS2812_IO_High), DMA_SIZE_16BITS, DMA_FROM_MEM);
  dma_set_priority(DMA1, DMA_CH2, DMA_PRIORITY_HIGH);

  // TIM2 CC1 event
  /* DMA1 Channel5 configuration ----------------------------------------------*/
  dma_setup_transfer(DMA1, DMA_CH5, (volatile void*) &(GPIOA->regs->ODR), DMA_SIZE_32BITS, 
                                    (volatile void*) &(WS2812_IO_framedata), DMA_SIZE_16BITS, DMA_FROM_MEM | DMA_MINC_MODE);
  dma_set_priority(DMA1, DMA_CH5, DMA_PRIORITY_HIGH);

  
  // TIM2 CC2 event
  /* DMA1 Channel7 configuration ----------------------------------------------*/
  dma_setup_transfer(DMA1, DMA_CH7, (volatile void*) &(GPIOA->regs->ODR), DMA_SIZE_32BITS, 
                                    (volatile void*) &(WS2812_IO_Low), DMA_SIZE_16BITS, DMA_FROM_MEM | DMA_TRNS_CMPLT);
  dma_set_priority(DMA1, DMA_CH7, DMA_PRIORITY_HIGH);


  /* configure DMA1 Channel7 interrupt */
  nvic_irq_set_priority(NVIC_DMA_CH7, 1);
  nvic_irq_enable(NVIC_DMA_CH7);
  dma_attach_interrupt(DMA1, DMA_CH7, DMA1_Channel7_IRQHandler);
  /* enable DMA1 Channel7 transfer complete interrupt */
}

/* Transmit the frambuffer with buffersize number of bytes to the LEDs 
 * buffersize = (#LEDs / 16) * 24 */
void WS2812_sendbuf(uint32_t buffersize)
{   
  // transmission complete flag, indicate that transmission is taking place
  WS2812_TC = 0;
  
  // clear all relevant DMA flags
  dma_clear_isr_bits(DMA1, DMA_CH2);
  dma_clear_isr_bits(DMA1, DMA_CH5);
  dma_clear_isr_bits(DMA1, DMA_CH7);

  // configure the number of bytes to be transferred by the DMA controller
  dma_set_num_transfers(DMA1, DMA_CH2, buffersize);
  dma_set_num_transfers(DMA1, DMA_CH5, buffersize);
  dma_set_num_transfers(DMA1, DMA_CH7, buffersize);

  // clear all TIM2 flags
  TIMER2->regs.gen->SR = 0;
  
  // enable the corresponding DMA channels
  dma_enable(DMA1, DMA_CH2);
  dma_enable(DMA1, DMA_CH5);
  dma_enable(DMA1, DMA_CH7);
  // IMPORTANT: enable the TIM2 DMA requests AFTER enabling the DMA channels!
  timer_dma_enable_req(TIMER2, 1);
  timer_dma_enable_req(TIMER2, 2);
  timer_dma_enable_req(TIMER2, 0); /* TIM_DMA_Update */
  
  // preload counter with 29 so TIM2 generates UEV directly to start DMA transfer
  timer_set_count(TIMER2, 29);
  
  // start TIM2
  timer_resume(TIMER2);
}

/* DMA1 Channel7 Interrupt Handler gets executed once the complete framebuffer has been transmitted to the LEDs */
void DMA1_Channel7_IRQHandler(void)
{
  // clear DMA7 transfer complete interrupt flag
  dma_clear_isr_bits(DMA1, DMA_CH7); 
  // enable TIM2 Update interrupt to append 50us dead period
  timer_enable_irq(TIMER2, TIMER_UPDATE_INTERRUPT);
  // disable the DMA channels
  dma_disable(DMA1, DMA_CH2);
  dma_disable(DMA1, DMA_CH5);
  dma_disable(DMA1, DMA_CH7);
  // IMPORTANT: disable the DMA requests, too!
  timer_dma_disable_req(TIMER2, 1);
  timer_dma_disable_req(TIMER2, 2);  
  timer_dma_disable_req(TIMER2, 0); /* TIM_DMA_Update */
}

/* TIM2 Interrupt Handler gets executed on every TIM2 Update if enabled */
void TIM2_IRQHandler(void)
{
  // Clear TIM2 Interrupt Flag
  TIMER2->regs.gen->SR &= ~TIMER_SR_UIF;
  /* check if certain number of overflows has occured yet 
   * this ISR is used to guarantee a 50us dead time on the data lines
   * before another frame is transmitted */
  if (TIM2_overflows < (uint8_t)WS2812_DEADPERIOD)
  {
    // count the number of occured overflows
    TIM2_overflows++;
  }
  else
  {
    // clear the number of overflows
    TIM2_overflows = 0; 
    // stop TIM2 now because dead period has been reached
    timer_pause(TIMER2);
    /* disable the TIM2 Update interrupt again 
     * so it doesn't occur while transmitting data */
    timer_disable_irq(TIMER2, TIMER_UPDATE_INTERRUPT);
    // finally indicate that the data frame has been transmitted
    WS2812_TC = 1;  
  }
}

/* This function sets the color of a single pixel in the framebuffer 
 * 
 * Arguments:
 * row = the channel number/LED strip the pixel is in from 0 to 15
 * column = the column/LED position in the LED string from 0 to number of LEDs per strip
 * red, green, blue = the RGB color triplet that the pixel should display 
 */
void WS2812_framedata_setPixel(uint8_t row, uint16_t column, uint8_t red, uint8_t green, uint8_t blue)
{
  uint8_t i;
  for (i = 0; i < 8; i++)
  {
    // clear the data for pixel 
    // write new data for pixel
    WS2812_IO_framedata[((column*24)+i)] = ((((green<<i) & 0x80)>>7));
    WS2812_IO_framedata[((column*24)+8+i)] = ((((red<<i) & 0x80)>>7));
    WS2812_IO_framedata[((column*24)+16+i)] = ((((blue<<i) & 0x80)>>7));
  }
}

/* This function is a wrapper function to set all LEDs in the complete row to the specified color
 * 
 * Arguments:
 * row = the channel number/LED strip to set the color of from 0 to 15
 * columns = the number of LEDs in the strip to set to the color from 0 to number of LEDs per strip
 * red, green, blue = the RGB color triplet that the pixels should display 
 */
void WS2812_framedata_setRow(uint8_t row, uint16_t columns, uint8_t red, uint8_t green, uint8_t blue)
{
  uint8_t i;
  for (i = 0; i < columns; i++)
  {
    WS2812_framedata_setPixel(row, i, red, green, blue);
  }
}

/* This function is a wrapper function to set all the LEDs in the column to the specified color
 * 
 * Arguments:
 * rows = the number of channels/LED strips to set the row in from 0 to 15
 * column = the column/LED position in the LED string from 0 to number of LEDs per strip
 * red, green, blue = the RGB color triplet that the pixels should display 
 */
void WS2812_framedata_setColumn(uint8_t rows, uint16_t column, uint8_t red, uint8_t green, uint8_t blue)
{
  uint8_t i;
  for (i = 0; i < rows; i++)
  {
    WS2812_framedata_setPixel(i, column, red, green, blue);
  }
}

void setup() { 
  GPIO_init();
  DMA_init();
  TIM2_init();
  memset(WS2812_IO_framedata, 0, sizeof(WS2812_IO_framedata));
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH);
}

void loop() {
  uint8_t i;
  // set two pixels (columns) in the defined row (channel 0) to the
  // color values defined in the colors array
  for (i = 0; i < 12; i++)
  {
    Serial.println(i);
    // wait until the last frame was transmitted
    while(!WS2812_TC);
    // this approach sets each pixel individually
    for (int j = 0; j < 19; j++){
      WS2812_framedata_setPixel(0, j, colors[i][0], colors[i][1], colors[i][2]);
    }
    // this funtion is a wrapper and achieved the same thing, tidies up the code
    //WS2812_framedata_setRow(0, 6, colors[i][0], colors[i][1], colors[i][2]);
    // send the framebuffer out to the LEDs
    WS2812_sendbuf(19*24);
    // wait some amount of time
    delay(500);
    }
}



