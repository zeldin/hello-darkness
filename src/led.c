#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stm32f4xx.h>
#include <stm32f4xx_ll_gpio.h>
#include <stm32f4xx_ll_tim.h>
#include <stm32f4xx_ll_spi.h>

#include "error.h"
#include "led.h"
#include "key.h"
#include "tim.h"
#include "spi.h"
#include "adc.h"

static uint16_t LED_Mode;
static uint16_t LED_Update_Page;
static uint16_t LED_Update_Buffer[4][3][256];
static uint16_t LED_Update_Scratch_Readback[256];
static uint16_t LED_Status_Readback[17];
static uint16_t LED_Start_Buffer[16];
static uint8_t LED_Current_Buffer;
static uint8_t LED_Next_Buffer;
static uint8_t LED_Brightness = 32;


static const uint8_t LED_RGB_Map[LED_ID_MAX+1] = {
	2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,
	0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,1,
	2,2,2,1,0,0,0,0,0,0,0,0,2,2,1,1,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,1,
	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
	1,1,1,1,1,1,1,1,2,2,2,2,2,2,2,2,
	2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,
};

/* 0xbf are dummy entries.  0xc0- is position in LED_Key_MultiMap */
static const uint8_t LED_Key_Map[KEY_CODE_MAX+1] = {
	0x40, 0x42, 0x46, 0x44, 0x43, 0x45, 0x38, 0x37, 0x39, 0x3e, 0x33, 0x3a, 0x3d, 0x3b, 0xbf, 0xbf,
	0x4e, 0x4f, 0x48, 0x4a, 0x49, 0x47, 0x4b, 0x36, 0x34, 0x35, 0x32, 0x31, 0x2b, 0x3f, 0xbf, 0xbf,
	0x51, 0x4d, 0x55, 0x4c, 0x52, 0x54, 0x53, 0x13, 0x15, 0x14, 0x12, 0x16, 0x17, 0x18, 0xbf, 0xbf,
	0x50, 0x5f, 0x57, 0x59, 0x58, 0x56, 0x5a, 0x11, 0x1d, 0x1f, 0x10, 0x1c, 0x1e, 0x2f, 0xbf, 0xbf,
	0x74, 0x5e, 0x76, 0x5c, 0x75, 0x60, 0x5d, 0x61, 0x5b, 0x62, 0x64, 0x1b, 0x1a, 0x19, 0xbf, 0xbf,
	0x73, 0x71, 0x23, 0x72, 0x77, 0x78, 0x6e, 0x6b, 0x64, 0x6f, 0x65, 0x2e, 0x63, 0x66, 0xbf, 0xbf,
	0x7e, 0x70, 0x7c, 0x7f, 0x7a, 0x79, 0x7b, 0x69, 0x0e, 0x0f, 0x20, 0x21, 0x22, 0x67, 0xbf, 0xbf,
	0xbf, 0xbf, 0xc3, 0xc0, 0x7d, 0x09, 0x0b, 0x0a, 0x0c, 0x0d, 0x00, 0x03, 0x2d, 0x01, 0xbf, 0xbf,
	0xc9, 0xbf, 0xc6, 0xbf, 0x80, 0x81, 0x08, 0x2c, 0xbf, 0xbf, 0xbf, 0xbf, 0xbf, 0x02
};

static const uint8_t LED_Key_MultiMap[] = {
	0x8d, 0x8b, 0xff, /* Pos 0: M1 */
	0x8e, 0x8a, 0xff, /* Pos 3: M2 */
	0x8f, 0x89, 0xff, /* Pos 6: M3 */
	0x85, 0x86, 0x88, 0x87, 0xff /* Pos 9: Q button */
};

/**
* @brief This function is ran at the TIM10 update interrupt
*/
void LED_IRQHandler(void)
{
	if (LED_Mode == 0) {
		HAL_SPI_TransmitReceive_DMA(&SPI_HandleStruct_SPI2,
					    (uint8_t *)LED_Update_Buffer[LED_Current_Buffer][LED_Update_Page],
					    (uint8_t *)LED_Update_Scratch_Readback, 256);
		if (++LED_Update_Page > 2) {
			LED_Update_Page = 0;
			if (!LED_Next_Buffer)
				LED_Current_Buffer = 0;
			else {
				uint8_t flip = LED_Current_Buffer + 1;
				if (flip > 3)
					flip = 1;
				if ((LED_Next_Buffer & (1<<flip)))
					LED_Current_Buffer = flip;
			}
		}
	} else {
		int pulse_count, delay;
		uint16_t spi_word;

		LL_TIM_DisableCounter(TIM_HandleStruct_TIM10.Instance);
		LL_GPIO_SetOutputPin(GPIOE, LL_GPIO_PIN_3);
		LL_GPIO_ResetOutputPin(GPIOE, LL_GPIO_PIN_3);
		LL_GPIO_ResetOutputPin(GPIOE, LL_GPIO_PIN_2);
		switch (LED_Mode) {
		case 1:
			for(pulse_count=19; pulse_count>=0; --pulse_count) {
				LL_GPIO_SetOutputPin(GPIOE, LL_GPIO_PIN_3);
				LL_GPIO_ResetOutputPin(GPIOE, LL_GPIO_PIN_3);
			}
			LL_GPIO_SetOutputPin(GPIOE, LL_GPIO_PIN_2);
			spi_word = 0x0000;
			HAL_SPI_Transmit(&SPI_HandleStruct_SPI2, (uint8_t *)&spi_word, 1, 0);
			spi_word = 0x003e;
			HAL_SPI_Transmit(&SPI_HandleStruct_SPI2, (uint8_t *)&spi_word, 1, 0);
			memset(LED_Status_Readback, 0, sizeof(LED_Status_Readback));
			for (delay = 0xffff; delay >= 0; --delay)
				if (!LL_SPI_IsActiveFlag_BSY(SPI_HandleStruct_SPI2.Instance))
					break;
			HAL_SPI_Receive(&SPI_HandleStruct_SPI2, (uint8_t *)LED_Status_Readback, 17, 1);
			break;
		case 2:
			for(pulse_count=19; pulse_count>=0; --pulse_count) {
				LL_GPIO_SetOutputPin(GPIOE, LL_GPIO_PIN_3);
				LL_GPIO_ResetOutputPin(GPIOE, LL_GPIO_PIN_3);
			}
			LL_GPIO_SetOutputPin(GPIOE, LL_GPIO_PIN_2);
			spi_word = 0x0000;
			HAL_SPI_Transmit(&SPI_HandleStruct_SPI2, (uint8_t *)&spi_word, 1, 0);
			spi_word = 0xfffe;
			HAL_SPI_Transmit(&SPI_HandleStruct_SPI2, (uint8_t *)&spi_word, 1, 0);
			for (delay = 0xffff; delay >= 0; --delay)
				if (!LL_SPI_IsActiveFlag_BSY(SPI_HandleStruct_SPI2.Instance))
					break;
			LL_GPIO_ResetOutputPin(GPIOE, LL_GPIO_PIN_2);
			for(pulse_count=7; pulse_count>=0; --pulse_count) {
				LL_GPIO_SetOutputPin(GPIOE, LL_GPIO_PIN_3);
				LL_GPIO_ResetOutputPin(GPIOE, LL_GPIO_PIN_3);
			}
			LL_GPIO_SetOutputPin(GPIOE, LL_GPIO_PIN_2);
			HAL_SPI_Transmit(&SPI_HandleStruct_SPI2, (uint8_t *)LED_Start_Buffer, 16, 2);
			for (delay = 0xffff; delay >= 0; --delay)
				if (!LL_SPI_IsActiveFlag_BSY(SPI_HandleStruct_SPI2.Instance))
					break;
		}
		LL_GPIO_SetOutputPin(GPIOE, LL_GPIO_PIN_3);
		LL_GPIO_ResetOutputPin(GPIOE, LL_GPIO_PIN_3);
		LED_Mode = 0;
		LL_TIM_EnableCounter(TIM_HandleStruct_TIM10.Instance);
	}
}

static void LED_Start_TIM1(void)
{
	TIM_BreakDeadTimeConfigTypeDef TIM_BreakDeadTimeConfigStruct;

	uint16_t old_duty_ch1, old_duty_ch2, old_duty_ch3, old_duty_ch4;

	/* Re-initialize TIM1 */

	TIM_HandleStruct_TIM1.Instance               = TIM1;
	TIM_HandleStruct_TIM1.Init.Prescaler         = 0;
	TIM_HandleStruct_TIM1.Init.CounterMode       = TIM_COUNTERMODE_UP;
	TIM_HandleStruct_TIM1.Init.Period            = 99;
	TIM_HandleStruct_TIM1.Init.ClockDivision     = TIM_CLOCKDIVISION_DIV1;
	TIM_HandleStruct_TIM1.Init.RepetitionCounter = 0;
	CHECK_HAL_RESULT(HAL_TIM_Base_Init(&TIM_HandleStruct_TIM1));

	__HAL_TIM_SET_COMPARE(&TIM_HandleStruct_TIM1, TIM_CHANNEL_1, 28);
	__HAL_TIM_SET_COMPARE(&TIM_HandleStruct_TIM1, TIM_CHANNEL_2, 42);
	__HAL_TIM_SET_COMPARE(&TIM_HandleStruct_TIM1, TIM_CHANNEL_3, 56);
	__HAL_TIM_SET_COMPARE(&TIM_HandleStruct_TIM1, TIM_CHANNEL_4, 70);

	TIM_BreakDeadTimeConfigStruct.OffStateRunMode  = TIM_OSSR_DISABLE;
	TIM_BreakDeadTimeConfigStruct.OffStateIDLEMode = TIM_OSSI_DISABLE;
	TIM_BreakDeadTimeConfigStruct.LockLevel        = TIM_LOCKLEVEL_OFF;
	TIM_BreakDeadTimeConfigStruct.DeadTime         = 7;
	TIM_BreakDeadTimeConfigStruct.BreakState       = TIM_BREAK_DISABLE;
	TIM_BreakDeadTimeConfigStruct.BreakPolarity    = TIM_BREAKPOLARITY_HIGH;
	TIM_BreakDeadTimeConfigStruct.AutomaticOutput  = TIM_AUTOMATICOUTPUT_DISABLE;
	CHECK_HAL_RESULT(HAL_TIMEx_ConfigBreakDeadTime(&TIM_HandleStruct_TIM1, &TIM_BreakDeadTimeConfigStruct));

	/* Start PWM TIM1 CH1-4 at a low duty cycle */

	old_duty_ch1 = __HAL_TIM_GET_COMPARE(&TIM_HandleStruct_TIM1, TIM_CHANNEL_1);
	old_duty_ch2 = __HAL_TIM_GET_COMPARE(&TIM_HandleStruct_TIM1, TIM_CHANNEL_2);
	old_duty_ch3 = __HAL_TIM_GET_COMPARE(&TIM_HandleStruct_TIM1, TIM_CHANNEL_3);
	old_duty_ch4 = __HAL_TIM_GET_COMPARE(&TIM_HandleStruct_TIM1, TIM_CHANNEL_4);
	__HAL_TIM_SET_COMPARE(&TIM_HandleStruct_TIM1, TIM_CHANNEL_1, TIM_BreakDeadTimeConfigStruct.DeadTime-1);
	__HAL_TIM_SET_COMPARE(&TIM_HandleStruct_TIM1, TIM_CHANNEL_2, TIM_BreakDeadTimeConfigStruct.DeadTime-1);
	__HAL_TIM_SET_COMPARE(&TIM_HandleStruct_TIM1, TIM_CHANNEL_3, TIM_BreakDeadTimeConfigStruct.DeadTime-1);
	__HAL_TIM_SET_COMPARE(&TIM_HandleStruct_TIM1, TIM_CHANNEL_4, TIM_BreakDeadTimeConfigStruct.DeadTime-1);
	HAL_TIM_Base_Start(&TIM_HandleStruct_TIM1);
	HAL_TIM_PWM_Start(&TIM_HandleStruct_TIM1, TIM_CHANNEL_1);
	HAL_TIMEx_PWMN_Start(&TIM_HandleStruct_TIM1, TIM_CHANNEL_1);
	HAL_TIM_PWM_Start(&TIM_HandleStruct_TIM1, TIM_CHANNEL_2);
	HAL_TIMEx_PWMN_Start(&TIM_HandleStruct_TIM1, TIM_CHANNEL_2);
	HAL_TIM_PWM_Start(&TIM_HandleStruct_TIM1, TIM_CHANNEL_3);
	HAL_TIMEx_PWMN_Start(&TIM_HandleStruct_TIM1, TIM_CHANNEL_3);
	HAL_TIM_PWM_Start(&TIM_HandleStruct_TIM1, TIM_CHANNEL_4);

	GPIOE->BSRR = GPIO_PIN_4;
	HAL_Delay(1);

	/* Ramp PWM TIM1 CH1-4 up to the desired duty values (28%, 42%, 56%, 70%) */

	while (__HAL_TIM_GET_COMPARE(&TIM_HandleStruct_TIM1, TIM_CHANNEL_1) != old_duty_ch1 ||
	       __HAL_TIM_GET_COMPARE(&TIM_HandleStruct_TIM1, TIM_CHANNEL_2) != old_duty_ch2 ||
	       __HAL_TIM_GET_COMPARE(&TIM_HandleStruct_TIM1, TIM_CHANNEL_3) != old_duty_ch3 ||
	       __HAL_TIM_GET_COMPARE(&TIM_HandleStruct_TIM1, TIM_CHANNEL_4) != old_duty_ch4) {
		if (__HAL_TIM_GET_COMPARE(&TIM_HandleStruct_TIM1, TIM_CHANNEL_1) != old_duty_ch1)
			__HAL_TIM_SET_COMPARE(&TIM_HandleStruct_TIM1, TIM_CHANNEL_1,
					      __HAL_TIM_GET_COMPARE(&TIM_HandleStruct_TIM1, TIM_CHANNEL_1) + 1);
		if (__HAL_TIM_GET_COMPARE(&TIM_HandleStruct_TIM1, TIM_CHANNEL_2) != old_duty_ch2)
			__HAL_TIM_SET_COMPARE(&TIM_HandleStruct_TIM1, TIM_CHANNEL_2,
					      __HAL_TIM_GET_COMPARE(&TIM_HandleStruct_TIM1, TIM_CHANNEL_2) + 1);
		if (__HAL_TIM_GET_COMPARE(&TIM_HandleStruct_TIM1, TIM_CHANNEL_3) != old_duty_ch3)
			__HAL_TIM_SET_COMPARE(&TIM_HandleStruct_TIM1, TIM_CHANNEL_3,
					      __HAL_TIM_GET_COMPARE(&TIM_HandleStruct_TIM1, TIM_CHANNEL_3) + 1);
		if (__HAL_TIM_GET_COMPARE(&TIM_HandleStruct_TIM1, TIM_CHANNEL_4) != old_duty_ch4)
			__HAL_TIM_SET_COMPARE(&TIM_HandleStruct_TIM1, TIM_CHANNEL_4,
					      __HAL_TIM_GET_COMPARE(&TIM_HandleStruct_TIM1, TIM_CHANNEL_4) + 1);
		HAL_Delay(1);
	}
}

static void LED_SetControlWords(uint16_t (*buf)[3][256])
{
	(*buf)[0][0]   = 0xa035;
	(*buf)[0][16]  = 0xa115;
	(*buf)[0][32]  = 0xa225;
	(*buf)[0][48]  = 0xa335;
	(*buf)[0][64]  = 0xa445;
	(*buf)[0][80]  = 0xa555;
	(*buf)[0][96]  = 0xa665;
	(*buf)[0][112] = 0xa775;
	(*buf)[0][128] = 0xa885;
	(*buf)[0][144] = 0xa995;
	(*buf)[0][160] = 0xa0a5;
	(*buf)[0][176] = 0xa0b5;
	(*buf)[0][192] = 0xa0c5;
	(*buf)[0][208] = 0xa0d5;
	(*buf)[0][224] = 0x00e6;
	(*buf)[0][240] = 0x0006;

	(*buf)[1][224] = 0x00e5;
	(*buf)[1][240] = 0x0005;

	(*buf)[2][224] = 0x00e3;
	(*buf)[2][240] = 0x0003;

}

static void LED_Set_Start_Packet(uint16_t value)
{
	uint16_t cword = (value << 4) | 0x0008;
	int i;
	for (i=0; i<16; i++)
		LED_Start_Buffer[i] = cword;
	LED_Mode = 2;
}

void LED_Start(void)
{
	uint16_t duty, old_duty, tim4_old_duty[2];

	tim4_old_duty[0] = __HAL_TIM_GET_COMPARE(&TIM_HandleStruct_TIM4, TIM_CHANNEL_1);
	tim4_old_duty[1] = __HAL_TIM_GET_COMPARE(&TIM_HandleStruct_TIM4, TIM_CHANNEL_2);

	__HAL_TIM_SET_COMPARE(&TIM_HandleStruct_TIM4, TIM_CHANNEL_1, 5);
	__HAL_TIM_SET_COMPARE(&TIM_HandleStruct_TIM4, TIM_CHANNEL_2, 5);


	/* Ramp up PWM TIM11 CH1 from 0 to the preset duty value (20%) */

	old_duty = __HAL_TIM_GET_COMPARE(&TIM_HandleStruct_TIM11, TIM_CHANNEL_1);
	__HAL_TIM_SET_COMPARE(&TIM_HandleStruct_TIM11, TIM_CHANNEL_1, 0);
	HAL_TIM_Base_Start(&TIM_HandleStruct_TIM11);
	HAL_TIM_PWM_Start(&TIM_HandleStruct_TIM11, TIM_CHANNEL_1);

	LL_GPIO_SetOutputPin(GPIOB, LL_GPIO_PIN_2);

	while (ADC_ExtraChannels[ADC_EXTRACHANNEL_15_2] <= 2500)
	       HAL_Delay(1);

	for (duty = 0; duty <= old_duty; duty++) {
		HAL_Delay(10);
		__HAL_TIM_SET_COMPARE(&TIM_HandleStruct_TIM11, TIM_CHANNEL_1, duty);
	}

	LL_GPIO_ResetOutputPin(GPIOC, LL_GPIO_PIN_8);


	/* Ramp up PWM TIM2 CH3 from 0 to the preset duty value (100%) */

	old_duty = __HAL_TIM_GET_COMPARE(&TIM_HandleStruct_TIM2, TIM_CHANNEL_3);
	__HAL_TIM_SET_COMPARE(&TIM_HandleStruct_TIM2, TIM_CHANNEL_3, 0);
	HAL_TIM_Base_Start(&TIM_HandleStruct_TIM2);
	HAL_TIM_PWM_Start(&TIM_HandleStruct_TIM2, TIM_CHANNEL_3);
	for (duty = 0; duty <= old_duty; duty++) {
		HAL_Delay(1);
		__HAL_TIM_SET_COMPARE(&TIM_HandleStruct_TIM2, TIM_CHANNEL_3, duty);
	}


	/* Start PWM TIM4 CH1 and CH2 */

	HAL_TIM_Base_Start(&TIM_HandleStruct_TIM4);
	HAL_TIM_PWM_Start(&TIM_HandleStruct_TIM4, TIM_CHANNEL_1);
	HAL_TIM_PWM_Start(&TIM_HandleStruct_TIM4, TIM_CHANNEL_2);

	LL_GPIO_SetOutputPin(GPIOE, LL_GPIO_PIN_5);


	LED_Start_TIM1();


	/* Restore PWM TIM4 CH1 & CH2 duty cycle (43.75% and 60.16%) */

	__HAL_TIM_SET_COMPARE(&TIM_HandleStruct_TIM4, TIM_CHANNEL_1, tim4_old_duty[0]);
	__HAL_TIM_SET_COMPARE(&TIM_HandleStruct_TIM4, TIM_CHANNEL_2, tim4_old_duty[1]);

	/* Start PWM TIM9 CH2 */

	HAL_TIM_Base_Start(&TIM_HandleStruct_TIM9);
	HAL_TIM_PWM_Start(&TIM_HandleStruct_TIM9, TIM_CHANNEL_2);


	/* Start PWM TIM10 CH 1 */

	__HAL_TIM_SET_COMPARE(&TIM_HandleStruct_TIM10, TIM_CHANNEL_1,
			      __HAL_TIM_GET_AUTORELOAD(&TIM_HandleStruct_TIM10) - 299);
	HAL_Delay(6);
	/* First in one-shot mode */
	TIM10->CR1 |= TIM_CR1_CEN | TIM_CR1_OPM;
	HAL_TIM_PWM_Start(&TIM_HandleStruct_TIM10, TIM_CHANNEL_1);
	HAL_Delay(10);
	/* Switch to continuous PWM */
	TIM10->CR1 &= ~TIM_CR1_OPM;
	TIM10->CR1 |= TIM_CR1_CEN;

	HAL_Delay(300);

	LED_Set_Start_Packet(7);

	/* Set control words for update buffer */
	LED_SetControlWords(&LED_Update_Buffer[0]);
	LED_SetControlWords(&LED_Update_Buffer[1]);
	LED_SetControlWords(&LED_Update_Buffer[2]);
	LED_SetControlWords(&LED_Update_Buffer[3]);


	/* Enable interrupt */

	__HAL_TIM_ENABLE_IT(&TIM_HandleStruct_TIM10, TIM_IT_UPDATE);

	HAL_Delay(10);
}

void LED_Set_LED(uint8_t id, uint8_t c0, uint8_t c1, uint8_t c2)
{
	if (id <= LED_ID_MAX) {
		unsigned offs = ((id&0xfu) << 4) + (id >> 4) + 7;
		LED_Update_Buffer[0][0][offs] = c0 * LED_Brightness;
		LED_Update_Buffer[0][1][offs] = c1 * LED_Brightness;
		LED_Update_Buffer[0][2][offs] = c2 * LED_Brightness;
	}
}

void LED_Set_LED_RGB(uint8_t id, uint8_t r, uint8_t g, uint8_t b)
{
	if (id <= LED_ID_MAX) {
		unsigned offs = ((id&0xfu) << 4) + (id >> 4) + 7;
		switch(LED_RGB_Map[id]) {
		case 0:
			LED_Update_Buffer[0][0][offs] = r * LED_Brightness;
			LED_Update_Buffer[0][1][offs] = g * LED_Brightness;
			LED_Update_Buffer[0][2][offs] = b * LED_Brightness;
			break;
		case 1:
			LED_Update_Buffer[0][0][offs] = b * LED_Brightness;
			LED_Update_Buffer[0][1][offs] = r * LED_Brightness;
			LED_Update_Buffer[0][2][offs] = g * LED_Brightness;
			break;
		case 2:
			LED_Update_Buffer[0][0][offs] = g * LED_Brightness;
			LED_Update_Buffer[0][1][offs] = b * LED_Brightness;
			LED_Update_Buffer[0][2][offs] = r * LED_Brightness;
			break;
		}
	}
}

void LED_Set_Key_RGB(uint8_t kc, uint8_t r, uint8_t g, uint8_t b)
{
	if (kc <= KEY_CODE_MAX) {
		uint8_t pos = LED_Key_Map[kc];
		if (pos >= 0xc0) {
			const uint8_t *p = &LED_Key_MultiMap[pos-0xc0];
			while ((pos = *p++) != 0xff)
				LED_Set_LED_RGB(pos, r, g, b);
		} else
			LED_Set_LED_RGB(pos, r, g, b);
	}
}

/* Note: rgb points to 16 red values, followed by 16 green values, followed by 16 blue values */
void LED_Set_ColumnEffect(void *buffer, unsigned column, const uint8_t *rgb)
{
	if (buffer == NULL || column > LED_COLUMN_MAX || rgb == NULL)
		return;
	uint16_t (*buf)[3][256] = buffer;
	const uint8_t *map = &LED_RGB_Map[column << 4];
	unsigned offs = column + 7;
	do {
		uint16_t b = rgb[32] * LED_Brightness;
		uint16_t g = rgb[16] * LED_Brightness;
		uint16_t r = *rgb++  * LED_Brightness;
		switch(*map++) {
		case 0:
			(*buf)[0][offs] = r;
			(*buf)[1][offs] = g;
			(*buf)[2][offs] = b;
			break;
		case 1:
			(*buf)[0][offs] = b;
			(*buf)[1][offs] = r;
			(*buf)[2][offs] = g;
			break;
		case 2:
			(*buf)[0][offs] = g;
			(*buf)[1][offs] = b;
			(*buf)[2][offs] = r;
			break;
		}
		offs += 16;
	} while(offs < 256);
}

void *LED_GetEffectBuffer(void)
{
	unsigned nb = LED_Next_Buffer;
	unsigned cb = LED_Current_Buffer;
	
	if (cb && !nb)
		/* Wait for previous effect to clear out */
		return NULL;

	if (!cb && nb == 0xe)
		/* Wait for effect to start before
		   reusing buffers*/
		return NULL;

	/* Clear used flag of current and preceeding;
	   we guard explicitly against reusing current
	   below so it's sage to mark it as "unused". */
	unsigned sb = cb;
	while ((nb & (1<<sb))) {
		nb &= ~(1<<sb);
		if (!--sb)
			sb = 3;
	}
	LED_Next_Buffer = nb;

	/* Find first unused buffer after current */
	sb = cb;
	do {
		if (++sb > 3)
			sb = 1;
	} while ((nb & (1<<sb)));

	/* If only current is "unused", abort */
	if (sb == cb)
		return NULL;

	return LED_Update_Buffer[sb];
}

void LED_CommitEffectBuffer(void *buf)
{
	LED_Next_Buffer |= 1 << (((uint16_t (*)[3][256])buf) - &LED_Update_Buffer[0]);
}

void LED_ClearEffect(void)
{
	LED_Next_Buffer = 0;
}

void LED_AdjustBrightness(int delta)
{
	if (delta > 255 - LED_Brightness)
		LED_Brightness = 255;
	else if (delta < -(LED_Brightness - 25))
		LED_Brightness = 25;
	else
		LED_Brightness += delta;
}
