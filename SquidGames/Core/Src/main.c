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
#include "ili9488.h"
#include "xpt2046.h"

#include "lvgl.h"
#include "motor_ui.h"
#include <math.h>
#include "screen_main.h"
#include "types.h"
#include "controller.h"
#include "motor.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

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
TIM_HandleTypeDef htim5;

/* USER CODE BEGIN PV */
Motor motor_1;
Motor motor_2;

float motor_1_ewma_limit = 0.29f;
float motor_2_ewma_limit = 0.31f;

ControllerState cs = {0.0f, 0.0f, 0, 0, 0, 0, 0, 0};
ControllerState last_state = {0.0f, 0.0f, 0, 0, 0, 0, 0, 0};
volatile uint16_t adc_vals[2];
int slip = 0;
int PID_enabled = 0;
int gyro_control = 0;
ExploreState es = START_RIGHT;
ControllerMode ps2_mode = ADVANCED;
float current_limit;

// LVGL display buffer
static lv_disp_draw_buf_t draw_buf;
static lv_color_t lvgl_buf1[ILI9488_WIDTH * LVGL_BUF_LINES];
static lv_color_t lvgl_buf2[ILI9488_WIDTH * LVGL_BUF_LINES];

// DMA transfer buffer — holds RGB666 converted pixels
// Max size: 320 pixels * 10 rows * 3 bytes = 9,600 bytes
static uint8_t dma_buf[ILI9488_WIDTH * LVGL_BUF_LINES * 3];

// Store the display driver pointer so the DMA ISR can call lv_disp_flush_ready
static lv_disp_drv_t *disp_drv_p = NULL;
lv_disp_t * g_disp_p;
volatile uint8_t dma_busy = 0;
uint8_t is_display_dma_busy(void) {
    return dma_busy;
}
static lv_indev_drv_t indev_drv;
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
static void MX_TIM5_Init(void);
/* USER CODE BEGIN PFP */
static void lvgl_display_init(void);
static void my_flush_cb(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_p);
void input_handler();

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
void control(Motor* m) {
	float velocity = get_velocity(m);
//	printf("velocity:%f,encoder:%d\n\r", velocity, m->get_encoder_count());
//	printf("velocity:%f,Dummy1:0,Dummy2:100\n\r", velocity, m->get_encoder_count());
	m->target_velocity += min(max(m->set_velocity - m->target_velocity, -MAX_ACCEL), MAX_ACCEL);
	float duty_cycle = pid(m, velocity, m->target_velocity);
	set_pwm(m, duty_cycle, &htim3);
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM4)  // control loop timer at 1kHz
    {
    	if (PID_enabled) {
            control(&motor_1);
            control(&motor_2);
    	}
    } else if (htim->Instance == TIM5) {
    	input_handler();
    }
}

void joystick_to_motor(float input1, float input2, float* motor_1_speed, float* motor_2_speed, ControllerMode mode) {
	// if (mode == PULL_ONLY) {
	// 	HAL_GPIO_WritePin(GPIOG, GPIO_PIN_0, GPIO_PIN_RESET);
	// 	HAL_GPIO_WritePin(GPIOG, GPIO_PIN_1, GPIO_PIN_RESET);
	// } else {
	// 	HAL_GPIO_WritePin(GPIOG, GPIO_PIN_0, GPIO_PIN_SET);
	// 	HAL_GPIO_WritePin(GPIOG, GPIO_PIN_1, GPIO_PIN_SET);
	// }
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
				if (slip < MAX_SLIP) {
					*motor_2_speed = input2 + offset; // add a slight unspooling offset
				} else {
					*motor_2_speed = 0;
				}
			} else if (input1 > 0) {
				*motor_2_speed = -1.0 * input1;
				float offset = min(0.5 * input1, 0.1);
				if (slip < MAX_SLIP) {
					*motor_1_speed = input2 + offset;
				} else {
					*motor_1_speed = 0;
				}
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
				input1 = min(input1, max(-input2 * 0.95, 0));
			}
			if (input2 > 0) {
				input2 = min(input2, max(-input1 * 0.95, 0));
			}
			*motor_1_speed = input1;
			*motor_2_speed = input2;
			break;
	}
}



ExploreState explore_update(ExploreState es) {
	static int wait_states = 0;
	static ExploreState next_state = WAIT;
	float motor_1_speed = 0.0f, motor_2_speed = 0.0f;
	update_ewma(&motor_1);
	update_ewma(&motor_2);
//	update_ewma_limits(&cs, &last_state, &motor_1_ewma_limit, &motor_2_ewma_limit);
//	printf("Dummy1:0,Dummy2:1,EWMA1:%f,EWMA2:%f\n\r", motor_1_ewma_limit, motor_2_ewma_limit);
	printf("Dummy1:0,Dummy2:1,Motor1:%f,Motor2:%f\n\r", motor_1.current_ewma_fast, motor_2.current_ewma_fast);
	switch (es) {
	case START_LEFT:
		HAL_GPIO_WritePin(GPIOB, GPIO_PIN_7, GPIO_PIN_SET);
//		// stop motors
//		for (int i = 0; i < 20; ++i) {
//			set_pwm(&motor_1, 0, &htim3);
//			set_pwm(&motor_2, 0, &htim3);
//			printf("Dummy1:0,Dummy2:1,EWMA1:%f,EWMA2:%f\n\r", motor_1_ewma_limit, motor_2_ewma_limit);
////			printf("Dummy1:0,Dummy2:1,Motor1:%f,Motor2:%f\n\r", motor_1.current_ewma_fast, motor_2.current_ewma_fast);
//			HAL_Delay(5);
//		}
		motor_1_speed = -0.3;
		motor_2_speed = 0.27;
		set_pwm(&motor_1, motor_1_speed, &htim3);
		set_pwm(&motor_2, motor_2_speed, &htim3);
		wait_states = 40;
		next_state = MOVE_LEFT;
		es = WAIT;
//		for (int i = 0; i < 90; ++i) {
//			set_pwm(&motor_1, motor_1_speed, &htim3);
//			set_pwm(&motor_2, motor_2_speed, &htim3);
////			if (i % 10 == 0)
////			printf("Dummy1:0,Dummy2:1,EWMA1:%f,EWMA2:%f\n\r", motor_1_ewma_limit, motor_2_ewma_limit);
//				printf("Dummy1:0,Dummy2:1,Motor1:%f,Motor2:%f\n\r", motor_1.current_ewma_fast, motor_2.current_ewma_fast);
//			HAL_Delay(5);
//		}
//		HAL_Delay(450);
		break;
	case MOVE_LEFT:
//		printf("Motor 1 EWMA: %.2f\n\r", motor_1.current_ewma_fast);
		HAL_GPIO_WritePin(GPIOB, GPIO_PIN_7, GPIO_PIN_RESET);
		if (motor_1.current_ewma_fast > motor_1_ewma_limit) {
			motor_1_speed = 0;
			motor_2_speed = 0;
			es = STOP_RIGHT;
		} else {
			motor_1_speed = -0.3;
			motor_2_speed = 0.27;
		}
		break;
	case STOP_LEFT:
		motor_1_speed = 0;
		motor_2_speed = 0;
		wait_states = 20;
		next_state = START_LEFT;
		es = WAIT;
		break;
	case START_RIGHT:
		HAL_GPIO_WritePin(GPIOB, GPIO_PIN_7, GPIO_PIN_SET);
		// stop motors
//		for (int i = 0; i < 20; ++i) {
//			set_pwm(&motor_1, 0, &htim3);
//			set_pwm(&motor_2, 0, &htim3);
//			printf("Dummy1:0,Dummy2:1,EWMA1:%f,EWMA2:%f\n\r", motor_1_ewma_limit, motor_2_ewma_limit);
//			printf("Dummy1:0,Dummy2:1,Motor1:%f,Motor2:%f\n\r", motor_1.current_ewma_fast, motor_2.current_ewma_fast);
//			HAL_Delay(5);
//		}
		motor_2_speed = -0.3;
		motor_1_speed = 0.27;
		set_pwm(&motor_1, motor_1_speed, &htim3);
		set_pwm(&motor_2, motor_2_speed, &htim3);
		wait_states = 40;
		next_state = MOVE_RIGHT;
		es = WAIT;
//		es = MOVE_RIGHT;
//		for (int i = 0; i < 90; ++i) {
//			set_pwm(&motor_1, motor_1_speed, &htim3);
//			set_pwm(&motor_2, motor_2_speed, &htim3);
////			if (i % 10 == 0)
////			printf("Dummy1:0,Dummy2:1,EWMA1:%f,EWMA2:%f\n\r", motor_1_ewma_limit, motor_2_ewma_limit);
//				printf("Dummy1:0,Dummy2:1,Motor1:%f,Motor2:%f\n\r", motor_1.current_ewma_fast, motor_2.current_ewma_fast);
//			HAL_Delay(5);
//		}
//		HAL_Delay(450);
		break;

	case MOVE_RIGHT:
		HAL_GPIO_WritePin(GPIOB, GPIO_PIN_7, GPIO_PIN_RESET);
//		printf("Motor 2 EWMA: %.2f\n\r", motor_2.current_ewma_fast);
		if (motor_2.current_ewma_fast > motor_2_ewma_limit) {
			motor_1_speed = 0;
			motor_2_speed = 0;
			es = STOP_LEFT;
		} else {
			motor_2_speed = -0.3;
			motor_1_speed = 0.27;
		}
		break;
	case STOP_RIGHT:
		motor_1_speed = 0;
		motor_2_speed = 0;
		wait_states = 20;
		next_state = START_RIGHT;
		es = WAIT;
		break;
	case WAIT:
		if (wait_states-- == 0) {
			es = next_state;
		} else {
			es = WAIT;
		}
		return es;
	}
	set_pwm(&motor_1, motor_1_speed, &htim3);
	set_pwm(&motor_2, motor_2_speed, &htim3);
	return es;
}

// Bluetooth Data
uint8_t UARTBuffer[8];
float roll = 0;
float pitch = 0;
// ---- LVGL Display Flush Callback (DMA version) ----
// Converts RGB565 → RGB666 into a static buffer, then fires DMA.
// Returns immediately — CPU is free while DMA streams to SPI.
// lv_disp_flush_ready() is called from HAL_SPI_TxCpltCallback when done.
static void my_flush_cb(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_p) {
    while (dma_busy);        // wait out the previous transfer
    dma_busy = 1;

    uint16_t w = area->x2 - area->x1 + 1;
    uint16_t h = area->y2 - area->y1 + 1;
    uint32_t total_pixels = w * h;
//    printf("FLUSH: %dx%d at (%d,%d)\n\r", w, h, area->x1, area->y1);


    // Save the driver pointer for the DMA completion callback
    disp_drv_p = drv;

    // Convert all pixels from RGB565 → RGB666 into the DMA buffer
    uint8_t *dst = dma_buf;
    for (uint32_t i = 0; i < total_pixels; i++) {
        uint16_t c = color_p[i].full;
        *dst++ = (c >> 8) & 0xF8;
        *dst++ = (c >> 3) & 0xFC;
        *dst++ = (c << 3) & 0xF8;
    }

    // Set the address window (blocking — only a few bytes, fast)
    ILI9488_SetWindow(area->x1, area->y1, area->x2, area->y2);

    // Set DC=data, CS=low, then fire DMA
    LCD_DC_HIGH();
    LCD_CS_LOW();
//    HAL_SPI_Transmit_DMA(&hspi3, dma_buf, total_pixels * 3);

    HAL_StatusTypeDef ret = HAL_SPI_Transmit_DMA(&hspi3, dma_buf, total_pixels * 3);
    if (ret != HAL_OK) {
//        printf("DMA FAIL: %d, total=%lu\n\r", ret, (unsigned long)(total_pixels * 3));
        LCD_CS_HIGH();
        dma_busy = 0;
        lv_disp_flush_ready(drv);  // unblock LVGL even on failure
    }
    // DO NOT call lv_disp_flush_ready here!
    // It will be called from HAL_SPI_TxCpltCallback when DMA finishes.
}

// ---- LVGL Display Driver Setup ----
static void lvgl_display_init(void) {
	lv_disp_draw_buf_init(&draw_buf, lvgl_buf1, lvgl_buf2, ILI9488_WIDTH * LVGL_BUF_LINES);

    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res  = ILI9488_WIDTH;    // 320
    disp_drv.ver_res  = ILI9488_HEIGHT;   // 480

    disp_drv.flush_cb = my_flush_cb;
    disp_drv.draw_buf = &draw_buf;
    g_disp_p = lv_disp_drv_register(&disp_drv);
//    lv_disp_set_rotation(g_disp_p, LV_DISP_ROT_90);
}

// ---- DMA Transfer Complete Callback ----
// Called by HAL from the DMA interrupt when SPI TX finishes.
void HAL_SPI_TxCpltCallback(SPI_HandleTypeDef *hspi) {
    if (hspi->Instance == SPI3) {
        /* DMA done ≠ SPI done. Wait for the shift register to drain
         * before raising CS, otherwise the last byte gets truncated. */
//        while (__HAL_SPI_GET_FLAG(hspi, SPI_FLAG_FTLVL) != SPI_FTLVL_EMPTY);
//        while (__HAL_SPI_GET_FLAG(hspi, SPI_FLAG_BSY));

        LCD_CS_HIGH();
        dma_busy = 0;
        if (disp_drv_p != NULL) lv_disp_flush_ready(disp_drv_p);
    }
}

static void my_touchpad_read(lv_indev_drv_t *drv, lv_indev_data_t *data) {
    static int16_t last_x = 0, last_y = 0;
    int16_t x, y;

    if (XPT2046_Read(&x, &y)) {
        last_x = x;
        last_y = y;
        data->state = LV_INDEV_STATE_PRESSED;
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
    }
    /* LVGL expects the last known point reported on release as well. */
    data->point.x = last_x;
    data->point.y = last_y;
}

static void lvgl_touch_init(void) {
    XPT2046_Init();
    lv_indev_drv_init(&indev_drv);
    indev_drv.type    = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = my_touchpad_read;
    lv_indev_drv_register(&indev_drv);
}

void input_handler() {
	settings_t s = settings_get();
	current_limit = s.current_limit;

	float motor_1_speed = 0.f, motor_2_speed = 0.f;

	switch (s.mode) {
	case BASIC:
	case ADV:
	case PULL:
		PID_enabled = 1;
		// returns 0 if controller is in digital mode (invalid)
		if (!update_controller_state(&cs, &last_state, &hspi1)) {
			set_pwm(&motor_1, 0, &htim3);
			set_pwm(&motor_2, 0, &htim3);
			return;
		}

		if (!cs.circle && !cs.square) {
			switch (s.mode) {
				case BASIC:
					joystick_to_motor(cs.joy_1_y, cs.joy_2_y, &motor_1_speed, &motor_2_speed, SAFE);
					break;
				case ADV:
					joystick_to_motor(cs.joy_1_y, cs.joy_2_y, &motor_1_speed, &motor_2_speed, ADVANCED);
					break;
				case PULL:
					joystick_to_motor(cs.joy_1_y, cs.joy_2_y, &motor_1_speed, &motor_2_speed, PULL_ONLY);
					break;
			}
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

		set_velocity(&motor_1, motor_1_speed * s.speed_mult);
		set_velocity(&motor_2, motor_2_speed * s.speed_mult);
		break;
	case BLUETOOTH:
		PID_enabled = 0;
		roll = min(max(roll, -1), 1);
		pitch = min(max(pitch, -1), 1);
		joystick_to_motor(-roll, pitch, &motor_1_speed, &motor_2_speed, LEFT_RIGHT);
				printf("roll: %f, pitch: %f\n\r", -roll, pitch);
		set_pwm(&motor_1, motor_1_speed * s.speed_mult / 400, &htim3);
		set_pwm(&motor_2, motor_2_speed * s.speed_mult / 400, &htim3);
		break;
	case EXPLORE:
		PID_enabled = 0;
		es = explore_update(es);
		break;
	}
	int16_t enc1 = motor_1.get_encoder_count();
	int16_t enc2 = motor_2.get_encoder_count();
	slip = enc1 + enc2;
}
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
  MX_TIM5_Init();
  /* USER CODE BEGIN 2 */
  // Quick SPI3 sanity check - toggle CS and send a dummy byte
//  LCD_CS_LOW();
//  uint8_t dummy = 0xAA;
//  HAL_StatusTypeDef ret = HAL_SPI_Transmit(&hspi3, &dummy, 1, 1000);
//  LCD_CS_HIGH();
//  printf("SPI3 transmit returned: %d\n\r", ret);  // 0=OK, 1=Error, 2=Busy, 3=Timeout

  // Init the LCD hardware
  ILI9488_Init(&hspi3);
  // Init LVGL core
  lv_init();
  // Register the display driver
  lvgl_display_init();
  // setup touch
  lvgl_touch_init();

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
  motor_2.pos_ch = TIM_CHANNEL_4;
  motor_2.neg_ch = TIM_CHANNEL_1;
//  tune_motor_1();
//  tune_motor_2();
  motor_1.gains.kp = 0.0035f;
  motor_1.gains.ki = 0.0175f;
  motor_1.gains.kd = 0.000067f;
  motor_1.gains.feed_forward = 0.0013f;
  motor_2.gains.kp = 0.0035f;
  motor_2.gains.ki = 0.0175f;
  motor_2.gains.kd = 0.000067f;
  motor_2.gains.feed_forward = 0.00145f;
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
  HAL_TIM_Base_Start_IT(&htim4);
  // input handling
  HAL_TIM_Base_Start_IT(&htim5);
  // PID stuff
//  float counter_target = (set_point / 360.0f) * CPR;
//  printf("Motor will go to %.2f degrees, which correlates to %.2f rotations\n\r", set_point, counter_target)
  // Tuning parameters:

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
  // 100f0d
  lv_obj_set_style_bg_color(lv_scr_act(), lv_color_hex(0x100f0d), LV_PART_MAIN);
  lv_obj_t *splash_view = lv_img_create(lv_scr_act());
  LV_IMG_DECLARE(splash);            // declare the converted image
  lv_img_set_src(splash_view, &splash);
  lv_obj_center(splash_view);

  uint32_t t0 = HAL_GetTick();
//  while (HAL_GetTick() - t0 < 2000) lv_timer_handler();  // show 2 sec
  while (!XPT2046_IS_TOUCHED()) lv_timer_handler();

  lv_obj_del(splash_view);
  screen_main_init();

  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
	lv_timer_handler();

// display testing
//	  uint16_t rx, ry;
//	  if (XPT2046_ReadRaw(&rx, &ry)) {
//		  char buf[48];
//		  snprintf(buf, sizeof(buf), "raw: x=%u y=%u\r\n", rx, ry);
//		  HAL_UART_Transmit(&hlpuart1, (uint8_t*)buf, strlen(buf), 100);
//		  HAL_Delay(100);
//	  }
	static uint32_t last_print = 0;

	int16_t sx, sy;
	if (XPT2046_Read(&sx, &sy)) {
		printf("screen x=%d y=%d \t delta t=%d\r\n", sx, sy, HAL_GetTick() - last_print);
//		HAL_Delay(100);
		last_print = HAL_GetTick();
	}

//	static float sim_voltage = 0.0f;
//	sim_voltage += 0.1f;

	motors_set_values(
			motor_1.current_ewma, motor_1.velocity_ewma,
			motor_2.current_ewma, motor_2.velocity_ewma
			);


//  	static float sim_voltage = 0.0f;
//	sim_voltage += 0.1f;

//	motor_bar_set_value(&motorV1, 12 * motor_1_speed);
//	motor_bar_set_value(&motorV2, 12 * motor_2_speed);
//	motor_bar_set_value(&motorC1, adc_to_current(adc_vals[0]));
//	motor_bar_set_value(&motorC2, adc_to_current(adc_vals[1]));
	// motor_bar_set_voltage(&motor1, 12 * sin(sim_voltage));
	// motor_bar_set_voltage(&motor2, 12 * cos(sim_voltage));
//	printf("targets:[%f, %f], vel: [%f, %f]\n\r", motor_1.target_velocity, motor_2.target_velocity, motor_1.velocity_ewma, motor_2.velocity_ewma);
	HAL_Delay(5);
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
  sConfig.IC1Polarity = TIM_ICPOLARITY_RISING;
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
  * @brief TIM5 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM5_Init(void)
{

  /* USER CODE BEGIN TIM5_Init 0 */

  /* USER CODE END TIM5_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};

  /* USER CODE BEGIN TIM5_Init 1 */

  /* USER CODE END TIM5_Init 1 */
  htim5.Instance = TIM5;
  htim5.Init.Prescaler = 79;
  htim5.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim5.Init.Period = 9999;
  htim5.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim5.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim5) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim5, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim5, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM5_Init 2 */

  /* USER CODE END TIM5_Init 2 */

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
  HAL_GPIO_WritePin(GPIOF, GPIO_PIN_13|GPIO_PIN_14|GPIO_PIN_15, GPIO_PIN_SET);

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

  /*Configure GPIO pins : PF13 PF14 PF15 */
  GPIO_InitStruct.Pin = GPIO_PIN_13|GPIO_PIN_14|GPIO_PIN_15;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
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

  /*Configure GPIO pin : TOUCH_IRQ_Pin */
  GPIO_InitStruct.Pin = TOUCH_IRQ_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(TOUCH_IRQ_GPIO_Port, &GPIO_InitStruct);

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
