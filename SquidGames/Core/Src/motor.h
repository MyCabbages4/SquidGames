/*
 * motor.h
 *
 *  Created on: Apr 19, 2026
 *      Author: khush
 */

#ifndef SRC_MOTOR_H_
#define SRC_MOTOR_H_

#include "stm32l4xx_hal.h"
#include "types.h"
#include "utils.h"

extern float current_limit;

// unsafe way to set motor PWM (this can stall the motor)
void set_pwm_unsafe(Motor* m, float duty_cycle_percent, TIM_HandleTypeDef* htim) {
	int duty_cycle = (int)(duty_cycle_percent * 200);
//	printf("Setting duty cycle to: %.2f\n\r", duty_cycle);
//	printf(",duty_cycle:%d", duty_cycle);
//	printf(",pos_ch:%d,neg_ch:%d\n\r", m->pos_ch, m->neg_ch);
	if (duty_cycle < 0) {
		__HAL_TIM_SET_COMPARE(htim, m->pos_ch, 0);
		__HAL_TIM_SET_COMPARE(htim, m->neg_ch, -duty_cycle);
	} else {
		__HAL_TIM_SET_COMPARE(htim, m->pos_ch, duty_cycle);
		__HAL_TIM_SET_COMPARE(htim, m->neg_ch, 0);
	}
}

void update_ewma(Motor* m) {
	// update ewma
	float current_now = m->get_current();
	m->current_ewma = (1-ALPHA) * m->current_ewma + ALPHA * current_now;
	m->current_ewma_fast = (1-ALPHA2) * m->current_ewma_fast + ALPHA2 * current_now;
}

// set PWM speed but also limit current
void set_pwm(Motor* m, float duty_cycle_percent, TIM_HandleTypeDef* htim) {
//	printf("Duty Cycle Percent: %f\n\r", duty_cycle_percent);
	update_ewma(m);
	// this is essentially a P controller
	float current_effort = (current_limit - m->current_ewma);
	if (current_effort < 0) current_effort = 0; // we want to avoid weirdness if current is over max value
	else if (current_effort > 1) current_effort = 1;

	float requested_magnitude = duty_cycle_percent < 0 ? -duty_cycle_percent : duty_cycle_percent;
	int sign = duty_cycle_percent < 0 ? -1 : 1;

	// choose the minimum of the 2 as your actual speed
	// the effect of this is that when we approach the max current, 'current_effort' becomes small, so we slow down the motor
	float min_magnitude = current_effort < requested_magnitude ? current_effort : requested_magnitude;
	set_pwm_unsafe(m, sign * min_magnitude, htim);
}

float pid(Motor* m, float measured, float set_point) {
	float error = set_point - measured;
//	 printf("Error:%.2f\n\r", error);

	float P = m->gains.kp * error;

	m->gains.integral += error * DT;
	float I = m->gains.ki * m->gains.integral;

	float D = m->gains.kd * (error - m->gains.prev_error) / DT;
	m->gains.prev_error = error;

	float output = P + I + D + m->gains.feed_forward * set_point;

	if (output < -1) {
		output = -1.0f;
	}

	if (output < -0.6) {
		m->gains.integral -= error * DT; // conditional integration
	}

	if (output > 1.0f) {
		output = 1.0f;
	}

	if (output > 0.6) {
		m->gains.integral -= error * DT; // conditional integration
	}
//	printf("Set_point:%f,measured:%f,dummy1:0,dummy2:100,output:%f\n\r", set_point, measured, output * 100);
	if (set_point < 0.001 && set_point > -0.001) return 0; // don't try to hold position when at 0 velocity
	return output;
}

float get_velocity(Motor* m) {
	int32_t count = m->get_encoder_count();
	int32_t delta = count - m->prev_encoder_count;
	m->prev_encoder_count = count;

	if (delta > 32767) delta -= 65536;
	if (delta < -32768) delta += 65536;
//	printf("Delta:%d\n\r", delta);

	float rpm = (((float)delta / CPR)) / DT * 60.f;
	m->velocity_ewma = (1 - ALPHA_VEL) * m->velocity_ewma + ALPHA_VEL * rpm;
	return m->velocity_ewma;
}

void set_velocity(Motor* m, float rpm) {
	m->set_velocity = rpm;
}

#endif /* SRC_MOTOR_H_ */
