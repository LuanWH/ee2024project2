/*
 * utility.c
 *
 *  Created on: Oct 31, 2014
 *      Author: Wenhao
 */
#include <stdio.h>
#include "lpc17xx_pinsel.h"
#include "lpc17xx_gpio.h"
#include "lpc17xx_i2c.h"
#include "lpc17xx_ssp.h"
#include "lpc17xx_timer.h"

#include "joystick.h"
#include "pca9532.h"
#include "acc.h"
#include "oled.h"
#include "rgb.h"
#include "led7seg.h"

#include "light.h"
#include "temp.h"

#include "utility.h"

 uint32_t notes[] = {
        2272, // A - 440 Hz
        2024, // B - 494 Hz
        3816, // C - 262 Hz
        3401, // D - 294 Hz
        3030, // E - 330 Hz
        2865, // F - 349 Hz
        2551, // G - 392 Hz
        1136, // a - 880 Hz
        1012, // b - 988 Hz
        1912, // c - 523 Hz
        1703, // d - 587 Hz
        1517, // e - 659 Hz
        1432, // f - 698 Hz
        1275, // g - 784 Hz
};
uint32_t msTicks = 0;
uint8_t barPos = 2;
 void moveBar(uint8_t steps, uint8_t dir)
{
    uint16_t ledOn = 0;

    if (barPos == 0)
        ledOn = (1 << 0) | (3 << 14);
    else if (barPos == 1)
        ledOn = (3 << 0) | (1 << 15);
    else
        ledOn = 0x07 << (barPos-2);

    barPos += (dir*steps);
    barPos = (barPos % 16);

    pca9532_setLeds(ledOn, 0xffff);
}


 void drawOled(uint8_t joyState)
{
     int wait = 0;
     uint8_t currX = 48;
     uint8_t currY = 32;
     uint8_t lastX = 0;
     uint8_t lastY = 0;

    if ((joyState & JOYSTICK_CENTER) != 0) {
        oled_clearScreen(OLED_COLOR_BLACK);
        return;
    }

    if (wait++ < 3)
        return;

    wait = 0;

    if ((joyState & JOYSTICK_UP) != 0 && currY > 0) {
        currY--;
    }

    if ((joyState & JOYSTICK_DOWN) != 0 && currY < OLED_DISPLAY_HEIGHT-1) {
        currY++;
    }

    if ((joyState & JOYSTICK_RIGHT) != 0 && currX < OLED_DISPLAY_WIDTH-1) {
        currX++;
    }

    if ((joyState & JOYSTICK_LEFT) != 0 && currX > 0) {
        currX--;
    }

    if (lastX != currX || lastY != currY) {
        oled_putPixel(currX, currY, OLED_COLOR_WHITE);
        lastX = currX;
        lastY = currY;
    }
}




 void playNote(uint32_t note, uint32_t durationMs) {

    uint32_t t = 0;

    if (note > 0) {

        while (t < (durationMs*1000)) {
            NOTE_PIN_HIGH();
            Timer0_us_Wait(note / 2);
            //delay32Us(0, note / 2);

            NOTE_PIN_LOW();
            Timer0_us_Wait(note / 2);
            //delay32Us(0, note / 2);

            t += note;
        }

    }
    else {
    	Timer0_Wait(durationMs);
        //delay32Ms(0, durationMs);
    }
}

 uint32_t getNote(uint8_t ch)
{
    if (ch >= 'A' && ch <= 'G')
        return notes[ch - 'A'];

    if (ch >= 'a' && ch <= 'g')
        return notes[ch - 'a' + 7];

    return 0;
}

 uint32_t getDuration(uint8_t ch)
{
    if (ch < '0' || ch > '9')
        return 400;

    /* number of ms */

    return (ch - '0') * 200;
}

 uint32_t getPause(uint8_t ch)
{
    switch (ch) {
    case '+':
        return 0;
    case ',':
        return 5;
    case '.':
        return 20;
    case '_':
        return 30;
    default:
        return 5;
    }
}

 void playSong(uint8_t *song) {
    uint32_t note = 0;
    uint32_t dur  = 0;
    uint32_t pause = 0;

    /*
     * A song is a collection of tones where each tone is
     * a note, duration and pause, e.g.
     *
     * "E2,F4,"
     */

    while(*song != '\0') {
        note = getNote(*song++);
        if (*song == '\0')
            break;
        dur  = getDuration(*song++);
        if (*song == '\0')
            break;
        pause = getPause(*song++);

        playNote(note, dur);
        //delay32Ms(0, pause);
        Timer0_Wait(pause);

    }
}

 void init_ssp(void)
{
	SSP_CFG_Type SSP_ConfigStruct;
	PINSEL_CFG_Type PinCfg;

	/*
	 * Initialize SPI pin connect
	 * P0.7 - SCK;
	 * P0.8 - MISO
	 * P0.9 - MOSI
	 * P2.2 - SSEL - used as GPIO
	 */
	PinCfg.Funcnum = 2;
	PinCfg.OpenDrain = 0;
	PinCfg.Pinmode = 0;
	PinCfg.Portnum = 0;
	PinCfg.Pinnum = 7;
	PINSEL_ConfigPin(&PinCfg);
	PinCfg.Pinnum = 8;
	PINSEL_ConfigPin(&PinCfg);
	PinCfg.Pinnum = 9;
	PINSEL_ConfigPin(&PinCfg);
	PinCfg.Funcnum = 0;
	PinCfg.Portnum = 2;
	PinCfg.Pinnum = 2;
	PINSEL_ConfigPin(&PinCfg);

	SSP_ConfigStructInit(&SSP_ConfigStruct);

	// Initialize SSP peripheral with parameter given in structure above
	SSP_Init(LPC_SSP1, &SSP_ConfigStruct);

	// Enable SSP peripheral
	SSP_Cmd(LPC_SSP1, ENABLE);

}

 void init_i2c(void)
{
	PINSEL_CFG_Type PinCfg;

	/* Initialize I2C2 pin connect */
	PinCfg.Funcnum = 2;
	PinCfg.Pinnum = 10;
	PinCfg.Portnum = 0;
	PINSEL_ConfigPin(&PinCfg);
	PinCfg.Pinnum = 11;
	PINSEL_ConfigPin(&PinCfg);

	// Initialize I2C peripheral
	I2C_Init(LPC_I2C2, 100000);

	/* Enable I2C1 operation */
	I2C_Cmd(LPC_I2C2, ENABLE);
}

 void init_GPIO(void)
{
	PINSEL_CFG_Type PinCfg;
	PinCfg.Funcnum = 0;
	PinCfg.Pinnum = 5;
	PinCfg.Portnum = 2;
	PINSEL_ConfigPin(&PinCfg);
	PinCfg.Pinnum = 10;
	PinCfg.Funcnum = 1;
	PINSEL_ConfigPin(&PinCfg);
	GPIO_SetDir(2, 1 << 5, 0);
	//GPIO_SetDir(2, 1<<10, 0);
	NVIC_EnableIRQ(EINT3_IRQn);
	NVIC_EnableIRQ(EINT0_IRQn);
	//LPC_GPIOINT->IO2IntEnF |= 1 << 10;
	LPC_GPIOINT->IO2IntEnF |= 1 << 5;
}

void SysTick_Handler(void){
	msTicks++;
}

uint32_t getTicks(void){
	return msTicks;
}


void EINT3_IRQHandler(void){
	if(LPC_GPIOINT->IO2IntStatF >> 10 & 0x01){
		printf("In SW3 triggered interrupt. EINT3.\n");
		LPC_GPIOINT->IO2IntClr = 1 << 10;
	}
	if(LPC_GPIOINT->IO2IntStatF >> 5 & 0x01){
		printf("In light sensor triggered interrupt.\n");
		LPC_GPIOINT->IO2IntClr = 1 << 5;
		light_clearIrqStatus();
	}
}

void EINT0_IRQHandler(void){
	printf("In SW3 triggered interrupt. EINT0.\n");
	LPC_SC->EXTINT = 0b001;
}

void check_failed(uint8_t *file, uint32_t line)
{
	/* User can add his own implementation to report the file name and line number,
	 ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */

	/* Infinite loop */
	while(1);
}

