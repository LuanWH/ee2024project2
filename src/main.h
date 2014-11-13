/*
 * main.h
 *
 *  Created on: Nov 8, 2014
 *      Author: Wenhao
 */

#ifndef MAIN_H_
#define MAIN_H_

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
#define TEMP_WARN 31.5
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


/** FLow Control **/
void initializeAll(void);
void checkAndUpdateAll(void);
void updateCondition(void);
void checkAndReport(void);

/** Mode Control**/
void changeToRelay(void);
void changeToRegular(void);
void toggleMode(void);

/** Led7Seg Control **/
void updateLed7Seg(void);

/** Alarm Control**/
void turnOnAlarm(void);
void playAlarm(void);
void turnOffAlarm(void);

/** RGBs Control**/
void turnOffRgb(void);
void turnOnRgb(void);
void toggleRgb(void);

/** Accelerometer Control **/
int calculateAccVar(int8_t now);
void updateAcc();
void resetAcc(void);

/** Light Sensor Control **/
void updateLight(void);
//void init_Light_GPIO(void);

/** Temperature Control **/
void updateTemp(void);

/** OLED Control **/
void updateOledAcc(void);
void updateOledLight(void);
void updateOledTemp(void);
void updateOledMode(void);
void updateOledCondition(void);
void updateOled(void);
void turnOnReportIcon(void);
void turnOnAlarmIcon(void);
void turnOnRelayMsgIcon(void);

/** Peripherals Initialization **/
static void init_ssp(void);
static void init_i2c(void);
static void init_SW4(void);
static void init_SW3(void);

/** UART Control **/
void pinsel_uart3(void);
void init_uart(void);
void printMsg(void);
void report(void);
void reportRelay(void);

/** SysTick Control **/
void SysTick_Handler(void);
uint32_t getTicks(void);

/** Interrupts Control **/
void config_EINT0(void);
void EINT0_IRQHandler(void);
//void config_EINT3(void);
//void EINT3_IRQHandler(void);

/** Others **/
void check_failed(uint8_t *file, uint32_t line);
#endif /* MAIN_H_ */
