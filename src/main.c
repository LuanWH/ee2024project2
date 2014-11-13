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

#define BUFFER_SIZE 200
#define REPORT_TIME 1000

char buffer[BUFFER_SIZE];
int isRecMsgReadyToPrint = 0;
int bufferPtr = 0;
char relayMsg[100];

Mode mode = REGULAR;
Condition condition = DIM;
Condition lastCondition = DIM;
volatile static uint32_t msTicks = 0;
volatile static uint32_t msCount = 0;

volatile uint32_t count1000 = 0;
volatile uint32_t count2000 = 0;
volatile uint32_t count3000 = 0;
volatile uint32_t count100 = 0;
volatile uint8_t count50 = 0;
volatile uint32_t count5000 = 0;
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

int32_t lastLastTemp = 0;
uint32_t lastLastLight = 0;
int lastLastVariance = 0;

int queuePosition = 0;
int queue[N_SAMPLE];

uint8_t data = 0;
uint32_t len = 0;
uint8_t line[64];
static char msgFormat[100] = "N091_T%02.1f_L%03u_V%03d\r\0";
static char relayMsgFormat[100] = "N091_T%02.1f_L%03u_V%03d_%s\r\0";
char myMsg[100];
char relayMsg[100];
char alarmMsg[] = "TEMPERATURE WARNING!!!\r\0";

int toggleHighLow = 0;

oled_color_t BG_COLOR = OLED_COLOR_BLACK;
oled_color_t FT_COLOR = OLED_COLOR_WHITE;

int isReportIconOn = 0;
int isAlarmIconOn = 0;
int isRelayMsgIconOn = 0;

volatile int i2cLock = 0;
volatile int spiLock = 0;

int isSelectionOn = 0;
int selectedLed = 0;

uint32_t lastPressed = 0;

void startBlink(void){
	pca9532_setLeds(0, 0xffff);
	pca9532_setBlink0Leds(1<<selectedLed);
}

void stopBlink(void){
	pca9532_setBlink1Leds(0);
	pca9532_setLeds(1<<selectedLed, 0xffff);
}

void toggleSelection(void){
	isSelectionOn = isSelectionOn == 0? 1: 0;
}

void ledMoveUp(void){
	if(selectedLed < 8){
		selectedLed = (selectedLed + 1) % 8;
	} else {
		selectedLed = (selectedLed - 1) % 8 + 8;
	}
}
void ledMoveDown(void){
	if(selectedLed < 8){
		selectedLed = (selectedLed - 1 + 8) % 8;
	} else {
		selectedLed = (selectedLed + 1) % 8 + 8;
	}
}
void ledMoveRight(void){
	if(selectedLed < 8){
		selectedLed = 15 - selectedLed;
		ledMoveUp();
	} else {
		selectedLed = 15 - selectedLed;
	}
}
void ledMoveLeft(void){
	if(selectedLed > 8){
		selectedLed = 15 - selectedLed;
		ledMoveDown();
	} else {
		selectedLed = 15 - selectedLed;
	}
}

void selectIC(uint8_t state){
	if(msTicks - lastPressed < 250){
		return;
	}
	lastPressed = msTicks;
	if((state & JOYSTICK_CENTER)!=0){
		toggleSelection();
	} else if(!isSelectionOn){
		return;
	} else if((state & JOYSTICK_UP)!=0){
		ledMoveUp();
	} else if((state & JOYSTICK_DOWN)!=0){
		ledMoveDown();
	} else if((state & JOYSTICK_LEFT)!=0){
		ledMoveLeft();
	} else if((state & JOYSTICK_RIGHT)!=0){
		ledMoveRight();
	} else {
		return;
	}
	if(isSelectionOn){
		startBlink();
	} else {
		stopBlink();
	}
}

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
	BG_COLOR = OLED_COLOR_WHITE;
	FT_COLOR = OLED_COLOR_BLACK;
}

void changeToRegular(void){
	mode = REGULAR;
	turnOffRgb();
	BG_COLOR = OLED_COLOR_BLACK;
	FT_COLOR = OLED_COLOR_WHITE;
}


void toggleMode(void){
	if(mode == REGULAR){
		changeToRelay();
	} else {
		changeToRegular();
	}
	updateOled();
	if(isReportIconOn){
		turnOnReportIcon();
	}
	if(isAlarmIconOn){
		turnOnAlarmIcon();
	}
	if(isRelayMsgIconOn){
		turnOnRelayMsgIcon();
	}
}

/*
 * Assume base board in zero-g position when reading first value.
 */
void resetAcc(void){
	if(!i2cLock){
		acc_read(&x, &y, &z);
		xoff = 0-x;
		yoff = 0-y;
		zoff = 0-z;
	}

}

void updateOledAcc(){
	if(lastLastVariance == lastVariance){
		return;
	} else {
		lastLastVariance = lastVariance;
	}
	while(spiLock);
	spiLock = 1;
	oled_fillRect(30,1,80,11,BG_COLOR);
	char sAcc[50];
	sprintf(sAcc, "%d", lastVariance);
	oled_putString(30, 1, (unsigned char *) sAcc, FT_COLOR,BG_COLOR);
	spiLock = 0;
}
void updateOledLight(){

	if(lastLastLight == lastLight){
		return;
	} else {
		lastLastLight = lastLight;
	}
	while(spiLock);
	spiLock = 1;
	oled_fillRect(40,12, 80, 22,BG_COLOR);
	char sLight[50];
	sprintf(sLight, "%u", lastLight);
	oled_putString(40, 12, (unsigned char *)sLight, FT_COLOR,BG_COLOR);
	spiLock = 0;
}
void updateOledTemp(){

	if(lastLastTemp == lastTemp){
		return;
	} else {
		lastLastTemp = lastTemp;
	}
	while(spiLock);
	spiLock = 1;
	oled_fillRect(35, 23, 80, 33, BG_COLOR);
	char sTemp[50];
	sprintf(sTemp, "%.1f", lastTemp/10.0);
	oled_putString(35, 23, (unsigned char *)sTemp, FT_COLOR, BG_COLOR);
	spiLock = 0;
}

void updateOledCondition(){
	spiLock = 1;
	oled_fillRect(35, 45, 70, 55, BG_COLOR);
	if(mode == REGULAR){
		if(condition == BRIGHT){
			oled_putString(35, 45, (unsigned char *) "BRIGHT", FT_COLOR, BG_COLOR);
		} else {
			oled_putString(35, 45, (unsigned char *) "DIM", FT_COLOR, BG_COLOR);
		}
	} else {
		oled_putString(35, 45, (unsigned char *) "N.A.", FT_COLOR, BG_COLOR);
	}
	spiLock = 0;

}

void updateOledMode(){
	while(spiLock) ;
	spiLock = 1;
	oled_fillRect(35, 34, 95, 44, BG_COLOR);
	if(mode == REGULAR){
		oled_putString(35, 34, (unsigned char *)"REGULAR", FT_COLOR, BG_COLOR);
	} else {
		oled_putString(35, 34, (unsigned char *) "RELAY", FT_COLOR, BG_COLOR);
	}
	updateOledCondition();
	spiLock = 0;
}

void updateOled(){
	while(spiLock);
	spiLock = 1;
	oled_clearScreen(BG_COLOR);
	char sAcc[50];
	sprintf(sAcc, "Var: %d", lastVariance);
	oled_putString(1, 1, (unsigned char *) sAcc, FT_COLOR,BG_COLOR);
	char sLight[50];
	sprintf(sLight, "Light: %u", lastLight);
	oled_putString(1, 12, (unsigned char *)sLight, FT_COLOR,BG_COLOR);
	char sTemp[50];
	sprintf(sTemp, "Temp: %.1f", lastTemp/10.0);
	oled_putString(1, 23, (unsigned char *)sTemp, FT_COLOR, BG_COLOR);
	if(mode == REGULAR){
		oled_putString(1, 34, (unsigned char *)"Mode: REGULAR", FT_COLOR, BG_COLOR);
		if(condition == BRIGHT){
			oled_putString(1, 45, (unsigned char *) "Cdtn: BRIGHT", FT_COLOR, BG_COLOR);
		} else {
			oled_putString(1, 45, (unsigned char *) "Cdtn: DIM", FT_COLOR, BG_COLOR);
		}
	} else {
		oled_putString(1, 34, (unsigned char *) "Mode: RELAY", FT_COLOR, BG_COLOR);
		oled_putString(1, 45, (unsigned char *) "Cdtn: N.A.", FT_COLOR, BG_COLOR);
	}
	spiLock = 0;
}

void turnOnReportIcon(void){
	if(spiLock) return;
	spiLock = 1;
	oled_circle(87, 7, 6, FT_COLOR);
	oled_putChar(85, 4, 'R', FT_COLOR, BG_COLOR);
	spiLock = 0;
}

void turnOffReportIcon(void){
	if(spiLock) return;
	spiLock = 1;
	oled_fillRect(80, 0, 95, 14, BG_COLOR);
	spiLock = 0;
}

void turnOnAlarmIcon(void){
	if(!isAlarmIconOn){
		if(spiLock) return;
		spiLock = 1;
		isAlarmIconOn = 1;
		oled_circle(87, 22, 6, FT_COLOR);
		oled_putChar(85, 19, 'T', FT_COLOR, BG_COLOR);
		spiLock = 0;
	}
}

void turnOffAlarmIcon(void){
	if(isAlarmIconOn){
		if(spiLock) return;
		spiLock = 1;
		isAlarmIconOn = 0;
		oled_fillRect(80, 15, 95, 29, BG_COLOR);
		spiLock = 0;
	}
}

void turnOnRelayMsgIcon(void){
	if(spiLock) return;
	spiLock = 1;
	oled_fillRect(74, 47, 94, 62, BG_COLOR);
	oled_rect(74,47,94,62,FT_COLOR);
	oled_line(74,47,84,54,FT_COLOR);
	oled_line(84,54,94,47,FT_COLOR);
	spiLock = 0;
}

void turnOffRelayMsgIcon(void){
	if(spiLock) return;
	spiLock = 1;
	oled_fillRect(74, 47, 94, 62, BG_COLOR);
	spiLock = 0;
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
	if(i2cLock){
		return;
	}
	i2cLock = 1;
	acc_read(&x, &y, &z);
	i2cLock = 0;
	z = z+zoff;
	lastVariance = calculateAccVar(z);
}

void SysTick_Handler(void){
	msTicks++;
	msCount++;
	count1000++;
	//count2000++;
	count3000++;
	//count100++;
	//count50++;
	count5000++;
	if(msCount % 50 == 0){
    	if(mode == REGULAR && condition ==BRIGHT){
			updateAcc();
    	}
	}
	if(msCount % 100 == 0){
		if(mode == RELAY || (mode==REGULAR && condition ==DIM)){
	        updateAcc();
		}
	}
	if(msCount % 1000 == 0){
		updateLed7Seg();
	}

	if(msCount % 2000 == 0){
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

    if(!((GPIO_ReadValue(1) >> 31) & 0b01)){
    	turnOffAlarm();
    	GPIO_ClearValue(1, 0b01 << 31);
    }

	if(isAlarmOn){
		if(toggleHighLow){
			NOTE_PIN_HIGH();
			toggleHighLow = 0;
		} else {
			NOTE_PIN_LOW();
			toggleHighLow = 1;
		}
	}
}

uint32_t getTicks(void){
	return msTicks;
}

void turnOnAlarm(void){
	isAlarmOn = 1;
	turnOnAlarmIcon();
}

void turnOffAlarm(void){
	isAlarmOn = 0;
	turnOffAlarmIcon();
}

void updateLed7Seg(void){
	if(spiLock) return;
	spiLock=1;
	segDisplayNumber = segDisplayNumber == 9 ? 0 : segDisplayNumber + 1;
	led7seg_setChar(48 + segDisplayNumber, 0);
	spiLock=0;
}

void updateLight(void){
	i2cLock = 1;
	lastLight = light_read();
	i2cLock = 0;
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

	if(count1000 >= 1000 || 1000 ==0){
		count1000 = 0;
    	updateOledAcc();
    	if(mode == REGULAR && condition ==BRIGHT){
    		updateLight();
    		updateCondition();
    		if(condition != lastCondition){
    			lastCondition = condition;
    			updateOledCondition();
    		}
    		updateTemp();

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
		UART_SendString(LPC_UART3, &alarmMsg);
	}

	if(isRecMsgReadyToPrint){
		if(!isRelayMsgIconOn && mode == RELAY){
			turnOnRelayMsgIcon();
			isRelayMsgIconOn = 1;
		}
	} else {
		if(isRelayMsgIconOn){
			turnOffRelayMsgIcon();
			isRelayMsgIconOn = 0;
		}
	}

	uint8_t jState = joystick_read();
	if(jState != 0){
		while(i2cLock);
		i2cLock = 1;
		selectIC(jState);
		i2cLock = 0;
	}
}

void EINT0_IRQHandler(void){
	toggleMode();
	updateOledMode();
	isRecMsgReadyToPrint = 0;
	LPC_SC->EXTINT = 0b001;
}

void readFromBuffer(uint8_t recChar){

	if (recChar == '\r'){
		if(bufferPtr == 22){
			buffer[22] = '\0';
			isRecMsgReadyToPrint = 1;
			sprintf(relayMsg, "%s", buffer);
		}
		bufferPtr = 0;
	} else if(bufferPtr < BUFFER_SIZE){
		buffer[bufferPtr] = (char) recChar;
		bufferPtr++;
	}

}

void UART3_IRQHandler(void){
    if(LPC_UART3->IIR & UART_IIR_INTID_THRE){
    	while(LPC_UART3->LSR & 0x1){
    		readFromBuffer(LPC_UART3->RBR);
		}
    }
    if(LPC_UART3->IIR & UART_IIR_INTID_RDA){
    	while(LPC_UART3->LSR & 0x1){
    		readFromBuffer(LPC_UART3->RBR);
		}
    }

}

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

	LPC_UART3->FCR |= UART_FCR_TRG_LEV0;
	LPC_UART3->IER = 0x1;
	LPC_UART3->IER |= UART_IER_THREINT_EN;
	UART_IntConfig(LPC_UART3, UART_INTCFG_RBR, ENABLE);
	NVIC_ClearPendingIRQ(UART3_IRQn);
	NVIC_EnableIRQ(UART3_IRQn);
	NVIC_SetPriority(UART3_IRQn, NVIC_EncodePriority(4,3,0));
}

void initializeAll(void){

    init_i2c();
    init_ssp();
    while(i2cLock);
    i2cLock = 1;
    pca9532_init();
	pca9532_setBlink0Period(120);
	pca9532_setBlink0Duty(50);
	pca9532_setLeds(1<<selectedLed, 0xffff);
	i2cLock = 0;
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
    light_setRange(LIGHT_RANGE_64000);
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

int testNum(char c){
	if(c >= '0' && c <='9'){
		return 1;
	} else {
		return 0;
	}
}

void printMsg(void){
	float ptemp = lastTemp/10.0;
	if(ptemp >= 100){
		ptemp = 99.9;
	} else if(ptemp <= 0){
		ptemp = 25.0;
	}
	uint32_t plight = lastLight;
	if(plight >= 1000){
		plight = 999;
	}
	int pvar = lastVariance;
	if(pvar >= 1000){
		pvar = 999;
	}
	if(mode == REGULAR || !isRecMsgReadyToPrint){
		sprintf(myMsg, msgFormat, ptemp, plight, pvar);
	} else{
		//#N091_T30.4_L136_V004#
		if (relayMsg[0] == '#' &&
				relayMsg[21] == '#' &&
				relayMsg[1] == 'N' &&
				testNum(relayMsg[2]) &&
				testNum(relayMsg[3]) &&
				testNum(relayMsg[4]) &&
				relayMsg[5] == '_' &&
				relayMsg[6] == 'T' &&
				testNum(relayMsg[7]) &&
				testNum(relayMsg[8]) &&
				relayMsg[9] == '.' &&
				testNum(relayMsg[10]) &&
				relayMsg[11] == '_' &&
				relayMsg[12] == 'L' &&
				testNum(relayMsg[13]) &&
				testNum(relayMsg[14]) &&
				testNum(relayMsg[15]) &&
				relayMsg[16] == '_' &&
				relayMsg[17] == 'V' &&
				testNum(relayMsg[18]) &&
				testNum(relayMsg[19]) &&
				testNum(relayMsg[20])){
			isRecMsgReadyToPrint = 0;
			relayMsg[21] = '\0';
			sprintf(myMsg, relayMsgFormat, ptemp, plight, pvar, relayMsg+1);
		} else {
			isRecMsgReadyToPrint = 0;
		}
	}

}

void report(void){
	printMsg();
	UART_SendString(LPC_UART3, &myMsg);
}

void checkAndReport(void){

	if(count5000 <= 1000 || count5000 >=4000){
		if(!isReportIconOn){
			turnOnReportIcon();
			isReportIconOn = 1;
		}
	} else {
		if(isReportIconOn){
			turnOffReportIcon();
			isReportIconOn = 0;
		}
	}
	if(count5000 >= 5000 || count5000 == 0){
		count5000 = 0;
		report();
	}
}

int main (void) {

	/* ---> SYSTEM CONFIG <---*/
	SysTick_Config (SystemCoreClock / 1000);

	initializeAll();

    while (1)
    {
    	checkAndUpdateAll();
    	checkAndReport();
    	Timer0_Wait(2);
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

