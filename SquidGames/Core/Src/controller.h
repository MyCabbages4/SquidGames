/*
 * controller.h
 *
 *  Created on: Apr 19, 2026
 *      Author: khush
 */

#ifndef SRC_CONTROLLER_H_
#define SRC_CONTROLLER_H_

#include "stm32l4xx_hal.h"
#include "types.h"

void spi_transaction(uint8_t* send, uint8_t* recv, uint32_t n, SPI_HandleTypeDef* hspi1) {
	HAL_GPIO_WritePin(GPIOE, GPIO_PIN_12, GPIO_PIN_RESET);
	HAL_SPI_TransmitReceive(hspi1, send, recv, n, 100);
	HAL_GPIO_WritePin(GPIOE, GPIO_PIN_12, GPIO_PIN_SET);
}

int update_controller_state(ControllerState* cs, ControllerState* last_state, SPI_HandleTypeDef* hspi1) {
	uint8_t send[] = {0x01, 0x42, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
	uint8_t recv[9];
	spi_transaction(send, recv, 9, hspi1);
	if (recv[1] != 0x73) { // Don't do anything if we are in digital mode
		printf("Controller in digital mode!\n\r");
		return 0;
	}

	last_state->joy_1_y = cs->joy_1_y;
	last_state->joy_2_y = cs->joy_2_y;
	last_state->square = cs->square;
	last_state->circle = cs->circle;
	last_state->dpad_up = cs->dpad_up;
	last_state->dpad_down = cs->dpad_down;
	last_state->dpad_left = cs->dpad_left;
	last_state->dpad_right = cs->dpad_right;

	cs->circle = !((recv[4] >> 5) & 1);
	cs->square = !(recv[4] >> 7);
	cs->dpad_left = !(recv[3] >> 7);
	cs->dpad_right = !((recv[3] >> 5) & 1);
	cs->dpad_up = !((recv[3] >> 4) & 1);
	cs->dpad_down = !((recv[3] >> 6) & 1);

	cs->joy_1_y = -(((float)recv[8] - 127.5f) / 127.5f);
	cs->joy_2_y = -(((float)recv[6] - 127.5f) / 127.5f);

	// deadzone
	if (116 < recv[8] && recv[8] < 150) {
		cs->joy_1_y = 0.0f;
	}
	if (117 < recv[6] && recv[6] < 127) {
		cs->joy_2_y = 0.0f;
	}

	return 1;
}

void update_ewma_limits(ControllerState* cs, ControllerState* last_state, float* motor_1_ewma_limit, float* motor_2_ewma_limit) {
	// Update ewma limits: motor_1 = dpad_left/right; motor_2 = dpad_up/down
	if (cs->dpad_right && !last_state->dpad_right) {
		*motor_1_ewma_limit += 0.01f;
	} else if (cs->dpad_left && !last_state->dpad_left) {
		if (*motor_1_ewma_limit > 0.0f)
			*motor_1_ewma_limit -= 0.01f;
	}

	if (cs->dpad_up && !last_state->dpad_up) {
		*motor_2_ewma_limit += 0.01f;
	} else if (cs->dpad_down && !last_state->dpad_down) {
		if (*motor_2_ewma_limit > 0.0f)
			*motor_2_ewma_limit -= 0.01f;
	}
}


#endif /* SRC_CONTROLLER_H_ */
