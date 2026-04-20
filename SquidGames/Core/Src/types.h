/*
 * types.h
 *
 *  Created on: Apr 19, 2026
 *      Author: khush
 */

#ifndef SRC_TYPES_H_
#define SRC_TYPES_H_

typedef struct {
	float kp;
	float ki;
	float kd;
	float integral;
	float prev_error;
	float output;
	float feed_forward;
} PID;

typedef struct {
	PID gains;
	int16_t (*get_encoder_count)();
	float (*get_current)();
	uint32_t pos_ch;
	uint32_t neg_ch;
	uint8_t id;
	float current_ewma;
	float current_ewma_fast;
	int16_t prev_encoder_count;
	float set_velocity;
	float target_velocity;
	float velocity_ewma;
} Motor;

typedef struct {
	float joy_1_y, joy_2_y;
	uint8_t square, circle;
	uint8_t dpad_up, dpad_down;
	uint8_t dpad_left, dpad_right;
} ControllerState;

typedef enum {
	EXPERT, // no limits
	ADVANCED, // same as expert, but with slip limit
	PULL_ONLY, // you can only pull
	LEFT_RIGHT, // one control for left/right, other for tensioning antagonistic motor
	AUTO_SPOOL, // add an offset to make the motors automatically spool themselves in
	SAFE, // limit unspooling to spooling speed of other motor, limits maneuverability somewhat
} ControllerMode;

typedef enum {
	START_LEFT = 0,
	MOVE_LEFT,
	STOP_LEFT,
	START_RIGHT,
	MOVE_RIGHT,
	STOP_RIGHT,
	WAIT,
} ExploreState;

#define PPR 20.0f
#define GEAR_RATIO 16.0f // ~4*4 = ~16 encoder revolutions for a motor output revolution
#define CPR 1000
#define MAX_CURRENT 1.0f // max current for each motor (Amps)
#define MAX_SLIP 3000
#define ALPHA 0.1 // for current EWMA
#define ALPHA2 0.5 // another, more responsive EWMA for detecting contact
#define ALPHA_VEL 0.3 // EWMA for velocity filtering
#define SPEED_MUL 0.35
#define DT 0.0065536f // Time between each encoder read
#define MAX_ACCEL 20 // max acceleration
#define LVGL_BUF_LINES  40  // number of rows in LVGL draw buffer


#endif /* SRC_TYPES_H_ */
