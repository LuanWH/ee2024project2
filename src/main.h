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
#define TEMP_WARN 33
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
static void playNote(uint32_t note, uint32_t durationMs);
static uint32_t getNote(uint8_t ch);
static uint32_t getDuration(uint8_t ch);
static void playSong(uint8_t *song) ;

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
void updateOledAcc();
void updateOledLight();
void updateOledTemp();
void updateOledMode();
void updateOledCondition();
void updateOled();

/** LEDs Control **/
static void moveBar(uint8_t steps, uint8_t dir);

/** Peripherals Initialization **/
static void init_ssp(void);
static void init_i2c(void);
static void init_SW4(void);
static void init_SW3(void);

/** UART Control **/
void pinsel_uart3(void);
void init_uart(void);
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
