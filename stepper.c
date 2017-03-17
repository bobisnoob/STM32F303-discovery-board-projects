/**
* File Name		 : stepper.c
* PROJECT		 : CNTR8000 Assignment #6
* PROGRAMMERS	 : Abdel Alkuor, 
* FIRST VERSION  : 2017-02-22
* Description    : This program runs the stepper motor in microstepping mode
				   with 16 microsteps. There are two main functions "step"
				   and "trapestep". "step" function allows the user to enter
				   the number of steps to run with a delay between each
     			   step	determined by the user. The second function "trapestep"
	     		   takes one input (steps) and it runs the stepper motor
				   in trapezoidal speed profile with 10% of the steps will be 
				   spent in acceleration, 80% of steps will be spent in constant
				   speed, then another 10% of the steps will be in deacceleration.
	
				   LV8712T IC is used to drive the stepper motor and the pins are
				   connected as follows:
				   
				   RST---->PB0 
				   OE----->PB1
				   STEP--->PB2
				   ATT1--->0.0v
				   ATT2--->0.0v
				   MD1---->3.3v
				   MD2---->3.3v
				   FR----->PB3
				   VREF--->PA4
				   MONI--->PB5

					****NOTE:
					
**/

#include "common.h"
#include <stdlib.h>
#include "stm32f3xx_hal.h"
#include <stdio.h>
#include <string.h>


#define CONSTANT_PERCENT		0.8       // constant speed percent of total step
#define ACCE_DEACCE_PERCENT	 	0.1		  // acceleration and deacceleration of total step
#define SPEED_INCREMENT		 	50					
#define ACCE_DEACCE_TIMER	 	50	      // time interval to increment or decrement in ms
#define MICROSTEPS				16		
#define TEN_MICRO_SECOND	 	0.000001
#define SPSTimerRegister2_MAX	2000
#define SPSTimerRegister2_MIN 	5.5


TIM_HandleTypeDef htim15;
TIM_HandleTypeDef htim16;
DAC_HandleTypeDef hdac;
	

int32_t userStep; 
volatile float SPSTimerRegister ; // step ON counter 
volatile float SPSTimerRegister2 ;// step off counter SPSTimerRegister2 should always be 2 x SPSTimerRegister
volatile uint8_t acceFlag = 0;
volatile uint8_t delayFlag = 0;
volatile uint16_t acceStep = 0; // steps counter in acceleration mode
volatile uint16_t deaccStep = 0;// steps counter in deacceleration mode
volatile uint16_t constantStep = 0; // steps counter in constant speed mode
volatile uint16_t SPS = 62; // step per second
uint32_t userDelay;		// delay between each step
uint16_t SPSTimerCounter = 0; // total SPSTimerRegister + SPSTimerRegister2
uint16_t acceDeacceCounter = 0;// counter between each acceleration or deacceleration 
uint16_t Vref = 3000;// 1.5v
volatile int16_t delayCounter = 0;



/* FUNCTION      : timerinit
 * DESCRIPTION   : This function initializes timer 17 and
				   timer 16
 * PARAMETERS    : int mode
 * RETURNS       : NULL
*/


void timerinit(void)
{
	
	htim15.Instance = TIM15;
	htim15.Init.Prescaler = 50;
	htim15.Init.CounterMode = TIM_COUNTERMODE_UP;
	htim15.Init.Period = 1000;
	htim15.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
	htim15.Init.RepetitionCounter = 0;
		
	if(HAL_TIM_Base_Init(&htim15) != HAL_OK)
	{
		printf("Error in time base init\n");
		return;
	}
	
	
	htim16.Instance = TIM16;
	htim16.Init.Prescaler = 1;
	htim16.Init.CounterMode = TIM_COUNTERMODE_UP;
	htim16.Init.Period = 35999;
	htim16.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
	htim16.Init.RepetitionCounter = 0;
	
	if(HAL_TIM_Base_Init(&htim16) != HAL_OK)
	{
		printf("Error in time base init\n");
		return;
	}
	
}

void HAL_TIM_Base_MspInit(TIM_HandleTypeDef *htim_base)
{
	if(htim_base->Instance==TIM17)
	{
		__TIM17_CLK_ENABLE();
		HAL_NVIC_SetPriority(TIM1_TRG_COM_TIM17_IRQn, 0, 0);
		HAL_NVIC_EnableIRQ(TIM1_TRG_COM_TIM17_IRQn);
	}
	else if(htim_base->Instance==TIM16)
	{
		__TIM16_CLK_ENABLE();
		HAL_NVIC_SetPriority(TIM1_UP_TIM16_IRQn, 0, 0);
		HAL_NVIC_EnableIRQ(TIM1_UP_TIM16_IRQn);
	}
}


void MX_DAC_Init(void)
{
	DAC_ChannelConfTypeDef sConfig;
	hdac.Instance = DAC;
	if(HAL_DAC_Init(&hdac) != HAL_OK)
	{
		printf("DAC ERROR\n");
	}
	
	sConfig.DAC_Trigger = DAC_TRIGGER_NONE;
	sConfig.DAC_OutputBuffer = DAC_OUTPUTBUFFER_ENABLE;
	
	if((HAL_DAC_ConfigChannel(&hdac,&sConfig,DAC_CHANNEL_1) != HAL_OK))
	{
		printf("Error in DAC config\n");
	}
}

void HAL_DAC_MspInit(DAC_HandleTypeDef *hdac)
{
	GPIO_InitTypeDef GPIO_InitStruct;
	__DAC1_CLK_ENABLE();
	GPIO_InitStruct.Pin = GPIO_PIN_4;
	GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
}

void MX_GPIO_Init(void)
{
	GPIO_InitTypeDef GPIO_InitStruct;

	__GPIOA_CLK_ENABLE();
	__GPIOB_CLK_ENABLE();

	HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0|GPIO_PIN_1|GPIO_PIN_2|GPIO_PIN_3
						|GPIO_PIN_4|GPIO_PIN_5, GPIO_PIN_RESET);
	GPIO_InitStruct.Pin = GPIO_PIN_0|GPIO_PIN_1|GPIO_PIN_2|GPIO_PIN_3
						|GPIO_PIN_4|GPIO_PIN_5;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_LOW;
	HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
}


void TIM1_TRG_COM_TIM17_IRQHandler(void)
{
	static uint16_t stepToTarget = 0;
	static uint8_t microStepCounter = 0;
	if(delayFlag ==1 || acceFlag==1)
	{
		if(stepToTarget < userStep)
		{
			if(SPSTimerCounter == (uint16_t)SPSTimerRegister) // step pin on time
			{
				HAL_GPIO_WritePin(GPIOB, GPIO_PIN_2, SET);
				SPSTimerCounter++;
				microStepCounter++;
			}
			else if (SPSTimerCounter == (uint16_t)SPSTimerRegister2) // step pin off time
			{
				HAL_GPIO_WritePin(GPIOB, GPIO_PIN_2, RESET);
				SPSTimerCounter = 0;
				microStepCounter++;
			}
			else
			{
				SPSTimerCounter++;
			}
		}
	

		if(microStepCounter == MICROSTEPS)// increment one step when microStepCounter is 16
		{
			microStepCounter = 0;
			stepToTarget++;
			delayFlag = 0;
			if(acceFlag ==1)
			{
				if(acceStep <= (ACCE_DEACCE_PERCENT * userStep)) // increase steps during acceleration
				{
					acceStep++;
				}
				//increase steps during constant speed 
				if(constantStep <= (CONSTANT_PERCENT * userStep) && acceStep > (ACCE_DEACCE_PERCENT * userStep))
				{
					constantStep++;
				}
				//increase steps during deacceleration
				if(constantStep > (CONSTANT_PERCENT * userStep) && deaccStep <= (ACCE_DEACCE_PERCENT * userStep))
				{
					deaccStep++;
				}
			}		
		}
		
	}
	// reset all flags and counters for next input step command 
	if(stepToTarget>=userStep) 
	{
		userStep=0;
		stepToTarget=0;
		microStepCounter = 0;
		SPSTimerCounter = 0;
		delayCounter = 0;
		delayFlag = 0;
		userDelay = 0;
		acceFlag = 0 ;
		acceStep =0;
		constantStep=0;
		deaccStep=0;
	}
	HAL_TIM_IRQHandler(&htim17);
}

			
void TIM1_UP_TIM16_IRQHandler(void)
{
	// acceleration calculations
	if((acceDeacceCounter == ACCE_DEACCE_TIMER) && (SPSTimerRegister2 > SPSTimerRegister2_MIN)
	   && (acceFlag == 1) && (acceStep <= (ACCE_DEACCE_PERCENT * userStep)))
	{
		SPS+=SPEED_INCREMENT;
		SPSTimerRegister2 = 1.0 / (SPS*TEN_MICRO_SECOND*MICROSTEPS);// change step pin on time
		SPSTimerRegister = SPSTimerRegister2 / 2.0; //change step pin off time
		acceDeacceCounter = 0;
		SPSTimerCounter = 0;
	}
	// deacceleration calculations
	if((deaccStep <= (ACCE_DEACCE_PERCENT*userStep)) && (SPSTimerRegister2 < SPSTimerRegister2_MAX)
	    && (acceDeacceCounter == ACCE_DEACCE_TIMER) && (constantStep > (userStep*CONSTANT_PERCENT))
	    && (acceFlag == 1))
	{
		SPS-=SPEED_INCREMENT; 
		SPSTimerRegister2 = 1.0 / (SPS*TEN_MICRO_SECOND*MICROSTEPS);// change step pin on time
		SPSTimerRegister = SPSTimerRegister2 / 2.0;//change step pin off time
		acceDeacceCounter = 0;
		SPSTimerCounter = 0;
	}

	if(acceDeacceCounter > ACCE_DEACCE_TIMER)
	{
		acceDeacceCounter = 0;
	}

	if(delayCounter==userDelay)
	{
		delayCounter = 0;
		delayFlag = 1;
	}

	if(acceFlag==0)
	{
		delayCounter++;
	}
	else
	{
		acceDeacceCounter++;
	}

	HAL_TIM_IRQHandler(&htim16);
}

/* FUNCTION      : stepperinit
 * DESCRIPTION   : This function initializes port pins for
					stepper motor driver
 * PARAMETERS    : int mode
 * RETURNS       : NULL
*/
void stepperinit(int mode)
{
	MX_GPIO_Init();
	MX_DAC_Init();
	timerinit();
	HAL_DAC_Start(&hdac, DAC_CHANNEL_1);
	HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0|GPIO_PIN_4|GPIO_PIN_5, SET);
	HAL_GPIO_WritePin(GPIOB, GPIO_PIN_1, RESET);
	HAL_DAC_SetValue(&hdac, DAC_CHANNEL_1,DAC_ALIGN_12B_R, Vref);// set Vref to 1.5v
}
ADD_CMD("stepperinit",stepperinit," initialize stepper motor")

/* FUNCTION      : stepperenable
 * DESCRIPTION   : This function enables or disables
				   the stepper motor 
 * PARAMETERS    : int mode
 * RETURNS       : NULL
*/

void stepperenable(int mode)
{
	uint32_t enable=0;
	int rc;
	rc = fetch_uint32_arg(&enable);
	if(rc)
	{
		printf(" Enter 0 (disable) or 1 (enable)\n");
		return;
	}
	if(enable==1)
	{
		enable =0;
	}
	else
	{
		enable =1;
	}

	HAL_GPIO_WritePin(GPIOB, GPIO_PIN_1, enable); 
	
}
ADD_CMD("stepperenable",stepperenable," 1 for enable 0 for disable")

/* FUNCTION      : step
 * DESCRIPTION   : This function let the user to select
					number of steps and delay between each
					step
 * PARAMETERS    : int mode
 * RETURNS       : NULL
*/
void step(int mode)
{
	HAL_TIM_Base_Stop_IT(&htim17);
	HAL_TIM_Base_Stop_IT(&htim16);
	int rc;
	rc = fetch_int32_arg(&userStep);
	if(rc)
	{
		printf("Enter number of steps\n");
		return;
	}
	rc = fetch_uint32_arg(&userDelay);
	if(rc)
	{
		printf("Enter delay in ms\n");
		return;
	}
	HAL_GPIO_WritePin(GPIOB,GPIO_PIN_3, RESET);
	SPSTimerRegister = 5;
	SPSTimerRegister2 = 10;
	delayCounter = 0;
	if(userStep<0)
	{
		userStep*=-1;
		HAL_GPIO_WritePin(GPIOB, GPIO_PIN_3, SET);
	}
	HAL_TIM_Base_Start_IT(&htim17);
	HAL_TIM_Base_Start_IT(&htim16);
}
ADD_CMD("step",step," select # of steps and delay")


/* FUNCTION      : trapestep
 * DESCRIPTION   : This function runs the stepper motor
				   in trapezoidal profile with 10% of steps
				   acceleration, 80% of steps stays at constant
			       speed and 10% of steps deacceleration
 * PARAMETERS    : int mode
 * RETURNS       : NULL
*/

void trapestep(int mode)
{
	HAL_TIM_Base_Stop_IT(&htim17);
	HAL_TIM_Base_Stop_IT(&htim16);
	int rc;
	rc = fetch_int32_arg(&userStep);
	if(rc)
	{
		printf("Enter number of steps\n");
		return;
	}
	acceFlag = 1;
	acceDeacceCounter= 0;
	SPSTimerRegister = 1000;
	SPSTimerRegister2 = 2000;
	HAL_TIM_Base_Start_IT(&htim17);
	HAL_TIM_Base_Start_IT(&htim16);
}
ADD_CMD("trapestep",trapestep," select # of steps in trapezoidal mode ")



