/*****************************************************************************
 *   A demo example using several of the peripherals on the base board
 *
 *   Copyright(C) 2011, EE2024
 *   All rights reserved.
 *
 ******************************************************************************/
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

#define LED7SEG_UPDATE 1000

#define ACC_ST_BRIGHT 50
#define LS_ST_BRIGHT 1000
#define TS_ST_BRIGHT 1000
#define ACC_ST_DIM 100
#define LS_ST_DIM 3000
#define TS_ST_DIM 3000

#define BRIGHT_CONDITION 800
#define N_SAMPLE 10
#define TEMP_WARN 26
#define REPORTING_TIME
#define DISTRESS_TIME

/* ---> TYPE DEFINE <--- */
typedef enum{
	REGULAR, RELAY
}Mode;

typedef enum{
	BRIGHT, DIM
}Condition;
/* --------------------- */

Mode mode = REGULAR;
Condition condition = BRIGHT;

volatile static uint32_t msTicks = 0;
volatile static uint32_t msCount = 0;

static uint8_t barPos = 2;
volatile uint32_t count1000 = 0;
volatile uint32_t count3000 = 0;
volatile uint32_t count100 = 0;
volatile uint8_t count50 = 0;
int segDisplayNumber = 0;
clock_t lastTime;
clock_t thisTime;
int32_t xoff = 0;
int32_t yoff = 0;
int32_t zoff = 0;
int8_t x = 0;
int8_t y = 0;
int8_t z = 0;

int isRgbOn = 0;

int32_t lastTemp = 0;
uint32_t lastLight = 0;
int lastVariance = 0;

int queuePosition = 0;
int queue[N_SAMPLE];




/*
 * Assume base board in zero-g position when reading first value.
 */
void resetAcc(void){
    acc_read(&x, &y, &z);
    xoff = 0-x;
    yoff = 0-y;
    zoff = 0-z;
}

void updateOledAcc(){
	oled_fillRect(30,1,95,11,OLED_COLOR_BLACK);
	char sAcc[50];
	sprintf(sAcc, "%d", lastVariance);
	oled_putString(30, 1, (unsigned char *) sAcc, OLED_COLOR_WHITE,OLED_COLOR_BLACK);
}
void updateOledLight(){
	oled_fillRect(40,12, 95, 22,OLED_COLOR_BLACK);
	char sLight[50];
	sprintf(sLight, "%u", lastLight);
	oled_putString(40, 12, (unsigned char *)sLight, OLED_COLOR_WHITE,OLED_COLOR_BLACK);
}
void updateOledTemp(){
	oled_fillRect(35, 23, 95, 33, OLED_COLOR_BLACK);
	char sTemp[50];
	sprintf(sTemp, "%.1f", lastTemp/10.0);
	oled_putString(35, 23, (unsigned char *)sTemp, OLED_COLOR_WHITE, OLED_COLOR_BLACK);
}
void updateOledMode(){
	oled_fillRect(35, 34, 95, 44, OLED_COLOR_BLACK);
	if(mode == REGULAR){
		oled_putString(35, 34, (unsigned char *)"REGULAR", OLED_COLOR_WHITE, OLED_COLOR_BLACK);
	} else {
		oled_putString(35, 34, (unsigned char *) "RELAY", OLED_COLOR_WHITE, OLED_COLOR_BLACK);
	}
	updateOledCondition();
}

void updateOledCondition(){
	oled_fillRect(35, 45, 95, 55, OLED_COLOR_BLACK);
	if(mode == REGULAR){
		if(condition == BRIGHT){
			oled_putString(35, 45, (unsigned char *) "BRIGHT", OLED_COLOR_WHITE, OLED_COLOR_BLACK);
		} else {
			oled_putString(35, 45, (unsigned char *) "DIM", OLED_COLOR_WHITE, OLED_COLOR_BLACK);
		}
	} else {
		oled_putString(35, 45, (unsigned char *) "N.A.", OLED_COLOR_WHITE, OLED_COLOR_BLACK);
	}

}
void updateOled(){
	oled_clearScreen(OLED_COLOR_BLACK);
	char sAcc[50];
	sprintf(sAcc, "Var: %d", lastVariance);
	oled_putString(1, 1, (unsigned char *) sAcc, OLED_COLOR_WHITE,OLED_COLOR_BLACK);
	char sLight[50];
	sprintf(sLight, "Light: %u", lastLight);
	oled_putString(1, 12, (unsigned char *)sLight, OLED_COLOR_WHITE,OLED_COLOR_BLACK);
	char sTemp[50];
	sprintf(sTemp, "Temp: %.1f", lastTemp/10.0);
	oled_putString(1, 23, (unsigned char *)sTemp, OLED_COLOR_WHITE, OLED_COLOR_BLACK);
	if(mode == REGULAR){
		oled_putString(1, 34, (unsigned char *)"Mode: REGULAR", OLED_COLOR_WHITE, OLED_COLOR_BLACK);
		if(condition == BRIGHT){
			oled_putString(1, 45, (unsigned char *) "Cdtn: BRIGHT", OLED_COLOR_WHITE, OLED_COLOR_BLACK);
		} else {
			oled_putString(1, 45, (unsigned char *) "Cdtn: DIM", OLED_COLOR_WHITE, OLED_COLOR_BLACK);
		}
	} else {
		oled_putString(1, 34, (unsigned char *) "Mode: RELAY", OLED_COLOR_WHITE, OLED_COLOR_BLACK);
		oled_putString(1, 45, (unsigned char *) "Cdtn: N.A.", OLED_COLOR_WHITE, OLED_COLOR_BLACK);
	}

}

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

static void init_SW4(void){
	PINSEL_CFG_Type PinCfg;
	PinCfg.Funcnum = 0;	 //Config SW4 Push Button as GPIO
	PinCfg.Portnum = 1;
	PinCfg.Pinnum = 31;
	PinCfg.OpenDrain = 0;
	PinCfg.Pinmode = 0;
	PINSEL_ConfigPin(&PinCfg);
	GPIO_SetDir(1, 1 << 31, 0);
	GPIO_ClearValue(1, 0b01 << 31);
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

int calculateAccVar(int8_t now){
	int i;
	if(queuePosition == N_SAMPLE){
		for(i = 0;i < N_SAMPLE - 1; i ++){
			queue[i] = queue[i + 1];
		}
		queue[N_SAMPLE - 1] = now;
	} else {
		queue[queuePosition++] = now;
	}
	int mean = 0;
	for(i = 0; i < queuePosition; ++i){
		mean += queue[i];
	}
	mean /= queuePosition;
	int variance = 0;
	for(i = 0; i < queuePosition;++i){
		variance += (queue[i] - mean) * (queue[i] - mean);
	}

	variance /= queuePosition;
	return variance;
}

void updateAcc(){
	acc_read(&x, &y, &z);
	z = z+zoff;
	lastVariance = calculateAccVar(z);
}

void SysTick_Handler(void){
	msTicks++;
	msCount++;
	count1000++;
	count3000++;
	count100++;
	count50++;
}

uint32_t getTicks(void){
	return msTicks;
}

void turnOnAlarm(){
	//TODO: alarm turning on
}

void turnOffAlarm(){
//TODO: alarm turning off
}

void EINT0_IRQHandler(void){
	if(DEBUG){
		printf("In SW3 triggered interrupt. EINT0.\n");
	}
	LPC_SC->EXTINT = 0b001;

	if(mode == REGULAR){
		mode = RELAY;
		turnOffAlarm();
	} else {
		mode = REGULAR;
	}

	updateOledMode();

	printf("Finish EINT0\n");
}

int main (void) {
	lastTime = clock();
	/* ---> SYSTEM CONFIG <---*/
	SysTick_Config (SystemCoreClock / 1000);

    uint8_t dir = 1;
    uint8_t wait = 0;
    uint8_t state = 0;

    init_i2c();
    init_ssp();

    pca9532_init();
    joystick_init();
    acc_init();
    oled_init();

    init_SW3();
    init_SW4();
    config_EINT0();

    temp_init(&getTicks);
    led7seg_init();
    light_enable();
    light_setRange(LIGHT_RANGE_16000);

    resetAcc();

    int tempi = 0;
    for(;tempi < N_SAMPLE; tempi++){
    	queue[tempi] = 0;
    }

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
    led7seg_setChar('0', 0);
    updateOled();
    rgb_init();
    while (1)
    {
    	//Update Acc
    	if(count50 >= 50 || count50 ==0){
    		count50 = 0;
        	if(mode == REGULAR && condition ==BRIGHT){
				updateAcc();
        	}
    	}
    	if(count100 >= 100 || count100 == 0){
    		count100 = 0;
    		if(mode == RELAY || (mode==REGULAR && condition ==DIM)){
    	        updateAcc();
    		}
    	}
		/*########## temp & light & led7seg Update  ########### */
				/* # */
    	if(count1000 >= 1000 || 1000 ==0){
    		count1000 = 0;
        	segDisplayNumber = segDisplayNumber == 9 ? 0 : segDisplayNumber + 1;
        	led7seg_setChar(48 + segDisplayNumber, 0);
        	updateOledAcc();
        	if(mode == REGULAR && condition ==BRIGHT){
            	lastLight = light_read();
            	lastTemp = temp_read();
            	updateOledLight();
            	updateOledTemp();
            	printf("update light %d, temp %d\n", lastLight, lastTemp);
        	}
    	}
    	if(count3000 >= 3000 || count3000 == 0){
    		count3000 = 0;
    		if(mode == RELAY || (mode==REGULAR && condition ==DIM)){
            	lastLight = light_read();
            	lastTemp = temp_read();
            	updateOledLight();
            	updateOledTemp();
            	updateOledAcc();
    		}
    	}

        /* ############ SW4 Push Button  ########### */
        /* # */
        if(!((GPIO_ReadValue(1) >> 31) & 0b01)){
        	if(DEBUG){
        		printf("In main loop reading SW4!\n");
        	}
        	GPIO_ClearValue(1, 0b01 << 31);
        	turnOffAlarm();
        	if(isRgbOn){
        		rgb_setLeds(0);
        		isRgbOn = 0;
        	} else {
        		isRgbOn = 1;
        		rgb_setLeds(RGB_RED);
        	}

        }

    	Timer0_Wait(5);
    }


}

void check_failed(uint8_t *file, uint32_t line)
{
	/* User can add his own implementation to report the file name and line number,
	 ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */

	/* Infinite loop */
	printf("Wrong parameters value: file %s on line %d\r\n", file, line);
	while(1);
}

