/*
 * utility.h
 *
 *  Created on: Oct 31, 2014
 *      Author: Wenhao
 */

#ifndef UTILITY_H_
#define UTILITY_H_

 void moveBar(uint8_t steps, uint8_t dir);
 void drawOled(uint8_t joyState);
#define NOTE_PIN_HIGH() GPIO_SetValue(0, 1<<26);
#define NOTE_PIN_LOW()  GPIO_ClearValue(0, 1<<26);
 void playNote(uint32_t note, uint32_t durationMs);
 uint32_t getNote(uint8_t ch);

 uint32_t getDuration(uint8_t ch);
 uint32_t getPause(uint8_t ch);
 void playSong(uint8_t *song);

 void init_ssp(void);
 void init_i2c(void);
 void init_GPIO(void);
void SysTick_Handler(void);
uint32_t getTicks(void);
void EINT3_IRQHandler(void);
void EINT0_IRQHandler(void);
void check_failed(uint8_t *file, uint32_t line);

#endif /* UTILITY_H_ */
