/**
* File Name		 : stepper2.c
* PROJECT		 : CNTR8000 Assignment #7
* PROGRAMMERS	 : Abdel Alkuor
* FIRST VERSION  : 2017-03-08
* Description    : This program runs the stepper motor in microstepping mode
				   There are two main functions "stepspeed"and "AA_Stepper".
			       "stepspeed" function allow the user to enter the direction of
				   the spinning stepper motor with a delay that will set the
				   speed of the motor. The second function "AA_stepper"
	     		   allow the user to run queue motion triplet(speed,direction, and
				   value at which the motor will keep spinning. "AA_stepper" function
				   uses PWM(timer15, channel_1) to run stepper motor at variable
				   speeds, by changing the frequency of the PWM and keeping the
				   duty cycle 50% all the time, the speed of the motor can be controlled
				   where a linear model has been developed to run the motor to higher
				   speed > 200steps/sec and it is been calculated by using calculateARR.
				
				
				   LV8712T IC is used to drive the stepper motor and the pins are
				   connected as follows:
				   
				   RST---->PB0 
				   OE----->PB1
				   STEP--->PF9 when using AA_Stepper otherwise connect to PB5
				   ATT1--->0.0v
				   ATT2--->0.0v
				   MD1---->3.3v
				   MD2---->3.3v
				   FR----->PB3
				   VREF--->PA4
				   
				****NOTE:
**/

#include "common.h"
#include <stdlib.h>
#include "stm32f3xx_hal.h"
#include <stdio.h>
#include <string.h>


TIM_HandleTypeDef htim15;
TIM_HandleTypeDef htim16;
DAC_HandleTypeDef hdac;
TIM_OC_InitTypeDef sConfigOC;

#define QUEUE_MOTION_SIZE 	5
#define TIME_BETWEEN_SPEED 	10 // time in ms
#define RST_INACTIVE		HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, SET)
#define RST_ACTIVE			HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, RESET)
#define MONI_SET			HAL_GPIO_WritePin(GPIOB, GPIO_PIN_5, SET)
#define MONI_RESET			HAL_GPIO_WritePin(GPIOB, GPIO_PIN_5, RESET)
#define CW_DIRECTION		HAL_GPIO_WritePin(GPIOB,GPIO_PIN_3, RESET);
#define CCW_DIRECTION		HAL_GPIO_WritePin(GPIOB,GPIO_PIN_3, SET);

typedef struct{
	int32_t SPS; //step per second
	int32_t stepperDirection;
	int32_t timeToSpin; 
}queueMotion;


//Global variables
queueMotion userQueueMotion[QUEUE_MOTION_SIZE];
uint8_t i=0;
uint16_t Vref = 3000;// 1.5v
volatile int16_t delayCounter = 0;
volatile uint8_t delayFlag = 0;
volatile uint16_t microStepDelay = 0;
volatile uint8_t  microStepFlag = 1; 
uint32_t userDelay;		// delay between each step
volatile uint8_t speedFlag =0;


//functions prototypes
void HAL_TIM_MspPostInit(TIM_HandleTypeDef* htim);
int  setDirection(int32_t);
void calculateARR(float);


/* FUNCTION      : timerinit
 * DESCRIPTION   : This function initializes timer 15 and
				   timer 16
 * PARAMETERS    : int mode
 * RETURNS       : NULL
*/


void timerinit(void)
{
	TIM_MasterConfigTypeDef sMasterConfig;
	TIM_BreakDeadTimeConfigTypeDef sBreakDeadTimeConfig;
	htim15.Instance = TIM15;
	htim15.Init.Prescaler = 50;
	htim15.Init.CounterMode = TIM_COUNTERMODE_UP;
	htim15.Init.Period = 1000;
	htim15.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
	htim15.Init.RepetitionCounter = 0;

	if(HAL_TIM_PWM_Init(&htim15) != HAL_OK)
	{
		printf("Error in time base init\n");
		return;
	}
	
	sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
	sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;

	if(HAL_TIMEx_MasterConfigSynchronization(&htim15, &sMasterConfig) != HAL_OK)
	{
		printf("Error in time sConfig \n");
		return;
	}
	
	sConfigOC.OCMode = TIM_OCMODE_PWM1;
	sConfigOC.Pulse = 500;
	sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
	sConfigOC.OCNPolarity = TIM_OCNPOLARITY_HIGH;
	sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
	sConfigOC.OCIdleState = TIM_OCIDLESTATE_RESET;
	sConfigOC.OCNIdleState = TIM_OCNIDLESTATE_RESET;
	
	if(HAL_TIM_PWM_ConfigChannel(&htim15, &sConfigOC, TIM_CHANNEL_1) != HAL_OK)
	{
		printf("Error in time sConfigOC init\n");
		return;
	}	

	sBreakDeadTimeConfig.OffStateIDLEMode = TIM_OSSI_DISABLE;
	sBreakDeadTimeConfig.OffStateRunMode = TIM_OSSR_DISABLE;
	sBreakDeadTimeConfig.LockLevel = TIM_LOCKLEVEL_OFF;
	sBreakDeadTimeConfig.DeadTime = 0;
	sBreakDeadTimeConfig.BreakState = TIM_BREAK_DISABLE;
	sBreakDeadTimeConfig.BreakPolarity = TIM_BREAKPOLARITY_HIGH;
	sBreakDeadTimeConfig.BreakFilter = 0;
	sBreakDeadTimeConfig.AutomaticOutput = TIM_AUTOMATICOUTPUT_DISABLE;

	if(HAL_TIMEx_ConfigBreakDeadTime(&htim15, &sBreakDeadTimeConfig) != HAL_OK)
	{
		printf("Error in time ConfigBreakDeadTime init\n");
		return;
	}	
	
	HAL_TIM_MspPostInit(&htim15);
	
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

void HAL_TIM_PWM_MspInit(TIM_HandleTypeDef* htim_pwm)
{
	__TIM15_CLK_ENABLE();
}

void HAL_TIM_MspPostInit(TIM_HandleTypeDef* htim)
{
	GPIO_InitTypeDef GPIO_InitStruct;
	GPIO_InitStruct.Pin = GPIO_PIN_9;
	GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_LOW;
	GPIO_InitStruct.Alternate = GPIO_AF3_TIM15;
	HAL_GPIO_Init(GPIOF, &GPIO_InitStruct);
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
	__GPIOF_CLK_ENABLE();
	__GPIOA_CLK_ENABLE();
	__GPIOB_CLK_ENABLE();

	HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0|GPIO_PIN_1|GPIO_PIN_2|GPIO_PIN_3
						|GPIO_PIN_4|GPIO_PIN_5, GPIO_PIN_RESET);
	GPIO_InitStruct.Pin = GPIO_PIN_0|GPIO_PIN_1|GPIO_PIN_2|GPIO_PIN_3
						|GPIO_PIN_4|GPIO_PIN_5;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Pull = GPIO_PULLUP;
	GPIO_InitStruct.Speed = GPIO_SPEED_LOW;
	HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
}

			
void TIM1_UP_TIM16_IRQHandler(void)
{
	//time interval between each increase of SPS
	if(microStepDelay<TIME_BETWEEN_SPEED)
	{
		microStepDelay++;
		microStepFlag =1;
	}
	else
	{			
		microStepFlag = 0;
		microStepDelay = 0;
	}
	
	if(delayCounter<userDelay)
	{
		delayCounter++;
	}

	if(delayCounter==userDelay && speedFlag==1)
	{
		HAL_GPIO_TogglePin(GPIOB,GPIO_PIN_5);
		delayCounter = 0;
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
	HAL_GPIO_WritePin(GPIOB,GPIO_PIN_3|GPIO_PIN_1, RESET);
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

	HAL_GPIO_WritePin(GPIOB, GPIO_PIN_1, enable); //!!
	
}
ADD_CMD("stepperenable",stepperenable," 1 for enable 0 for disable")



/* FUNCTION      : stepSpeed
 * DESCRIPTION   :This function allow the user to 
				  set the speed of the stepper motor by choosing
				  proper delay in ms and the direction at which
				  the motor should spins, this function also allows
			      the user to override the previous delay and direction
				  by calling the function again with new delay and 
				  direction
		
 * PARAMETERS    : int mode
 * RETURNS       : NULL
*/
void stepSpeed(int mode)//!!!!!!
{	
	speedFlag = 0;	
	delayCounter = 0;
	int32_t userDirection;	
	RST_INACTIVE;
	MONI_SET;
	HAL_TIM_Base_Stop_IT(&htim16);
	int rc;
	rc = fetch_int32_arg(&userDirection);
	
	if(rc)
	{
		printf("Choose Direction Value: negative (CCW), 0(stop) or positive(CW)\n");
		return;
	}

	rc = fetch_uint32_arg(&userDelay);

	if(rc)
	{
		printf("Enter delay in ms\n");
		return;
	}

	if(userDirection>0)
	{
	HAL_GPIO_WritePin(GPIOB,GPIO_PIN_3, RESET);
	}
	else if(userDirection<0)
	{
		HAL_GPIO_WritePin(GPIOB, GPIO_PIN_3, SET);
	}
	else
	{
		HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, RESET);
		HAL_GPIO_WritePin(GPIOB, GPIO_PIN_5, RESET);
	}
	speedFlag = 1;	
	HAL_TIM_Base_Start_IT(&htim16);
	
	
}
ADD_CMD("stepspeed",stepSpeed,"Enter direction and delay value where direction value can be postive(cw) negative(ccw) or 0(stop)")


/* FUNCTION      : AA_Stepper
 * DESCRIPTION   : This function runs queue motion triplets
				   when the speed =< 200SPS it just runs the
				   stepper motor by calculating the proper 
				   frequency and apply that frequency in force
				   mode. But when the speed is > 200SPS 
				   the motor builds up the speed gradually to 
				   make sure that the motor doesn't get stalled.
		
 * PARAMETERS    : int mode
 * RETURNS       : NULL
*/

void AA_Stepper(int mode)
{	

	for(uint8_t n=0; n<i;n++)
	{	
		HAL_TIM_Base_Stop_IT(&htim16);
		int dir = 0;//motor direction value
		uint8_t speedFlag = 1;
		delayCounter = 0;
		userDelay = userQueueMotion[n].timeToSpin;
		MONI_SET;
		RST_INACTIVE;
		dir = setDirection(userQueueMotion[n].stepperDirection);
		HAL_TIM_Base_Start_IT(&htim16);

		while(delayCounter < userDelay)
		{	
			if((userQueueMotion[n].SPS <= 200) && dir && speedFlag)
			{
				speedFlag = 0;
				calculateARR(userQueueMotion[n].SPS);
			}
			else if((userQueueMotion[n].SPS > 200) && dir && speedFlag)
			{	
				speedFlag = 0;
				for(float j=200 ; j < (userQueueMotion[n].SPS)+1; j++)
				{
					calculateARR(j);//everytime microStepFlag is reset ARR is upgraded
					while(microStepFlag);
					microStepFlag = 1; // the flag should be set directly otherwise
				}					   // j will increase so quickly because it takes
			}						   // at 1 ms to set the flag from the interrupt 
			else if(speedFlag)
			{
				MONI_RESET;
				RST_INACTIVE;	
				speedFlag = 0;
			}
		}
	}

	HAL_TIM_PWM_Stop(&htim15, TIM_CHANNEL_1);
}	
ADD_CMD("AA_Stepper",AA_Stepper," select # of steps in trapezoidal mode ")

/* FUNCTION      : addMotionQueue
 * DESCRIPTION   : you can add queue motion triplet
				   using this function, first Speed/sec,
				   then stepper direction value which can
				   be either positive(CW), negative(CCW) or
				   0 values, and the last value in the set 
				   is the time at which the motor spinning
				   will last.
		
 * PARAMETERS    : int mode
 * RETURNS       : NULL
*/

void addMotionQueue(int mode)
{
	int rc;
	if(i == QUEUE_MOTION_SIZE)
	{
	  	printf("Queue motion is full\n");
		return;
	}

	rc = fetch_int32_arg(&(userQueueMotion[i].SPS));

	if(rc || (userQueueMotion[i].SPS) < 0)
	{
		printf("Enter speed bigger than 0\n");
		printf("Re-enter the whole triplet\n");
		return;
	}

	rc = fetch_int32_arg(&(userQueueMotion[i].stepperDirection));

	if(rc)
	{
		printf("Forgot to enter direction which can be postive, negative or 0 value\n");
		printf("Re-enter the whole triplet\n");
		return;
	}
	
	rc = fetch_int32_arg(&(userQueueMotion[i].timeToSpin));

	if(rc || (userQueueMotion[i].timeToSpin) < 0)
	{
		printf("Enter time value bigger than 0\n");
		printf("Re-enter the whole triplet\n");
		return;
	}
	i++;
}


ADD_CMD("add_triplet",addMotionQueue," add queue motion triplet [speed,direction,time]")


/* FUNCTION      : printTripletList
 * DESCRIPTION   : this function prints queue motion
				   triples
		
 * PARAMETERS    : int mode
 * RETURNS       : NULL
*/
void printTripletList(int mode)
{	

	for(uint8_t j=0;j<i;j++)
	{
		printf("[%lu, %lu, %lu]\n", userQueueMotion[j].SPS,userQueueMotion[j].stepperDirection,userQueueMotion[j].timeToSpin);
	}

}
ADD_CMD("print_triplet",printTripletList," print queue motion triplets ")


/* FUNCTION      : setDirection
 * DESCRIPTION   : this function sets the stepper motor
				   direction either CW, CCW or no direction
				   where it stops the motor from spinning
		
 * PARAMETERS    : int32_t direction
 * RETURNS       : NULL
*/

int setDirection(int32_t direction)
{
	if(direction > 0)
	{
		CW_DIRECTION;
	}
	else if(direction < 0)
	{
		CCW_DIRECTION;
	}
	else
	{
		RST_ACTIVE;
		MONI_RESET;
		return 0;
	}
	return 1;	
}


/* FUNCTION      : calculateARR
 * DESCRIPTION   : This function calculates the ARR value (period)
				   of the PWM according to the provided speed(SPS)
				   by the user.The speed of the stepper motor depends
				   on the PWM frequency and in this program microstepping
				   motor mode is used( 9 microsteps = 1 step), thereforeSPS is calculated as follows:
					
				   			  (PWM frequency)
					  SPS =   _______________ ... equ.(1)...where the unit is steps/second
							         9

					Now we have relation between SPS and PWM frequency, we can use PWM frequency equation
					to calculate ARR as follows:
					
								               CPU_CLK 
					PWM frequency = ___________________________ Hz  .. equ.(2)....where CPU_CLK = 72MHz , Pre-scalar = 50			
									
									 (Pre-Scalar+1) x (ARR+1)
					

					plugging CPU_CLK and Pre-Scalar value equ.(2) becomes as follows:
		
								         1411765
					PWM frequency = ___________________ Hz			
									
									     (ARR+1)
					re-arranging equ.(2) and plugging equ.(1) into equ.(2) as follows:
				
							 1411765	     			  156862
					ARR = _____________ - 1 ===> ARR =   ________ - 1
														   	
 						     9 x SPS 					    SPS

					
 * PARAMETERS    : int32_t direction				
 * RETURNS       : NULL
*/


void calculateARR(float speed)
{	
	HAL_TIM_Base_Stop_IT(&htim16);
	HAL_TIM_PWM_Stop(&htim15, TIM_CHANNEL_1);	
	htim15.Init.Period = (uint32_t)((156862/speed)-1);
	HAL_TIM_PWM_Init(&htim15);
	sConfigOC.Pulse = (uint32_t)((htim15.Init.Period)/2.0); // pulse should be set half of ARR (period) to have 50% duty cycle
	HAL_TIM_PWM_ConfigChannel(&htim15, &sConfigOC, TIM_CHANNEL_1);
	HAL_TIM_PWM_Start(&htim15, TIM_CHANNEL_1);
	HAL_TIM_Base_Start_IT(&htim16);					
}



