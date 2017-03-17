/**
* File Name		 : stepper2.c
* PROJECT		 : CNTR8000 Assignment #8
* PROGRAMMERS	 : Abdel Alkuor, Nimal Kirshna(supervisor) (Group #2)
* FIRST VERSION  : 2017-03-15
* Description    : This lab demonstrates using three timer channels to
				   generate pulse width modulation(PWM) with different
				   duty cycles determined by the user. This code also
				   turns on a RGB LED in breathing behaviour.
*/

#include "common.h"
#include <stdlib.h>
#include "stm32f3xx_hal.h"
#include <stdio.h>
#include <string.h>
#include "stm32f3_discovery.h"

TIM_HandleTypeDef htim2;
TIM_HandleTypeDef htim3;
TIM_HandleTypeDef htim8;
TIM_HandleTypeDef htim15;

#define RESOLUTION			6
#define RED_LED				1
#define GREEN_LED			1
#define BLUE_LED			1
#define DUTYCYCLE_MAPPER	2.55
#define FULL_LED_CYCLE		41
#define MAX_INDEX			2
volatile uint8_t levelUpdate = 0;
volatile uint8_t dimFlag = 0;
volatile uint8_t stopFlag = 1;
float diagonalArray [3][3]={{RED_LED,0,0},{0,GREEN_LED,0},{0,0,BLUE_LED}};// This array can be changed with values between 0 and 1
volatile int RGBIndex=0;												  // to have different colors		
int ledSwitcher=0;
void HAL_TIM_MspPostInit(TIM_HandleTypeDef* htim);
void MX_GPIO_Init(void);
void MX_TIM2_Init(void);
void MX_TIM3_Init(void);
void MX_TIM8_Init(void);
void MX_TIM15_Init(void);
void RGBColorMixer(uint8_t redIntensity, uint8_t greenIntensity, uint8_t blueIntensity);




/* FUNCTION      : RGBColorMixer
 * DESCRIPTION   : This function mixes the colors of red, green, and blue using PWM generator as follows:
 * 					Red pin   ---> 150ohms --->PA3
 * 					Ground	  ---------------->GND
 *					Green pin ---> 100ohms --->PC9
 *					Blue pin  ---> 100ohms --->PF10
 * PARAMETERS    : redIntensity   : Unsigned 8bit integer (0-255)
 	 	 	 	   greenIntensity : Unsigned 8bit integer (0-255)
 	 	 	 	   blueIntensity  : Unsigned 8bit integer (0-255)
 * RETURNS       : NULL
*/
void RGBColorMixer(uint8_t redIntensity, uint8_t greenIntensity, uint8_t blueIntensity)
{
	__HAL_TIM_SetCompare(&htim2,  TIM_CHANNEL_4, redIntensity);
	__HAL_TIM_SetCompare(&htim8,  TIM_CHANNEL_4, greenIntensity);
	__HAL_TIM_SetCompare(&htim15, TIM_CHANNEL_2, blueIntensity);
}


/* FUNCTION      : pwminit
 * DESCRIPTION   : This function intializes the pwm for
				   timer2 channel_4, timer8,channel_4
				   timer15 channel_2
 * 					
 * PARAMETERS    : NULL
 * RETURNS       : NULL
*/
void pwminit(int mode)
{
	MX_GPIO_Init();
	MX_TIM2_Init();
	MX_TIM3_Init();
	MX_TIM8_Init();
	MX_TIM15_Init();
	HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_4);
	HAL_TIM_PWM_Start(&htim8, TIM_CHANNEL_4);
	HAL_TIM_PWM_Start(&htim15, TIM_CHANNEL_2);
}
ADD_CMD("pwminit",pwminit, " Initialize PWM15,PWM3,PWM8")

/* FUNCTION      : breathingLED
 * DESCRIPTION   : This function turns on RGB LED in
				   breathing in and out fashion where
				   it alters the color for each full(turning on
				   and off) cycle. RED--->GREEN--->BLUE and so on.
 * 				   this function uses timer 3 as an interrupt to 
				   update the duty-cycle factor for each pwm channel	
 * PARAMETERS    : NULL
 * RETURNS       : NULL
*/

void breathingLED(int mode)
{
	HAL_TIM_Base_Start_IT(&htim3);
	while(stopFlag)
	{
	
		RGBColorMixer(levelUpdate*RESOLUTION*diagonalArray[0][RGBIndex],\
					  diagonalArray[1][RGBIndex]*levelUpdate*RESOLUTION,\
					  diagonalArray[2][RGBIndex]*levelUpdate*RESOLUTION);
	}
	HAL_TIM_Base_Stop_IT(&htim3);
	RGBColorMixer(0,0,0);
	stopFlag = 1;
} 
ADD_CMD("breathingLED",breathingLED, " Initialize PWM15,PWM3,PWM8")

/* FUNCTION      : pwm
 * DESCRIPTION   : This function allows the user to choose
				   one of the pwm channels with the desired
				   duty cycle. Avaliable channels are 2,8 and
			       15, and where the duty-cycle is between 0-100	
 * PARAMETERS    : NULL
 * RETURNS       : NULL
*/


void pwm(int mode)
{
	int32_t selectedChannel;
	int rc;
	rc = fetch_int32_arg(&selectedChannel);
	if(rc) 
	{
		printf("select PWM channel,2, 8 or 15\n");
		return;
	} 
	if(selectedChannel!=2 && selectedChannel!=15 && selectedChannel!=8)
	{
		printf("Select channel either 2, 8, or 15\n");
		return;
	}

	int32_t dutyCycle;
	rc = fetch_int32_arg(&dutyCycle);
	if(rc) 
	{
		printf("duty cycle must be between 0 and 100\n");
		return;
	}	 
	if(dutyCycle<0 && dutyCycle>100) 
	{
		printf("duty cycle must be between 0 and 100\n");
		return;
	}
	dutyCycle = DUTYCYCLE_MAPPER * dutyCycle;// covert the user's input of duty cycle value(0-100) 
										     // to value between 0 and 255
	switch (selectedChannel) 
	{
		case 2:
			RGBColorMixer(dutyCycle,0,0);
			break;
		case 8:
			RGBColorMixer(0,dutyCycle,0);
			break;
		case 15:
			RGBColorMixer(0,0,dutyCycle);
			break;
		default:
			break;
	}
}
ADD_CMD("pwm",pwm, "choose channel 2, 8, 15 and duty cycle 0-100")


/* FUNCTION      : TIM3_IRQHandler
 * DESCRIPTION   : Timer 3 is used as timer interrupt
				   to update the duty-cycle slicing, it
				   updates the index of the next RGB color
				   and it monitors the user's pushbutton
 * PARAMETERS    : NULL
 * RETURNS       : NULL
*/


void TIM3_IRQHandler(void)
{
	int button = BSP_PB_GetState(BUTTON_USER);

	HAL_TIM_IRQHandler(&htim3);
	if(dimFlag == 0)
	{	
		if(levelUpdate > FULL_LED_CYCLE-1)
		{
			dimFlag = 1;
		}
		else
		{
			levelUpdate++;
		}
	}
	else
	{
		if(levelUpdate == 0)
		{
			dimFlag = 0;
		}
		else
		{
			levelUpdate--;
		}
		if(ledSwitcher<FULL_LED_CYCLE)
		{
			ledSwitcher++;
		}
		else
		{
			ledSwitcher=0;
			(RGBIndex==MAX_INDEX)? RGBIndex=0: RGBIndex++;
		}
	}
	
	if(button == 1)
	{
		stopFlag = 0;
	}	
}
	
void MX_TIM2_Init(void)
{
	TIM_MasterConfigTypeDef sMasterConfig;
	TIM_OC_InitTypeDef sConfigOC;
	
	htim2.Instance = TIM2;
	htim2.Init.Prescaler = 72;
	htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
	htim2.Init.Period = 255;
	htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
	
	if(HAL_TIM_PWM_Init(&htim2) != HAL_OK)
	{
		printf("Error in Time 2 init\n");
		return;
	}

	sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
	sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
	if(HAL_TIMEx_MasterConfigSynchronization(&htim2,&sMasterConfig) != HAL_OK)
	{
		printf("Error in Time2 MasterConfig init\n");
	}
	sConfigOC.OCMode = TIM_OCMODE_PWM1;
	sConfigOC.Pulse = 0;
	sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
	sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;

	if(HAL_TIM_PWM_ConfigChannel(&htim2, &sConfigOC, TIM_CHANNEL_4) != HAL_OK)
	{
		printf("Error in Time 2 PWM init\n");
		return;
	}

	HAL_TIM_MspPostInit(&htim2);
}

void MX_TIM3_Init(void)
{
	TIM_ClockConfigTypeDef sClockSourceConfig;
	TIM_MasterConfigTypeDef sMasterConfig;
	htim3.Instance = TIM3;
	htim3.Init.Prescaler = 7200;
	htim3.Init.CounterMode = TIM_COUNTERMODE_UP;
	htim3.Init.Period = 550;
	htim3.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
	
	if(HAL_TIM_Base_Init(&htim3) != HAL_OK)
	{
		printf("Error in Time 2 init\n");
		return;
	}

	sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
	if(HAL_TIM_ConfigClockSource(&htim3, &sClockSourceConfig) != HAL_OK)
	{
		printf("Error in Time 3 init\n");
		return;
	}

	sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
	sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
	if(HAL_TIMEx_MasterConfigSynchronization(&htim3,&sMasterConfig) != HAL_OK)
	{
		printf("Error in Time2 MasterConfig init\n");
		return;
	}
				
}
void MX_TIM8_Init(void)
{
	TIM_MasterConfigTypeDef sMasterConfig;
	TIM_OC_InitTypeDef sConfigOC;
	TIM_BreakDeadTimeConfigTypeDef sBreakDeadTimeConfig;
	htim8.Instance = TIM8;
	htim8.Init.Prescaler = 72;
	htim8.Init.CounterMode = TIM_COUNTERMODE_UP;
	htim8.Init.Period = 255;
	htim8.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
	htim8.Init.RepetitionCounter = 0;
	if(HAL_TIM_PWM_Init(&htim8) != HAL_OK)
	{
		printf("Error in Time 2 init\n");
		return;
	}

	sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
	sMasterConfig.MasterOutputTrigger2 = TIM_TRGO2_RESET;
	sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
	if(HAL_TIMEx_MasterConfigSynchronization(&htim8,&sMasterConfig) != HAL_OK)
	{
		printf("Error in Time2 MasterConfig init\n");
		return;
	}
	sConfigOC.OCMode = TIM_OCMODE_PWM1;
	sConfigOC.Pulse = 0;
	sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
	sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
	sConfigOC.OCIdleState = TIM_OCIDLESTATE_RESET;
	sConfigOC.OCNIdleState = TIM_OCNIDLESTATE_RESET;
	if(HAL_TIM_PWM_ConfigChannel(&htim8, &sConfigOC, TIM_CHANNEL_4) != HAL_OK)
	{
		printf("Error in Time 2 PWM init\n");
		return;
	}

	sBreakDeadTimeConfig.OffStateRunMode = TIM_OSSR_DISABLE;
	sBreakDeadTimeConfig.OffStateIDLEMode = TIM_OSSI_DISABLE;
	sBreakDeadTimeConfig.LockLevel = TIM_LOCKLEVEL_OFF;
	sBreakDeadTimeConfig.DeadTime = 0;
	sBreakDeadTimeConfig.BreakState = TIM_BREAK_DISABLE;
	sBreakDeadTimeConfig.BreakPolarity = TIM_BREAKPOLARITY_HIGH;
	sBreakDeadTimeConfig.BreakFilter = 0;
	sBreakDeadTimeConfig.Break2State = TIM_BREAK2_DISABLE;
	sBreakDeadTimeConfig.Break2Polarity = TIM_BREAK2POLARITY_HIGH;
	sBreakDeadTimeConfig.Break2Filter = 0;
	sBreakDeadTimeConfig.AutomaticOutput = TIM_AUTOMATICOUTPUT_DISABLE;
	if (HAL_TIMEx_ConfigBreakDeadTime(&htim8, &sBreakDeadTimeConfig) != HAL_OK)
	{
		printf("Error in Time 2 PWM init\n");
		return;
	}

	 HAL_TIM_MspPostInit(&htim8);
}

void MX_TIM15_Init(void)
{
	TIM_MasterConfigTypeDef sMasterConfig;
	TIM_OC_InitTypeDef sConfigOC;
	TIM_BreakDeadTimeConfigTypeDef sBreakDeadTimeConfig;
	htim15.Instance = TIM15;
	htim15.Init.Prescaler = 72;
	htim15.Init.CounterMode = TIM_COUNTERMODE_UP;
	htim15.Init.Period = 255;
	htim15.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
	htim15.Init.RepetitionCounter = 0;
	if (HAL_TIM_PWM_Init(&htim15) != HAL_OK)
	{
		printf("Error in Time 2 PWM init\n");
		return;
	}
	sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
	sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
	if (HAL_TIMEx_MasterConfigSynchronization(&htim15, &sMasterConfig) != HAL_OK)
	{
		printf("Error in Time 2 PWM init\n");
		return;
	}
	sConfigOC.OCMode = TIM_OCMODE_PWM1;
	sConfigOC.Pulse = 0;
	sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
	sConfigOC.OCNPolarity = TIM_OCNPOLARITY_HIGH;
	sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
	sConfigOC.OCIdleState = TIM_OCIDLESTATE_RESET;
	sConfigOC.OCNIdleState = TIM_OCNIDLESTATE_RESET;

	if (HAL_TIM_PWM_ConfigChannel(&htim15, &sConfigOC, TIM_CHANNEL_2) != HAL_OK)
	{
		printf("Error in Time 2 PWM init\n");
		return;
	}

	sBreakDeadTimeConfig.OffStateRunMode = TIM_OSSR_DISABLE;
	sBreakDeadTimeConfig.OffStateIDLEMode = TIM_OSSI_DISABLE;
	sBreakDeadTimeConfig.LockLevel = TIM_LOCKLEVEL_OFF;
	sBreakDeadTimeConfig.DeadTime = 0;
	sBreakDeadTimeConfig.BreakState = TIM_BREAK_DISABLE;
	sBreakDeadTimeConfig.BreakPolarity = TIM_BREAKPOLARITY_HIGH;
	sBreakDeadTimeConfig.BreakFilter = 0;
	sBreakDeadTimeConfig.AutomaticOutput = TIM_AUTOMATICOUTPUT_DISABLE;

	if (HAL_TIMEx_ConfigBreakDeadTime(&htim15, &sBreakDeadTimeConfig) != HAL_OK)
	{
		printf("Error in Time 2 PWM init\n");
		return;
	}

	HAL_TIM_MspPostInit(&htim15);
}


void HAL_TIM_PWM_MspInit(TIM_HandleTypeDef* htim_pwm)
{
	if(htim_pwm->Instance==TIM2)
	{
		__TIM2_CLK_ENABLE();
	}
	else if(htim_pwm->Instance==TIM8)
	{
		__TIM8_CLK_ENABLE();
	}
	else if(htim_pwm->Instance==TIM15)
	{
		__TIM15_CLK_ENABLE();
	}
}

void HAL_TIM_Base_MspInit(TIM_HandleTypeDef* htim_base)
{
	if(htim_base->Instance==TIM3)
	{
		__TIM3_CLK_ENABLE();
		HAL_NVIC_SetPriority(TIM3_IRQn,0,0);
		HAL_NVIC_EnableIRQ(TIM3_IRQn);
	}
}

void HAL_TIM_MspPostInit(TIM_HandleTypeDef* htim)
{
	GPIO_InitTypeDef GPIO_InitStruct;
	if(htim->Instance==TIM2)
	{
		GPIO_InitStruct.Pin = GPIO_PIN_3;
		GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
		GPIO_InitStruct.Pull = GPIO_NOPULL;
		GPIO_InitStruct.Speed = GPIO_SPEED_LOW;
		GPIO_InitStruct.Alternate = GPIO_AF1_TIM2;
		HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
	}
	else if (htim->Instance==TIM8)
	{
		GPIO_InitStruct.Pin = GPIO_PIN_9;
		GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
		GPIO_InitStruct.Pull = GPIO_NOPULL;
		GPIO_InitStruct.Speed = GPIO_SPEED_LOW;
		GPIO_InitStruct.Alternate = GPIO_AF4_TIM8;
		HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);
	}	
	else if(htim->Instance==TIM15)
	{	
		GPIO_InitStruct.Pin = GPIO_PIN_10;
		GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
		GPIO_InitStruct.Pull = GPIO_NOPULL;
		GPIO_InitStruct.Speed = GPIO_SPEED_LOW;
		GPIO_InitStruct.Alternate = GPIO_AF3_TIM15;
		HAL_GPIO_Init(GPIOF, &GPIO_InitStruct);
	}
}

void MX_GPIO_Init(void)
{
	__GPIOF_CLK_ENABLE();
	__GPIOA_CLK_ENABLE();
	__GPIOC_CLK_ENABLE();
}

