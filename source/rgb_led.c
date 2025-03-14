/*
 * rgb_led.c
 *
 *  Created on: 13 mar. 2025
 *  Author: Gustavo Sanchez
 */
#include "rgb_led.h"
#include "board.h"
#include "fsl_debug_console.h"
#include "fsl_gpio.h"
#include "app.h"


void rgb_init()
{


	uint32_t port_state = 0;

	/* Define the init structure for the output LED pin*/
	gpio_pin_config_t led_config = {
		kGPIO_DigitalOutput,
		0,
	};
    /* Init output LED GPIO. */
    GPIO_PortInit(GPIO, APP_BOARD_TEST_LED_PORT);
    GPIO_PortInit(GPIO, APP_SW_PORT);
    GPIO_PinInit(GPIO, APP_BOARD_TEST_LED_PORT, APP_BOARD_TEST_LED_PIN, &led_config);
    GPIO_PinWrite(GPIO, APP_BOARD_TEST_LED_PORT, APP_BOARD_TEST_LED_PIN, 1);

    /* Port masking */
    GPIO_PortMaskedSet(GPIO, APP_BOARD_TEST_LED_PORT, 0x0000FFFF);
    GPIO_PortMaskedWrite(GPIO, APP_BOARD_TEST_LED_PORT, 0xFFFFFFFF);
    port_state = GPIO_PortRead(GPIO, APP_BOARD_TEST_LED_PORT);
    port_state = GPIO_PortMaskedRead(GPIO, APP_BOARD_TEST_LED_PORT);
}

void rgb_blue_switch()
{
    LED_BLUE_TOGGLE();
}
