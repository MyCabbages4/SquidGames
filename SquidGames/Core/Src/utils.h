/*
 * utils.h
 *
 *  Created on: Apr 19, 2026
 *      Author: khush
 */

#ifndef SRC_UTILS_H_
#define SRC_UTILS_H_

#include "stm32l4xx_hal.h"

extern TIM_HandleTypeDef htim1;
extern TIM_HandleTypeDef htim2;
extern uint16_t adc_vals[2];
extern Motor motor_1;
extern Motor motor_2;

float min(float a, float b) {
	return a < b ? a : b;
}

float max(float a, float b) {
	return a > b ? a : b;
}

int16_t get_tim1_val() {
	return __HAL_TIM_GET_COUNTER(&htim1);
}

int16_t get_tim2_val() {
	return __HAL_TIM_GET_COUNTER(&htim2);
}

float adc_to_current(uint16_t adc_val) {
	// resistance is 0.7 ohms
	float voltage = (adc_val / 65535.0) * 3.3;
	// V = IR --> I = V/R
	return voltage / 0.7;
}

float get_current_1() {
	return adc_to_current(adc_vals[0]);
}

float get_current_2() {
	return adc_to_current(adc_vals[1]);
}

void tune_motor_1() {
	  motor_1.gains.kp = 0.01f;
	  motor_1.gains.ki = 0.002f;
//	  motor_1.gains.integral_max = 1.0f/motor_1.gains.ki;
//	  motor_1.gains.integral_min = -1.0f/motor_1.gains.ki;
//	  motor_1.gains.feed_forward = 0.1f;
}

void tune_motor_2() {
	  motor_2.gains.kp = 0.0041f;
	  motor_2.gains.ki = 0.0f;
}


#endif /* SRC_UTILS_H_ */
