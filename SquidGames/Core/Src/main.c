/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "stdio.h"
#include "string.h"
#include <math.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
typedef struct {
	float kp;
	float ki;
	float kd;
	float integral;
	float prev_error;
	float output;
	float integral_max;
	float integral_min;
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
} Motor;

typedef struct {
	float joy_1_y, joy_2_y;
	uint8_t square, circle;
	uint8_t dpad_up, dpad_down;
	uint8_t dpad_left, dpad_right;
} ControllerState;

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define PPR 20.0f
#define GEAR_RATIO 16.0f // ~4*4 = ~16 encoder revolutions for a motor output revolution
#define CPR 1000
#define MAX_CURRENT 1.0f // max current for each motor (Amps)
#define MAX_SLIP 2500
#define ALPHA 0.1 // for current EWMA
#define ALPHA2 0.5 // another, more responsive EWMA for detecting contact
#define SPEED_MUL 0.35
#define DT 0.0065536f	 // Time between each encoder read

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
ADC_HandleTypeDef hadc1;
DMA_HandleTypeDef hdma_adc1;

UART_HandleTypeDef hlpuart1;
UART_HandleTypeDef huart2;

SPI_HandleTypeDef hspi1;
SPI_HandleTypeDef hspi3;
DMA_HandleTypeDef hdma_spi3_tx;

TIM_HandleTypeDef htim1;
TIM_HandleTypeDef htim2;
TIM_HandleTypeDef htim3;
TIM_HandleTypeDef htim4;

/* USER CODE BEGIN PV */
Motor motor_1;
Motor motor_2;

ControllerState cs = {0.0f, 0.0f, 0, 0, 0, 0, 0, 0};
ControllerState last_state = {0.0f, 0.0f, 0, 0, 0, 0, 0, 0};
uint16_t adc_vals[2];
int slip = 0;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_TIM2_Init(void);
static void MX_LPUART1_UART_Init(void);
static void MX_TIM3_Init(void);
static void MX_TIM1_Init(void);
static void MX_SPI1_Init(void);
static void MX_ADC1_Init(void);
static void MX_SPI3_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_TIM4_Init(void);
/* USER CODE BEGIN PFP */
void update_ewma_limits(void);

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
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

// unsafe way to set motor PWM (this can stall the motor)
void set_pwm_unsafe(Motor* m, float duty_cycle_percent) {
	int duty_cycle = (int)(duty_cycle_percent * 200);
//	printf("Setting duty cycle to: %.2f\n\r", duty_cycle);
//	printf(",duty_cycle:%d", duty_cycle);
//	printf(",pos_ch:%d,neg_ch:%d\n\r", m->pos_ch, m->neg_ch);
	if (duty_cycle < 0) {
		__HAL_TIM_SET_COMPARE(&htim3, m->pos_ch, 0);
		__HAL_TIM_SET_COMPARE(&htim3, m->neg_ch, -duty_cycle);
	} else {
		__HAL_TIM_SET_COMPARE(&htim3, m->pos_ch, duty_cycle);
		__HAL_TIM_SET_COMPARE(&htim3, m->neg_ch, 0);
	}
}

// set PWM speed but also limit current
void set_pwm(Motor* m, float duty_cycle_percent) {
//	printf("Duty Cycle Percent: %f\n\r", duty_cycle_percent);
	// update ewma
	float current_now = m->get_current();
	m->current_ewma = (1-ALPHA) * m->current_ewma + ALPHA * current_now;
	m->current_ewma_fast = (1-ALPHA2) * m->current_ewma_fast + ALPHA2 * current_now;
	// this is essentially a P controller
	float current_effort = (MAX_CURRENT - m->current_ewma);
	if (current_effort < 0) current_effort = 0; // we want to avoid weirdness if current is over max value
	else if (current_effort > 1) current_effort = 1;

	float requested_magnitude = duty_cycle_percent < 0 ? -duty_cycle_percent : duty_cycle_percent;
	int sign = duty_cycle_percent < 0 ? -1 : 1;

	// choose the minimum of the 2 as your actual speed
	// the effect of this is that when we approach the max current, 'current_effort' becomes small, so we slow down the motor
	float min_magnitude = current_effort < requested_magnitude ? current_effort : requested_magnitude;
	set_pwm_unsafe(m, sign * min_magnitude);
}

float pid(Motor* m, float measured, float set_point) {
	float error = set_point - measured;
//	 printf("Error:%.2f\n\r", error);

	float P = m->gains.kp * error;

	m->gains.integral += error * DT;
	m->gains.integral = fmin(m->gains.integral, m->gains.integral_max);
	m->gains.integral = fmax(m->gains.integral, m->gains.integral_min);
	float I = m->gains.ki * m->gains.integral;

	float D = m->gains.kd * (error - m->gains.prev_error) / DT;
	m->gains.prev_error = error;

	float output = P + I + D + m->gains.feed_forward; // 0.1 is a feedforward term

	if (output < -1) {
		output = -1.0f;
	}

	if (output < -0.05f && output > -0.1f) {
		output = -0.1f;
	}

	if (output > 0.05f && output < 0.1f) {
		output = 0.1f;
	}

	if (output > 1.0f) {
		output = 1.0f;
	}

	return output;
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

float get_velocity(Motor* m) {
	int32_t count = __HAL_TIM_GET_COUNTER(&htim1);
	int32_t delta = count - m->prev_encoder_count;
	m->prev_encoder_count = count;

	if (delta > 32767) delta -= 65536;
	if (delta < -32768) delta += 65536;
//	printf("Delta:%d\n\r", delta);

	float rpm = (((float)delta / CPR)) / DT * 60.f;

	return rpm;
}

void control(Motor* m) {
	float velocity = get_velocity(m);
	float duty_cycle = pid(m, velocity, m->set_velocity);
	set_pwm(m, duty_cycle);
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM4)  // control loop timer at 1kHz
    {
//        control(&motor_1);
        // control(&motor_2);
    }
}

void spi_transaction(uint8_t* send, uint8_t* recv, uint32_t n) {
	HAL_GPIO_WritePin(GPIOE, GPIO_PIN_12, GPIO_PIN_RESET);
	HAL_SPI_TransmitReceive(&hspi1, send, recv, n, 100);
	HAL_GPIO_WritePin(GPIOE, GPIO_PIN_12, GPIO_PIN_SET);
}

typedef enum {
	EXPERT, // no limits
	ADVANCED, // same as expert, but with slip limit
	PULL_ONLY, // you can only pull
	LEFT_RIGHT, // one control for left/right, other for tensioning antagonistic motor
	AUTO_SPOOL, // add an offset to make the motors automatically spool themselves in
	SAFE, // limit unspooling to spooling speed of other motor, limits maneuverability somewhat
} ControllerMode;

void joystick_to_motor(float input1, float input2, float* motor_1_speed, float* motor_2_speed, ControllerMode mode) {
	// motor 1 is on the left, motor 2 is on the right
	// negative is pulling on the string, positive is releasing tension
	switch (mode) {
		case EXPERT:
			*motor_1_speed = input1;
			*motor_2_speed = input2;
			break;
		case ADVANCED:
			*motor_1_speed = input1;
			*motor_2_speed = input2;
			if (slip > MAX_SLIP) {
				*motor_1_speed = min(*motor_1_speed, 0);
				*motor_2_speed = min(*motor_2_speed, 0);
			}
			break;
		// TODO: think about disabling brake mode for this mode
		case PULL_ONLY:
			*motor_1_speed = min(input1, 0);
			*motor_2_speed = min(input2, 0);
			break;
		case LEFT_RIGHT:
			if (input1 < 0) {
				*motor_1_speed = input1;
				float offset = min(-0.5 * input1, 0.1);
				*motor_2_speed = input2 + offset; // add a slight unspooling offset
			} else if (input1 > 0) {
				*motor_2_speed = -1.0 * input1;
				float offset = min(0.5 * input1, 0.1);
				*motor_1_speed = input2 + offset;
			} else { // left/right control at 0, don't want to be able to unspool the other guy
				*motor_1_speed = 0;
				*motor_2_speed = 0;
			}
			break;
		case AUTO_SPOOL:
			*motor_1_speed = -0.12 + input1;
			*motor_2_speed = -0.13 + input2;
			break;
		case SAFE:
			if (input1 > 0) {
				input1 = min(input1, max(-input2, 0));
			}
			if (input2 > 0) {
				input2 = min(input2, max(-input1, 0));
			}
			*motor_1_speed = input1;
			*motor_2_speed = input2;
			break;
	}
}

int update_controller_state() {
	uint8_t send[] = {0x01, 0x42, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
	uint8_t recv[9];
	spi_transaction(send, recv, 9);
	if (recv[1] != 0x73) { // Don't do anything if we are in digital mode
//		printf("Controller in digital mode!\n\r");
		return 0;
	}

	last_state.joy_1_y = cs.joy_1_y;
	last_state.joy_2_y = cs.joy_2_y;
	last_state.square = cs.square;
	last_state.circle = cs.circle;
	last_state.dpad_up = cs.dpad_up;
	last_state.dpad_down = cs.dpad_down;
	last_state.dpad_left = cs.dpad_left;
	last_state.dpad_right = cs.dpad_right;

	cs.circle = !((recv[4] >> 5) & 1);
	cs.square = !(recv[4] >> 7);
	cs.dpad_left = !(recv[3] >> 7);
	cs.dpad_right = !((recv[3] >> 5) & 1);
	cs.dpad_up = !((recv[3] >> 4) & 1);
	cs.dpad_down = !((recv[3] >> 6) & 1);

	cs.joy_1_y = -(((float)recv[8] - 127.5f) / 127.5f) * SPEED_MUL;
	cs.joy_2_y = -(((float)recv[6] - 127.5f) / 127.5f) * SPEED_MUL;

	// deadzone
	if (116 < recv[8] && recv[8] < 150) {
		cs.joy_1_y = 0.0f;
	}
	if (117 < recv[6] && recv[6] < 127) {
		cs.joy_2_y = 0.0f;
	}

	update_ewma_limits();

	return 1;
}

typedef enum {
	START_LEFT = 0,
	MOVE_LEFT,
	START_RIGHT,
	MOVE_RIGHT
} ExploreState;

float motor_1_ewma_limit = 0.29f;
float motor_2_ewma_limit = 0.29f;

void update_ewma_limits() {
	// Update ewma limits: motor_1 = dpad_left/right; motor_2 = dpad_up/down
	if (cs.dpad_right && !last_state.dpad_right) {
		motor_1_ewma_limit += 0.01f;
	} else if (cs.dpad_left && !last_state.dpad_left) {
		if (motor_1_ewma_limit > 0.0f)
			motor_1_ewma_limit -= 0.01f;
	}

	if (cs.dpad_up && !last_state.dpad_up) {
		motor_2_ewma_limit += 0.01f;
	} else if (cs.dpad_down && !last_state.dpad_down) {
		if (motor_2_ewma_limit > 0.0f)
			motor_2_ewma_limit -= 0.01f;
	}
}

ExploreState explore_update(ExploreState es) {
	float motor_1_speed = 0.0f, motor_2_speed = 0.0f;
//	printf("Dummy1:0,Dummy2:1,EWMA1:%f,EWMA2:%f\n\r", motor_1_ewma_limit, motor_2_ewma_limit);
	printf("Dummy1:0,Dummy2:1,Motor1:%f,Motor2:%f\n\r", motor_1.current_ewma_fast, motor_2.current_ewma_fast);
	switch (es) {
	case START_LEFT:
		HAL_GPIO_WritePin(GPIOB, GPIO_PIN_7, GPIO_PIN_SET);
		// stop motors
		for (int i = 0; i < 20; ++i) {
			set_pwm(&motor_1, 0);
			set_pwm(&motor_2, 0);
//			printf("Dummy1:0,Dummy2:1,EWMA1:%f,EWMA2:%f\n\r", motor_1_ewma_limit, motor_2_ewma_limit);
			printf("Dummy1:0,Dummy2:1,Motor1:%f,Motor2:%f\n\r", motor_1.current_ewma_fast, motor_2.current_ewma_fast);
			HAL_Delay(5);
		}
		motor_1_speed = -0.3;
		motor_2_speed = 0.27;
		es = MOVE_LEFT;
		for (int i = 0; i < 90; ++i) {
			set_pwm(&motor_1, motor_1_speed);
			set_pwm(&motor_2, motor_2_speed);
//			if (i % 10 == 0)
//			printf("Dummy1:0,Dummy2:1,EWMA1:%f,EWMA2:%f\n\r", motor_1_ewma_limit, motor_2_ewma_limit);
				printf("Dummy1:0,Dummy2:1,Motor1:%f,Motor2:%f\n\r", motor_1.current_ewma_fast, motor_2.current_ewma_fast);
			HAL_Delay(5);
		}
//		HAL_Delay(450);
		break;

	case MOVE_LEFT:
//		printf("Motor 1 EWMA: %.2f\n\r", motor_1.current_ewma_fast);
		HAL_GPIO_WritePin(GPIOB, GPIO_PIN_7, GPIO_PIN_RESET);
		if (motor_1.current_ewma_fast > motor_1_ewma_limit) {
			motor_1_speed = 0;
			motor_2_speed = 0;
			es = START_RIGHT;
		} else {
			motor_1_speed = -0.3;
			motor_2_speed = 0.27;
		}
		break;

	case START_RIGHT:
		HAL_GPIO_WritePin(GPIOB, GPIO_PIN_7, GPIO_PIN_SET);
		// stop motors
		for (int i = 0; i < 20; ++i) {
			set_pwm(&motor_1, 0);
			set_pwm(&motor_2, 0);
//			printf("Dummy1:0,Dummy2:1,EWMA1:%f,EWMA2:%f\n\r", motor_1_ewma_limit, motor_2_ewma_limit);
			printf("Dummy1:0,Dummy2:1,Motor1:%f,Motor2:%f\n\r", motor_1.current_ewma_fast, motor_2.current_ewma_fast);
			HAL_Delay(5);
		}
		motor_2_speed = -0.3;
		motor_1_speed = 0.27;
		es = MOVE_RIGHT;
		for (int i = 0; i < 90; ++i) {
			set_pwm(&motor_1, motor_1_speed);
			set_pwm(&motor_2, motor_2_speed);
//			if (i % 10 == 0)
//			printf("Dummy1:0,Dummy2:1,EWMA1:%f,EWMA2:%f\n\r", motor_1_ewma_limit, motor_2_ewma_limit);
				printf("Dummy1:0,Dummy2:1,Motor1:%f,Motor2:%f\n\r", motor_1.current_ewma_fast, motor_2.current_ewma_fast);
			HAL_Delay(5);
		}
//		HAL_Delay(450);
		break;

	case MOVE_RIGHT:
		HAL_GPIO_WritePin(GPIOB, GPIO_PIN_7, GPIO_PIN_RESET);
//		printf("Motor 2 EWMA: %.2f\n\r", motor_2.current_ewma_fast);
		if (motor_2.current_ewma_fast > motor_2_ewma_limit) {
			motor_1_speed = 0;
			motor_2_speed = 0;
			es = START_LEFT;
		} else {
			motor_2_speed = -0.3;
			motor_1_speed = 0.27;
		}
		break;
	}
	set_pwm(&motor_1, motor_1_speed);
	set_pwm(&motor_2, motor_2_speed);
	return es;
}

// Bluetooth Data
uint8_t UARTBuffer[8];
float roll = 0;
float pitch = 0;
/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_TIM2_Init();
  MX_LPUART1_UART_Init();
  MX_TIM3_Init();
  MX_TIM1_Init();
  MX_SPI1_Init();
  MX_ADC1_Init();
  MX_SPI3_Init();
  MX_USART2_UART_Init();
  MX_TIM4_Init();
  /* USER CODE BEGIN 2 */
  // calibrate ADC for better accuracy
  HAL_ADCEx_Calibration_Start(&hadc1, ADC_SINGLE_ENDED);

  HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_1);
  HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_2);
  HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_3);
  HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_4);
  // Start encoder mode
  __HAL_TIM_SET_COUNTER(&htim2, 0);
  __HAL_TIM_SET_COUNTER(&htim1, 0);
  HAL_TIM_Encoder_Start_IT(&htim2, TIM_CHANNEL_ALL);
  HAL_TIM_Encoder_Start_IT(&htim1, TIM_CHANNEL_ALL);
  // Initialize motors
  motor_1.pos_ch = TIM_CHANNEL_2;
  motor_1.neg_ch = TIM_CHANNEL_3;
  motor_2.pos_ch = TIM_CHANNEL_1;
  motor_2.neg_ch = TIM_CHANNEL_4;
  tune_motor_1();
  tune_motor_2();
  motor_1.id = 1;
  motor_2.id = 2;
  motor_1.get_encoder_count = get_tim1_val;
  motor_2.get_encoder_count = get_tim2_val;
  motor_1.get_current = get_current_1;
  motor_2.get_current = get_current_2;
  motor_1.current_ewma = 0;
  motor_2.current_ewma = 0;
  motor_2.current_ewma_fast = 0;
  motor_2.current_ewma_fast = 0;
  // current sensing
  HAL_ADC_Start_DMA(&hadc1, (uint32_t*)adc_vals, 2);
  // PID timer
//  HAL_TIM_Base_Start_IT(&htim4);
//  HAL_TIM_IC_Start_IT(&htim4, TIM_CHANNEL_1);
  // PID stuff
  float set_point = 0.0f;
//  printf("Enter a set point for the motor to go to in degrees \n\r");
//  scanf("%f", set_point);
  set_point = 90.f;
//  float counter_target = (set_point / 360.0f) * CPR;
//  printf("Motor will go to %.2f degrees, which correlates to %.2f rotations\n\r", set_point, counter_target)
  // Tuning parameters:

  int delay = 5;

  ExploreState es = START_RIGHT;
  int explore_enabled = 0;

  // Communicate with Wrist Mount
  HAL_NVIC_SetPriority(USART2_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(USART2_IRQn);
  HAL_UART_Receive_IT(&huart2, UARTBuffer, sizeof(UARTBuffer));
  
  HAL_GPIO_WritePin(GPIOG, GPIO_PIN_0, GPIO_PIN_SET);
  HAL_GPIO_WritePin(GPIOG, GPIO_PIN_1, GPIO_PIN_SET);
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  printf("Hello world\n\r");
//  motor_1.set_velocity = 60.0f;
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  // returns 0 if controller is in digital mode (invalid)
	if (!update_controller_state()) {
		set_pwm(&motor_1, 0);
		set_pwm(&motor_2, 0);
		HAL_Delay(10);
		continue;
	}

//	update_motor(&motor_2, set_point);
//	update_motor(&motor_1, set_point);
	if (explore_enabled) {
		es = explore_update(es);
		HAL_Delay(delay);
		continue;
	}

//	printf("Dummy1:0,Dummy2:1,Motor1:%f,Motor2:%f\n\r", motor_1.current_ewma_fast, motor_2.current_ewma_fast);
//	printf("Dummy1:0,Dummy2:1,Motor1:%f,Motor2:%f\n\r", motor_1.get_current(), motor_2.get_current());

	float motor_1_speed = 0.f, motor_2_speed = 0.f;
	if (!cs.circle && !cs.square) {
		joystick_to_motor(cs.joy_1_y, cs.joy_2_y, &motor_1_speed, &motor_2_speed, LEFT_RIGHT);
	} else if (cs.circle && !cs.square) { // move right
		motor_2_speed = -0.3;
		motor_1_speed = 0.27;
	} else if (cs.square && !cs.circle) { // move left
		motor_1_speed = -0.3;
		motor_2_speed = 0.27;
	} else {
		motor_1_speed = 0;
		motor_2_speed = 0;
	}

//	printf("0: %02X, 1: %02X, 2: %02X, 3: %02X, 4: %02X, 5: %02X, 6: %02X, 7: %02X, 8: %02X,\n\r", recv[0] & 0xFF, recv[1] & 0xFF, recv[2] & 0xFF, recv[3] & 0xFF, recv[4] & 0xFF, recv[5] & 0xFF, recv[6] & 0xFF, recv[7] & 0xFF, recv[8] & 0xFF);
//	printf("Motor 1 Speed: %.2f, Motor 2 Speed: %.2f\n\r", motor_1_speed, motor_2_speed);
//	motor_1.set_velocity = motor_1_speed;
//	motor_2.set_velocity = motor_2_speed;
//	printf("Motor 1 set to: %.2f, Motor 2 set to: %.2f\n\r", motor_1.set_velocity, motor_2.set_velocity);
	set_pwm(&motor_1, motor_1_speed);
	set_pwm(&motor_2, motor_2_speed);

//	printf("Current: [%f, %f]\n\r", adc_to_current(adc_vals[0]), adc_to_current(adc_vals[1]));
//	printf("Motor speed: [%f, %f]\n\r", motor_1_speed, motor_2_speed);
//	printf("circle: %d, square: %d\n\r", circle, square);
	printf("Current_L:%f,Current_R:%f,Lower:0,Upper:0.7,Thresh1:0.2,Thresh2:0.27\n\r", motor_1.current_ewma_fast, motor_2.current_ewma_fast);

	int16_t enc1 = motor_1.get_encoder_count();
	int16_t enc2 = motor_2.get_encoder_count();
	slip = enc1 + enc2;
//	printf("Encoder counts: [%d, %d]\n\r", enc1, enc2);
//	printf("enc1:%d,enc2:%d,slip:%d\n\r", enc1, enc2, slip);

    HAL_Delay(delay);
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  if (HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_MSI;
  RCC_OscInitStruct.MSIState = RCC_MSI_ON;
  RCC_OscInitStruct.MSICalibrationValue = 0;
  RCC_OscInitStruct.MSIClockRange = RCC_MSIRANGE_6;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_MSI;
  RCC_OscInitStruct.PLL.PLLM = 1;
  RCC_OscInitStruct.PLL.PLLN = 40;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = RCC_PLLQ_DIV2;
  RCC_OscInitStruct.PLL.PLLR = RCC_PLLR_DIV2;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_3) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief ADC1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_ADC1_Init(void)
{

  /* USER CODE BEGIN ADC1_Init 0 */

  /* USER CODE END ADC1_Init 0 */

  ADC_ChannelConfTypeDef sConfig = {0};

  /* USER CODE BEGIN ADC1_Init 1 */

  /* USER CODE END ADC1_Init 1 */

  /** Common config
  */
  hadc1.Instance = ADC1;
  hadc1.Init.ClockPrescaler = ADC_CLOCK_ASYNC_DIV32;
  hadc1.Init.Resolution = ADC_RESOLUTION_12B;
  hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc1.Init.ScanConvMode = ADC_SCAN_ENABLE;
  hadc1.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
  hadc1.Init.LowPowerAutoWait = DISABLE;
  hadc1.Init.ContinuousConvMode = ENABLE;
  hadc1.Init.NbrOfConversion = 2;
  hadc1.Init.DiscontinuousConvMode = DISABLE;
  hadc1.Init.ExternalTrigConv = ADC_SOFTWARE_START;
  hadc1.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
  hadc1.Init.DMAContinuousRequests = ENABLE;
  hadc1.Init.Overrun = ADC_OVR_DATA_PRESERVED;
  hadc1.Init.OversamplingMode = ENABLE;
  hadc1.Init.Oversampling.Ratio = ADC_OVERSAMPLING_RATIO_256;
  hadc1.Init.Oversampling.RightBitShift = ADC_RIGHTBITSHIFT_4;
  hadc1.Init.Oversampling.TriggeredMode = ADC_TRIGGEREDMODE_SINGLE_TRIGGER;
  hadc1.Init.Oversampling.OversamplingStopReset = ADC_REGOVERSAMPLING_CONTINUED_MODE;
  if (HAL_ADC_Init(&hadc1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Regular Channel
  */
  sConfig.Channel = ADC_CHANNEL_1;
  sConfig.Rank = ADC_REGULAR_RANK_1;
  sConfig.SamplingTime = ADC_SAMPLETIME_92CYCLES_5;
  sConfig.SingleDiff = ADC_SINGLE_ENDED;
  sConfig.OffsetNumber = ADC_OFFSET_NONE;
  sConfig.Offset = 0;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Regular Channel
  */
  sConfig.Channel = ADC_CHANNEL_2;
  sConfig.Rank = ADC_REGULAR_RANK_2;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN ADC1_Init 2 */

  /* USER CODE END ADC1_Init 2 */

}

/**
  * @brief LPUART1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_LPUART1_UART_Init(void)
{

  /* USER CODE BEGIN LPUART1_Init 0 */

  /* USER CODE END LPUART1_Init 0 */

  /* USER CODE BEGIN LPUART1_Init 1 */

  /* USER CODE END LPUART1_Init 1 */
  hlpuart1.Instance = LPUART1;
  hlpuart1.Init.BaudRate = 115200;
  hlpuart1.Init.WordLength = UART_WORDLENGTH_8B;
  hlpuart1.Init.StopBits = UART_STOPBITS_1;
  hlpuart1.Init.Parity = UART_PARITY_NONE;
  hlpuart1.Init.Mode = UART_MODE_TX_RX;
  hlpuart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  hlpuart1.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  hlpuart1.Init.ClockPrescaler = UART_PRESCALER_DIV1;
  hlpuart1.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  hlpuart1.FifoMode = UART_FIFOMODE_DISABLE;
  if (HAL_UART_Init(&hlpuart1) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_SetTxFifoThreshold(&hlpuart1, UART_TXFIFO_THRESHOLD_1_8) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_SetRxFifoThreshold(&hlpuart1, UART_RXFIFO_THRESHOLD_1_8) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_DisableFifoMode(&hlpuart1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN LPUART1_Init 2 */

  /* USER CODE END LPUART1_Init 2 */

}

/**
  * @brief USART2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART2_UART_Init(void)
{

  /* USER CODE BEGIN USART2_Init 0 */

  /* USER CODE END USART2_Init 0 */

  /* USER CODE BEGIN USART2_Init 1 */

  /* USER CODE END USART2_Init 1 */
  huart2.Instance = USART2;
  huart2.Init.BaudRate = 115200;
  huart2.Init.WordLength = UART_WORDLENGTH_8B;
  huart2.Init.StopBits = UART_STOPBITS_1;
  huart2.Init.Parity = UART_PARITY_NONE;
  huart2.Init.Mode = UART_MODE_TX_RX;
  huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart2.Init.OverSampling = UART_OVERSAMPLING_16;
  huart2.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  huart2.Init.ClockPrescaler = UART_PRESCALER_DIV1;
  huart2.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  if (HAL_UART_Init(&huart2) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_SetTxFifoThreshold(&huart2, UART_TXFIFO_THRESHOLD_1_8) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_SetRxFifoThreshold(&huart2, UART_RXFIFO_THRESHOLD_1_8) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_DisableFifoMode(&huart2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART2_Init 2 */

  /* USER CODE END USART2_Init 2 */

}

/**
  * @brief SPI1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_SPI1_Init(void)
{

  /* USER CODE BEGIN SPI1_Init 0 */

  /* USER CODE END SPI1_Init 0 */

  /* USER CODE BEGIN SPI1_Init 1 */

  /* USER CODE END SPI1_Init 1 */
  /* SPI1 parameter configuration*/
  hspi1.Instance = SPI1;
  hspi1.Init.Mode = SPI_MODE_MASTER;
  hspi1.Init.Direction = SPI_DIRECTION_2LINES;
  hspi1.Init.DataSize = SPI_DATASIZE_8BIT;
  hspi1.Init.CLKPolarity = SPI_POLARITY_HIGH;
  hspi1.Init.CLKPhase = SPI_PHASE_2EDGE;
  hspi1.Init.NSS = SPI_NSS_SOFT;
  hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_256;
  hspi1.Init.FirstBit = SPI_FIRSTBIT_LSB;
  hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi1.Init.CRCPolynomial = 7;
  hspi1.Init.CRCLength = SPI_CRC_LENGTH_DATASIZE;
  hspi1.Init.NSSPMode = SPI_NSS_PULSE_DISABLE;
  if (HAL_SPI_Init(&hspi1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN SPI1_Init 2 */

  /* USER CODE END SPI1_Init 2 */

}

/**
  * @brief SPI3 Initialization Function
  * @param None
  * @retval None
  */
static void MX_SPI3_Init(void)
{

  /* USER CODE BEGIN SPI3_Init 0 */

  /* USER CODE END SPI3_Init 0 */

  /* USER CODE BEGIN SPI3_Init 1 */

  /* USER CODE END SPI3_Init 1 */
  /* SPI3 parameter configuration*/
  hspi3.Instance = SPI3;
  hspi3.Init.Mode = SPI_MODE_MASTER;
  hspi3.Init.Direction = SPI_DIRECTION_2LINES;
  hspi3.Init.DataSize = SPI_DATASIZE_8BIT;
  hspi3.Init.CLKPolarity = SPI_POLARITY_LOW;
  hspi3.Init.CLKPhase = SPI_PHASE_1EDGE;
  hspi3.Init.NSS = SPI_NSS_SOFT;
  hspi3.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_4;
  hspi3.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi3.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi3.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi3.Init.CRCPolynomial = 7;
  hspi3.Init.CRCLength = SPI_CRC_LENGTH_DATASIZE;
  hspi3.Init.NSSPMode = SPI_NSS_PULSE_DISABLE;
  if (HAL_SPI_Init(&hspi3) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN SPI3_Init 2 */

  /* USER CODE END SPI3_Init 2 */

}

/**
  * @brief TIM1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM1_Init(void)
{

  /* USER CODE BEGIN TIM1_Init 0 */

  /* USER CODE END TIM1_Init 0 */

  TIM_Encoder_InitTypeDef sConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};

  /* USER CODE BEGIN TIM1_Init 1 */

  /* USER CODE END TIM1_Init 1 */
  htim1.Instance = TIM1;
  htim1.Init.Prescaler = 0;
  htim1.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim1.Init.Period = 65535;
  htim1.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim1.Init.RepetitionCounter = 0;
  htim1.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  sConfig.EncoderMode = TIM_ENCODERMODE_TI12;
  sConfig.IC1Polarity = TIM_ICPOLARITY_RISING;
  sConfig.IC1Selection = TIM_ICSELECTION_DIRECTTI;
  sConfig.IC1Prescaler = TIM_ICPSC_DIV1;
  sConfig.IC1Filter = 0;
  sConfig.IC2Polarity = TIM_ICPOLARITY_RISING;
  sConfig.IC2Selection = TIM_ICSELECTION_DIRECTTI;
  sConfig.IC2Prescaler = TIM_ICPSC_DIV1;
  sConfig.IC2Filter = 0;
  if (HAL_TIM_Encoder_Init(&htim1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterOutputTrigger2 = TIM_TRGO2_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim1, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM1_Init 2 */

  /* USER CODE END TIM1_Init 2 */

}

/**
  * @brief TIM2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM2_Init(void)
{

  /* USER CODE BEGIN TIM2_Init 0 */

  /* USER CODE END TIM2_Init 0 */

  TIM_Encoder_InitTypeDef sConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};

  /* USER CODE BEGIN TIM2_Init 1 */

  /* USER CODE END TIM2_Init 1 */
  htim2.Instance = TIM2;
  htim2.Init.Prescaler = 0;
  htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim2.Init.Period = 65535;
  htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  sConfig.EncoderMode = TIM_ENCODERMODE_TI12;
  sConfig.IC1Polarity = TIM_ICPOLARITY_FALLING;
  sConfig.IC1Selection = TIM_ICSELECTION_DIRECTTI;
  sConfig.IC1Prescaler = TIM_ICPSC_DIV1;
  sConfig.IC1Filter = 0;
  sConfig.IC2Polarity = TIM_ICPOLARITY_FALLING;
  sConfig.IC2Selection = TIM_ICSELECTION_DIRECTTI;
  sConfig.IC2Prescaler = TIM_ICPSC_DIV1;
  sConfig.IC2Filter = 0;
  if (HAL_TIM_Encoder_Init(&htim2, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim2, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM2_Init 2 */

  /* USER CODE END TIM2_Init 2 */

}

/**
  * @brief TIM3 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM3_Init(void)
{

  /* USER CODE BEGIN TIM3_Init 0 */

  /* USER CODE END TIM3_Init 0 */

  TIM_MasterConfigTypeDef sMasterConfig = {0};
  TIM_OC_InitTypeDef sConfigOC = {0};

  /* USER CODE BEGIN TIM3_Init 1 */

  /* USER CODE END TIM3_Init 1 */
  htim3.Instance = TIM3;
  htim3.Init.Prescaler = 9;
  htim3.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim3.Init.Period = 99;
  htim3.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_PWM_Init(&htim3) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim3, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sConfigOC.OCMode = TIM_OCMODE_PWM1;
  sConfigOC.Pulse = 0;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  if (HAL_TIM_PWM_ConfigChannel(&htim3, &sConfigOC, TIM_CHANNEL_1) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_TIM_PWM_ConfigChannel(&htim3, &sConfigOC, TIM_CHANNEL_2) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_TIM_PWM_ConfigChannel(&htim3, &sConfigOC, TIM_CHANNEL_3) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_TIM_PWM_ConfigChannel(&htim3, &sConfigOC, TIM_CHANNEL_4) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM3_Init 2 */

  /* USER CODE END TIM3_Init 2 */
  HAL_TIM_MspPostInit(&htim3);

}

/**
  * @brief TIM4 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM4_Init(void)
{

  /* USER CODE BEGIN TIM4_Init 0 */

  /* USER CODE END TIM4_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};

  /* USER CODE BEGIN TIM4_Init 1 */

  /* USER CODE END TIM4_Init 1 */
  htim4.Instance = TIM4;
  htim4.Init.Prescaler = 7;
  htim4.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim4.Init.Period = 65535;
  htim4.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim4.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim4) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim4, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim4, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM4_Init 2 */

  /* USER CODE END TIM4_Init 2 */

}

/**
  * Enable DMA controller clock
  */
static void MX_DMA_Init(void)
{

  /* DMA controller clock enable */
  __HAL_RCC_DMAMUX1_CLK_ENABLE();
  __HAL_RCC_DMA1_CLK_ENABLE();

  /* DMA interrupt init */
  /* DMA1_Channel1_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Channel1_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA1_Channel1_IRQn);
  /* DMA1_Channel2_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Channel2_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA1_Channel2_IRQn);

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOE_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOF_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOG_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();
  HAL_PWREx_EnableVddIO2();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOF, GPIO_PIN_13|GPIO_PIN_15, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOG, GPIO_PIN_0|GPIO_PIN_1, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOE, GPIO_PIN_12, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_7, GPIO_PIN_RESET);

  /*Configure GPIO pin : PE2 */
  GPIO_InitStruct.Pin = GPIO_PIN_2;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  GPIO_InitStruct.Alternate = GPIO_AF13_SAI1;
  HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);

  /*Configure GPIO pins : PF0 PF1 PF2 */
  GPIO_InitStruct.Pin = GPIO_PIN_0|GPIO_PIN_1|GPIO_PIN_2;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_OD;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  GPIO_InitStruct.Alternate = GPIO_AF4_I2C2;
  HAL_GPIO_Init(GPIOF, &GPIO_InitStruct);

  /*Configure GPIO pin : PF7 */
  GPIO_InitStruct.Pin = GPIO_PIN_7;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  GPIO_InitStruct.Alternate = GPIO_AF13_SAI1;
  HAL_GPIO_Init(GPIOF, &GPIO_InitStruct);

  /*Configure GPIO pins : PC4 PC5 */
  GPIO_InitStruct.Pin = GPIO_PIN_4|GPIO_PIN_5;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  GPIO_InitStruct.Alternate = GPIO_AF7_USART3;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pin : PB0 */
  GPIO_InitStruct.Pin = GPIO_PIN_0;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  GPIO_InitStruct.Alternate = GPIO_AF10_OCTOSPIM_P1;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pin : PB2 */
  GPIO_InitStruct.Pin = GPIO_PIN_2;
  GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pins : PF13 PF15 */
  GPIO_InitStruct.Pin = GPIO_PIN_13|GPIO_PIN_15;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOF, &GPIO_InitStruct);

  /*Configure GPIO pins : PG0 PG1 */
  GPIO_InitStruct.Pin = GPIO_PIN_0|GPIO_PIN_1;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOG, &GPIO_InitStruct);

  /*Configure GPIO pin : PE12 */
  GPIO_InitStruct.Pin = GPIO_PIN_12;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);

  /*Configure GPIO pins : PB12 PB13 PB15 */
  GPIO_InitStruct.Pin = GPIO_PIN_12|GPIO_PIN_13|GPIO_PIN_15;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  GPIO_InitStruct.Alternate = GPIO_AF13_SAI2;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pin : PB14 */
  GPIO_InitStruct.Pin = GPIO_PIN_14;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  GPIO_InitStruct.Alternate = GPIO_AF14_TIM15;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pin : PC6 */
  GPIO_InitStruct.Pin = GPIO_PIN_6;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  GPIO_InitStruct.Alternate = GPIO_AF13_SAI2;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pin : PC7 */
  GPIO_InitStruct.Pin = GPIO_PIN_7;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  GPIO_InitStruct.Alternate = GPIO_AF8_SDMMC1;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pins : PC8 PC9 */
  GPIO_InitStruct.Pin = GPIO_PIN_8|GPIO_PIN_9;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  GPIO_InitStruct.Alternate = GPIO_AF12_SDMMC1;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pins : PA8 PA10 */
  GPIO_InitStruct.Pin = GPIO_PIN_8|GPIO_PIN_10;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  GPIO_InitStruct.Alternate = GPIO_AF10_OTG_FS;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pin : PA9 */
  GPIO_InitStruct.Pin = GPIO_PIN_9;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pin : PD0 */
  GPIO_InitStruct.Pin = GPIO_PIN_0;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  GPIO_InitStruct.Alternate = GPIO_AF9_CAN1;
  HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

  /*Configure GPIO pin : PD2 */
  GPIO_InitStruct.Pin = GPIO_PIN_2;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  GPIO_InitStruct.Alternate = GPIO_AF12_SDMMC1;
  HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

  /*Configure GPIO pin : PB7 */
  GPIO_InitStruct.Pin = GPIO_PIN_7;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pins : PB8 PB9 */
  GPIO_InitStruct.Pin = GPIO_PIN_8|GPIO_PIN_9;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_OD;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  GPIO_InitStruct.Alternate = GPIO_AF4_I2C1;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */
#ifdef __GNUC__
#define PUTCHAR_PROTOTYPE int __io_putchar(int ch)
#else
  #define PUTCHAR_PROTOTYPE int fputc(int ch, FILE *f)
#endif /* __GNUC__ */
PUTCHAR_PROTOTYPE
{
  HAL_UART_Transmit(&hlpuart1, (uint8_t *)&ch, 1, 0xFFFF);
  return ch;
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
    if (huart == &huart2) {
    	roll = *((float*)UARTBuffer);
    	pitch = *((float*)(UARTBuffer + 4));

    	printf("roll: %.2f, pitch: %.2f\r\n", roll, pitch);
        HAL_UART_Receive_IT(&huart2, UARTBuffer, sizeof(UARTBuffer));
    }
}
void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart) {
    if (huart == &huart2) {
        // Re-arm the receiver
        HAL_UART_Receive_IT(&huart2, UARTBuffer, sizeof(UARTBuffer));
    }
}
/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
