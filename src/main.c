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
#include "lpc17xx_uart.h"

#include "joystick.h"
#include "pca9532.h"
#include "acc.h"
#include "oled.h"
#include "rgb.h"

#include "led7seg.h"
#include "light.h"
#include "temp.h"

#include "main.h"

Mode mode = REGULAR;
Condition condition = DIM;
Condition lastCondition = DIM;
volatile static uint32_t msTicks = 0;
volatile static uint32_t msCount = 0;

static uint8_t barPos = 2;
volatile uint32_t count1000 = 0;
volatile uint32_t count2000 = 0;
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
int isAlarmOn = 0;
int isDisableChangeMade = 0;
int isEnableChangeMade = 0;

int32_t lastTemp = 0;
uint32_t lastLight = 0;
int lastVariance = 0;

int queuePosition = 0;
int queue[N_SAMPLE];


uint8_t data = 0;
uint32_t len = 0;
uint8_t line[64];
static char * msg = NULL;

void toggleRgb(void){
	if(isRgbOn){
		turnOffRgb();
	} else {
		turnOnRgb();
	}
}

void turnOffRgb(void){
	isRgbOn = 0;
	rgb_setLeds(0);
}

void turnOnRgb(void){
	isRgbOn = 1;
	rgb_setLeds(RGB_RED);
}

void changeToRelay(void){
	mode = RELAY;
	turnOffAlarm();
}

void changeToRegular(void){
	mode = REGULAR;
	turnOffRgb();
}


void toggleMode(void){
	if(mode == REGULAR){
		changeToRelay();
	} else {
		changeToRegular();
	}
}

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

void updateOledMode(){
	oled_fillRect(35, 34, 95, 44, OLED_COLOR_BLACK);
	if(mode == REGULAR){
		oled_putString(35, 34, (unsigned char *)"REGULAR", OLED_COLOR_WHITE, OLED_COLOR_BLACK);
	} else {
		oled_putString(35, 34, (unsigned char *) "RELAY", OLED_COLOR_WHITE, OLED_COLOR_BLACK);
	}
	updateOledCondition();
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

void config_EINT0(void){
	NVIC_EnableIRQ(EINT0_IRQn); //[Enabled] Use EINT0
}

//void config_EINT3(void){
//	GPIO_SetDir(2, 1 << 5, 0);
//	NVIC_EnableIRQ(EINT3_IRQn);
//	LPC_GPIOINT->IO2IntEnF |= 1 << 5;
//}

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
	count2000++;
	count3000++;
	count100++;
	count50++;
}

uint32_t getTicks(void){
	return msTicks;
}

void turnOnAlarm(void){
	isAlarmOn = 1;
	playAlarm();
}

void playAlarm(void){
    while (isAlarmOn) {
        if(!((GPIO_ReadValue(1) >> 31) & 0b01)){
        	turnOffAlarm();
        	GPIO_ClearValue(1, 0b01 << 31);
        	break;
        }
        NOTE_PIN_HIGH();
        Timer0_us_Wait(1200 / 2);
        if(!isAlarmOn){
        	break;
        }
        NOTE_PIN_LOW();
        Timer0_us_Wait(1200 / 2);
    }
}

void turnOffAlarm(void){
	isAlarmOn = 0;
}

void updateLed7Seg(void){
	segDisplayNumber = segDisplayNumber == 9 ? 0 : segDisplayNumber + 1;
	led7seg_setChar(48 + segDisplayNumber, 0);
}

void updateLight(void){
	lastLight = light_read();
	updateOledLight();
}

void updateTemp(void){
	lastTemp = temp_read();
	updateOledTemp();
}

void updateCondition(void){
	if(lastLight >= BRIGHT_CONDITION){
		condition = BRIGHT;
	} else {
		condition = DIM;
	}
}

void checkAndUpdateAll(void){
	/** Update Light Watchdog **/
//	if(mode == RELAY||lastLight >= BRIGHT_CONDITION){
//		if(!isDisableChangeMade){
//			NVIC_DisableIRQ(EINT3_IRQn);
//			isDisableChangeMade = 1;
//		}
//		isEnableChangeMade = 0;
//	} else {
//		condition = DIM;
//		if(!isEnableChangeMade){
//			LPC_GPIOINT->IO2IntClr = 1 << 5;
//			NVIC_EnableIRQ(EINT3_IRQn);
//			isEnableChangeMade = 1;
//		}
//		isDisableChangeMade = 0;
//	}

	/** Update Acceleration **/
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

	if(count1000 >= 1000 || 1000 ==0){
		count1000 = 0;
		updateLed7Seg();
    	updateOledAcc();
    	if(mode == REGULAR && condition ==BRIGHT){
    		updateLight();
    		updateCondition();
    		if(condition != lastCondition){
    			lastCondition = condition;
    			updateOledCondition();
    		}
    		updateTemp();
        	//printf("update light %d, temp %d\n", lastLight, lastTemp);
    	}
	}
	if(count2000>=2000 || count2000 == 0){
		count2000 = 0;
		if(mode == RELAY){
        	if(isRgbOn){
        		rgb_setLeds(0);
        		isRgbOn = 0;
        	} else {
        		isRgbOn = 1;
        		rgb_setLeds(RGB_RED);
        	}
		}
	}
	if(count3000 >= 3000 || count3000 == 0){
		count3000 = 0;
		if(mode == RELAY || (mode==REGULAR && condition ==DIM)){
			updateLight();
			updateCondition();
			if(condition != lastCondition){
				lastCondition = condition;
				updateOledCondition();
			}
			updateTemp();
        	updateOledAcc();
		}
	}


	/** Update Alarm **/
	if(mode == REGULAR && lastTemp >= TEMP_WARN*10 && !isAlarmOn){
		turnOnAlarm();
	} else {
		turnOffAlarm();
	}
}

void EINT0_IRQHandler(void){
	if(DEBUG){
		//printf("In SW3 triggered interrupt. EINT0.\n");
	}
	toggleMode();
	updateOledMode();
	LPC_SC->EXTINT = 0b001;

	//printf("Finish EINT0\n");
}

//void EINT3_IRQHandler(void){
//		condition = BRIGHT;
//		LPC_GPIOINT->IO2IntClr = 1 << 5;
//		lastLight = light_read();
//		light_clearIrqStatus();
//		updateOledCondition();
//		//NVIC_DisableIRQ(EINT3_IRQn);
//		isDisableChangeMade = 0;
//		isEnableChangeMade = 0;
//		printf("In EINT3_IRQHandler\n");
//}

//void init_Light_GPIO(void){
//	PINSEL_CFG_Type PinCfg;
//	PinCfg.Funcnum = 0;
//	PinCfg.Pinnum = 5;
//	PinCfg.Portnum = 2;
//	PINSEL_ConfigPin(&PinCfg);
//}

void pinsel_uart3(void){
	PINSEL_CFG_Type PinCfg;
	PinCfg.Funcnum = 2;
	PinCfg.Pinnum = 0;
	PinCfg.Portnum = 0;
	PINSEL_ConfigPin(&PinCfg);
	PinCfg.Pinnum = 1;
	PINSEL_ConfigPin(&PinCfg);
}

void init_uart(void){
	UART_CFG_Type uartCfg;
	uartCfg.Baud_rate = 115200;
	uartCfg.Databits = UART_DATABIT_8;
	uartCfg.Parity = UART_PARITY_NONE;
	uartCfg.Stopbits = UART_STOPBIT_1; //pin select for uart3;
	pinsel_uart3();
	//supply power & setup working par.s for uart3
	UART_Init(LPC_UART3, &uartCfg); //enable transmit for uart3
	UART_TxCmd(LPC_UART3, ENABLE);
}

void initializeAll(void){
	msg = "Welcome to EE2024 \r\n";
    init_i2c();
    init_ssp();

    pca9532_init();
    joystick_init();
    acc_init();
    oled_init();

    init_SW3();
    init_SW4();
    //init_Light_GPIO();
    config_EINT0();
    //config_EINT3();
    init_uart();

    temp_init(&getTicks);
    led7seg_init();
    light_enable();
    light_setRange(LIGHT_RANGE_16000);
    //light_setHiThreshold(BRIGHT_CONDITION);
    resetAcc();
    lastTime = clock();
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

    oled_clearScreen(OLED_COLOR_BLACK);
    led7seg_setChar('0', 0);
    updateOled();
    rgb_init();
}

int main (void) {

	/* ---> SYSTEM CONFIG <---*/
	SysTick_Config (SystemCoreClock / 1000);

	initializeAll();

	//Testing
	//while(1){
	//	UART_Receive(LPC_UART3, &data, 1, BLOCKING); UART_Send(LPC_UART3, &data, 1, BLOCKING);
	//}


    while (1)
    {
    	checkAndUpdateAll();
    	Timer0_Wait(5);
    }


}

void check_failed(uint8_t *file, uint32_t line)
{
	/* User can add his own implementation to report the file name and line number,
	 ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */

	/* Infinite loop */
	//printf("Wrong parameters value: file %s on line %d\r\n", file, line);
	while(1);
}

