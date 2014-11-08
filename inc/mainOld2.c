#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>

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


#define NOTE_PIN_HIGH() GPIO_SetValue(0, 1<<26);
#define NOTE_PIN_LOW()  GPIO_ClearValue(0, 1<<26);

#define DEBUG 1


/*#############Constants##############*/
static uint32_t notes[] = {
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
static uint8_t * song = (uint8_t*)"C2.C2,D4,C4,F4,E8,";
/*####################################*/


/*#########Global Variable############*/
static uint8_t barPos = 2;
uint32_t msTicks = 0;
uint32_t msCount = 0;
volatile uint8_t is500 = 1;
volatile uint8_t is50 = 1;
volatile uint8_t isSW3Pressed = 0;

clock_t lastTime;
int32_t lastTemp=0;
uint16_t lastLight=0;

int RESET_INITIAL = 1;
int INITIAL_TIME = 0;
int CURRENT_TIME = 0;
uint8_t SEGMENT_DISPLAY = 0;

int32_t xoff = 0;
int32_t yoff = 0;
int32_t zoff = 0;

int8_t x = 0;
int8_t y = 0;
int8_t z = 0;
/*####################################*/

static void moveBar(uint8_t steps, uint8_t dir)
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

static void drawOled(uint8_t joyState)
{
    static int wait = 0;
    static uint8_t currX = 48;
    static uint8_t currY = 32;
    static uint8_t lastX = 0;
    static uint8_t lastY = 0;

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

static void playNote(uint32_t note, uint32_t durationMs) {

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

static uint32_t getNote(uint8_t ch)
{
    if (ch >= 'A' && ch <= 'G')
        return notes[ch - 'A'];

    if (ch >= 'a' && ch <= 'g')
        return notes[ch - 'a' + 7];

    return 0;
}

static uint32_t getDuration(uint8_t ch)
{
    if (ch < '0' || ch > '9')
        return 400;

    /* number of ms */

    return (ch - '0') * 200;
}

static uint32_t getPause(uint8_t ch)
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

static void playSong(uint8_t *song) {
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

static void init_ssp(void)
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

static void init_i2c(void)
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

static void init_Light_For_GPIO(void)
{
	PINSEL_CFG_Type PinCfg;
	PinCfg.Funcnum = 0;
	PinCfg.Pinnum = 5;
	PinCfg.Portnum = 2;
	PINSEL_ConfigPin(&PinCfg); //Config Light Sensor as EINT3 input
	GPIO_SetDir(2, 1 << 5, 0);
}

static void config_EINT3(void){
	NVIC_EnableIRQ(EINT3_IRQn); //[Enabled] Use EINT3
	LPC_GPIOINT->IO2IntEnF |= 1 << 5;
}

static void config_EINT0(void){
	NVIC_EnableIRQ(EINT0_IRQn); //[Enabled] Use EINT0
}

static void init_SW3(void){
	PINSEL_CFG_Type PinCfg;
	PinCfg.Funcnum = 1;	 //Config SW3 Push Button as EINT0 input
	PinCfg.Portnum = 2;
	PinCfg.Pinnum = 10;
	PINSEL_ConfigPin(&PinCfg);
}

static void init_SW4(void){
	PINSEL_CFG_Type PinCfg;
	PinCfg.Funcnum = 0;	 //Config SW4 Push Button as GPIO
	PinCfg.Portnum = 1;
	PinCfg.Pinnum = 31;
	PinCfg.OpenDrain = 0;
	PinCfg.Pinmode = 0;
	PINSEL_ConfigPin(&PinCfg);
	GPIO_SetDir(1, 1 << 31, 0);
}

void increment7Seg(void){
	if(SEGMENT_DISPLAY == 9){
		SEGMENT_DISPLAY = 0;
	} else {
		SEGMENT_DISPLAY++;
	}

	led7seg_setChar(48+SEGMENT_DISPLAY, 0);
}

void readAccWithOffset(int8_t * x1,int8_t * y1,int8_t * z1){
	acc_read(x1, y1, z1);
    *x1 = *x1+xoff;
    *y1 = *y1+yoff;
    *z1 = *z1+zoff;
}

void drawOLED(uint16_t light_val, int32_t temp_val){
	unsigned char printLight[50];
	unsigned char printTemp[50];
	unsigned char printAcc0[] = "Acceleration:";
	unsigned char printAcc1[50];
	sprintf(printLight,"Light: %u", light_val);
	sprintf(printTemp, "Temperature: %d", temp_val);
	sprintf(printAcc1, "  x: %d, y: %d, z: %d", (int)x, (int)y, (int)z);
	printf("%s\n", printLight);
	printf("%s\n", printTemp);
	oled_clearScreen(OLED_COLOR_WHITE);
	oled_putString(1, 0, printLight,OLED_COLOR_WHITE, OLED_COLOR_BLACK);
	oled_putString(1,10, printTemp, OLED_COLOR_WHITE, OLED_COLOR_BLACK);
	oled_putString(1,20, printAcc0, OLED_COLOR_WHITE, OLED_COLOR_BLACK);
	oled_putString(1,30, printAcc1, OLED_COLOR_WHITE, OLED_COLOR_BLACK);
	Timer0_Wait(1);
}

void updateOLEDWithTemp(void){
	uint16_t light_val = light_read();
	int32_t temp_val = temp_read();
	readAccWithOffset(&x, &y, &z);
	printf("updateOLEDWithTemp, %d, %u\n", light_val, temp_val);
	drawOLED(light_val, temp_val);
	lastTemp = temp_val;

}
void updateOLEDWithoutTemp(void){
	uint16_t light_val = light_read();
	readAccWithOffset(&x, &y, &z);
	printf("updateOLEDWithoutTemp, %d, %d\n", light_val, lastTemp);
	drawOLED(light_val, lastTemp);
}


void SysTick_Handler(void){
	msTicks++;
	msCount++;
	if(msCount % 999 == 0){
		clock_t t = clock();
		float diff = (((float)t - (float)lastTime) / 1000000.0F ) * 1000;
		printf("Now msCount is %u, system time elapsed %f \n", msCount, diff);
		lastTime = t;
		increment7Seg();
	}

	if(msCount % 499 == 0){
		is500 = 1;
	}else if(msCount %49 == 0){
		is50 = 1;
	}
}

uint32_t getTicks(void){
	return msTicks;
}

void EINT3_IRQHandler(void){

// [Disabled] SW3 as GPIO Interrupt. Please refer to EINT0.
/*	if(LPC_GPIOINT->IO2IntStatF >> 10 & 0x01){
		if(DEBUG){
			printf("In SW3 triggered interrupt. EINT3.\n");
		}
		LPC_GPIOINT->IO2IntClr = 1 << 10;
	}*/

	if(LPC_GPIOINT->IO2IntStatF >> 5 & 0x01){
		if(DEBUG){
			printf("In light sensor triggered interrupt.\n");
		}
		LPC_GPIOINT->IO2IntClr = 1 << 5;
		light_clearIrqStatus();
	}

}

void EINT0_IRQHandler(void){
	if(DEBUG){
		printf("In SW3 triggered interrupt. EINT0.\n");
	}
	LPC_SC->EXTINT = 0b001;
}

int main (void) {
	lastTime = clock();
	/* ---> SYSTEM CONFIG <---*/
	//SysTick_Config (SystemCoreClock / 1000);
	/* -----------------------*/



    uint8_t dir = 1;
    uint8_t wait = 0;
    uint8_t state = 0;

    init_i2c();
    init_ssp();
    init_Light_For_GPIO();
    init_SW4();
    init_SW3();

    config_EINT3();
    config_EINT0();

    pca9532_init();
    joystick_init();
    acc_init();
    oled_init();
    light_enable();
    temp_init(&getTicks);
    led7seg_init();
    rgb_init();

    /*
     * Assume base board in zero-g position when reading first value.
     */
    acc_read(&x, &y, &z);
    xoff = 0-x;
    yoff = 0-y;
    zoff = 64-z;

    /* ---- Speaker ------> */

    GPIO_SetDir(2, 1<<0, 1);
    GPIO_SetDir(2, 1<<1, 1);

    GPIO_SetDir(0, 1<<27, 1);
    GPIO_SetDir(0, 1<<28, 1);
    GPIO_SetDir(2, 1<<13, 1);
    GPIO_SetDir(0, 1<<26, 1);

    GPIO_ClearValue(0, 1<<27); //LM4811-clk
    GPIO_ClearValue(0, 1<<28); //LM4811-up/dn
    GPIO_ClearValue(2, 1<<13); //LM4811-shutdn

    /* <---- Speaker ------ */

    moveBar(1, dir);
    oled_clearScreen(OLED_COLOR_BLACK);

    /* <---- Project ------ */

    rgb_setLeds(RGB_RED);


    while (1)
    {

        /* ####### Accelerometer and LEDs  ###### */
        /* # */

        acc_read(&x, &y, &z);
        x = x+xoff;
        y = y+yoff;
        z = z+zoff;

        if (y < 0) {
            dir = 1;
            y = -y;
        }
        else {
            dir = -1;
        }

        if (y > 1 && wait++ > (40 / (1 + (y/10)))) {
            moveBar(1, dir);
            wait = 0;
        }


        /* # */
        /* ############################################# */


        /* ####### Joystick and OLED  ###### */
        /* # */

        state = joystick_read();
        if (state != 0)
            drawOled(state);

        /* # */
        /* ############################################# */

        /* ####### SW4  Push Button###### */
        if(!(GPIO_ReadValue(1) >> 31 & 0x01)){
        	if(DEBUG){
        		printf("In main loop reading SW4!\n");
        	}
        	GPIO_ClearValue(1, 0b01 << 31);
        }
        /* # */
        /* ############################################# */


        /* ############ Trumpet and RGB LED  ########### */
        /* # */

        //Timer0_Wait(1);

    	if(is500){
    		updateOLEDWithTemp();
    		is500 = 0;
    	} else if(is50){
    		updateOLEDWithoutTemp();
    		is50 = 0;
    	}
    	Timer0_Wait(1);
    }


}

void check_failed(uint8_t *file, uint32_t line)
{
	/* User can add his own implementation to report the file name and line number,
	 ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */

	/* Infinite loop */
	while(1);
}

