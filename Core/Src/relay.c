#include "relay.h"





void relay1(uint8_t state)
{
	if(state == 0)
	{
		HAL_GPIO_WritePin(OUT1_GPIO_Port, OUT1_Pin, GPIO_PIN_RESET);
	}
	else
	{
		HAL_GPIO_WritePin(OUT1_GPIO_Port, OUT1_Pin, GPIO_PIN_SET);
	}
}


void relay2(uint8_t state)
{
	if(state == 0)
	{
		HAL_GPIO_WritePin(OUT2_GPIO_Port, OUT2_Pin, GPIO_PIN_RESET);
	}
	else
	{
		HAL_GPIO_WritePin(OUT2_GPIO_Port, OUT2_Pin, GPIO_PIN_SET);
	}
}


void relay3(uint8_t state)
{
	if(state == 0)
	{
		HAL_GPIO_WritePin(OUT3_GPIO_Port, OUT3_Pin, GPIO_PIN_RESET);
	}
	else
	{
		HAL_GPIO_WritePin(OUT3_GPIO_Port, OUT3_Pin, GPIO_PIN_SET);
	}
}

void relay4(uint8_t state)
{
	if(state == 0)
	{
		HAL_GPIO_WritePin(OUT4_GPIO_Port, OUT4_Pin, GPIO_PIN_RESET);
	}
	else
	{
		HAL_GPIO_WritePin(OUT4_GPIO_Port, OUT4_Pin, GPIO_PIN_SET);
	}
}


void relay5(uint8_t state)
{
	if(state == 0)
	{
		HAL_GPIO_WritePin(OUT5_GPIO_Port, OUT5_Pin, GPIO_PIN_RESET);
	}
	else
	{
		HAL_GPIO_WritePin(OUT5_GPIO_Port, OUT5_Pin, GPIO_PIN_SET);
	}
}

void relay6(uint8_t state)
{
	if(state == 0)
	{
		HAL_GPIO_WritePin(OUT6_GPIO_Port, OUT6_Pin, GPIO_PIN_RESET);
	}
	else
	{
		HAL_GPIO_WritePin(OUT6_GPIO_Port, OUT6_Pin, GPIO_PIN_SET);
	}
}
