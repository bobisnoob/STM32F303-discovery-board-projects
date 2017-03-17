
 /*
  *FILE   		 		  :  timer17.c
  *PROJECT		 		  :	 CNTR8000-Hardware/Software Interfacing Assignment #5 (TIMER)
  *PROGRAMMER             :  Abdel Alkuor, Nimal Krishna
  *FIRST VERSION          :  15/02/2017
  *DESCRIPTION            :  This project is for creating a interrupt Timer based delay which can print "Awesome" to the serial window  
  *                          after a particular delay which is determined by the user.It also allows one to choose the operation 
  *                          mode of Timer 17, in either delay mode or frequency counter  mode.
  *                          Minicom commands are used to intialise the timer and timer delay can be send via serial port using the command 
  *                          timerdelay.when the timer is in frequency mode, it prints the frequency of the signal each time the frequency 
  *                          changes more than 15HZ.PA7 is used as the input channel for signal whose frequency is to be measured.
 */
 
 /* Includes ------------------------------------------------------------------*/
 
#include "common.h"
#include <stdlib.h>
#include "stm32f3xx_hal.h"
#include <stdio.h>
#include <string.h>

/* Global variables declaration ---------------------------------------------------*/

TIM_HandleTypeDef htim17;
#define MARGIN_ERROR 	15             // The margin of change required to detect a new frequency
#define ONE_MHZ 	 	1000000
#define ONE_MS_FACTOR	67
#define PRINT_COUNTER 5
volatile uint8_t freqFlag = 0;	    // Freq flag = 1 means freaquency measurement is complete 
volatile uint8_t delayFlag = 0;     // delay flag = 1 means delay measurement is complete 
uint32_t userDelay = 0;		    // delay user gave as input 
char *timerMode;		    // timer operation mode
char selectedMode[6]={0};	    // used to store the operation mode of the timer

/*
 * FUNCTION	: main function
 *
 * DESCRIPTION 	: This function initialize the timer 17 , and also allow one to choose timer 17 in either delay mode or frequency counter mode
 *
 * PARAMETERS	: int mode
 *
 * RETURNS	: void

 */

void timerinit(int mode)
{
	int rc;
	
	rc = fetch_string_arg(&timerMode);	        // read the timer operation mode typed
	if(rc)						               // user didn't enter the timer operation mode
	{
		printf(" Choose timer 17 mode: delay or fc\n");        //fc= frequency counter
		return;
	}
	if(strcmp(timerMode,"delay")!=0 && strcmp(timerMode,"fc")!=0)  // user entered invalid timer mode
	{
		printf(" The input type is incorrect\n");
		return;
	}

	for(int i=0; i<6 ; i++)				// each time user inputs a timer mode, select mode is initialised to zero  before copying the timer 
                                        // mode entered to the select mode, this is done to remove the previous stored timer mode
	{
		selectedMode[i]=0;
	}	
	strcpy(selectedMode,timerMode);  // timer mode is stored in the select mode
	__GPIOA_CLK_ENABLE();	         // as PA7 is used for frequency counter input
	htim17.Instance = TIM17;
	htim17.Init.Prescaler = 71;
	htim17.Init.CounterMode = TIM_COUNTERMODE_UP;
	htim17.Init.Period = 0xffff;
	htim17.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
	htim17.Init.RepetitionCounter = 0;
	
	if(HAL_TIM_Base_Init(&htim17) != HAL_OK)        // Timer initiatization didn't happend correctly
	{
		printf("Error in time base init\n");
		return;
	}

	if(strcmp(selectedMode,"fc")==0)                // Timer in frequency counter mode
	{
		TIM_IC_InitTypeDef sConfigIC;
		if(HAL_TIM_IC_Init(&htim17) != HAL_OK)  // Timer initiatization didn't happend correctly
		{
			printf("Error in Timer IC init\n");
			return;
		}
	
		sConfigIC.ICPolarity = TIM_INPUTCHANNELPOLARITY_RISING; // Timer17 to detect rising pulse of the input signal
		sConfigIC.ICSelection = TIM_ICSELECTION_DIRECTTI;
		sConfigIC.ICFilter = 0;
	
		if(HAL_TIM_IC_ConfigChannel(&htim17,&sConfigIC, TIM_CHANNEL_1) !=HAL_OK)  // Timer not  properly configured in frequency 												  									  // counter mode
		{
			printf("Error in TIM IC config\n");
			return;
		}
	}
}
ADD_CMD("timerinit", timerinit," initiatization  for timer 17")

/*
 * FUNCTION	: HAL_TIM_Base_MspInit
 *
 * DESCRIPTION 	: This function configures timer 17, and also enables interrupt
 *
 * PARAMETERS	: TIM_HandleTypeDef *htim_base
 *
 * RETURNS	: void

 */

void HAL_TIM_Base_MspInit(TIM_HandleTypeDef *htim_base)
{
	GPIO_InitTypeDef GPIO_InitStruct;
	__TIM17_CLK_ENABLE();
	
	GPIO_InitStruct.Pin = GPIO_PIN_7;
	GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_LOW;
	GPIO_InitStruct.Alternate = GPIO_AF1_TIM17;
	HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
	
	HAL_NVIC_SetPriority(TIM1_TRG_COM_TIM17_IRQn, 0, 0);
	HAL_NVIC_EnableIRQ(TIM1_TRG_COM_TIM17_IRQn);
}


/*
 * FUNCTION	: timerdeinit
 *
 * DESCRIPTION 	: This is function disables timer 17
 *
 * PARAMETERS	: void
 *
 * RETURNS	: int mode

 */
void timerdeinit(int mode)
{
	__TIM17_CLK_DISABLE();
	HAL_NVIC_DisableIRQ(TIM1_TRG_COM_TIM17_IRQn);
}

ADD_CMD("timerdeinit", timerdeinit," deinitialize timer 17")

/*
 * FUNCTION	: TIM1_TRG_COM_TIM17_IRQHandler
 *
 * DESCRIPTION 	: This function is called every time timer interrupt is triggered
 *
 * PARAMETERS	: void
 *
 * RETURNS	: void
 *
 */

void TIM1_TRG_COM_TIM17_IRQHandler(void)
{	
	static uint16_t delayCounter=0;
	HAL_TIM_IRQHandler(&htim17);

	if(strcmp(selectedMode,"fc")==0)	  // Timer is in freaquency counter mode
	{
		__HAL_TIM_SetCounter(&htim17,0);  
		freqFlag = 1;			 		 // frequency measurement is compleate
	}
	else	                                  // Timer is in delay mode
	{ 
		if(userDelay==delayCounter)       //  delayCounter is increased till it equals userDelay to get the specific delay in ms
		{
			delayCounter = 0;         // delayCounter to be initialized for the next round of delay measurement
			delayFlag = 1;            // delay is compleate
		}
		delayCounter++;		          //  delayCounter is increased till it equals userDelay to get the specific  delay in ms
	}
	  	
}


/*
 * FUNCTION	: timerdelay
 *
 * DESCRIPTION 	: This  function configure timer 17 to measure specific delay
 *
 * PARAMETERS	: void
 *
 * RETURNS	: integer
 *
 */
void timerdelay(int mode)
{
	int rc;
	int j=0;
	delayFlag = 0;                       // delay measurement not complete
	rc = fetch_uint32_arg(&userDelay);   // timer delay value required is stored
	if(rc)                               // user didn't input delay value
	{
		printf("Delay value is required in ms\n");
		return;
	}
	userDelay /=ONE_MS_FACTOR; 
	if(strcmp(selectedMode,"delay")==0)      	// Timer is in delay mode
	{
		HAL_TIM_Base_Start_IT(&htim17);  	// start the timer interrupt
		while(j<PRINT_COUNTER)            	// used to print Awesome exactly five times
		{
			if(delayFlag == 1)       	// delay measurement  complete
			{		
				printf("Awesome!\n");
				delayFlag = 0;   	// delayFlag to be initialized for the next round of delay measurement
				j++;
			}
		
		}
	}
	else
	{
		printf("Timer 17 is not in delay mode\n");
		return;
	}
	HAL_TIM_Base_Stop_IT(&htim17);
	return;

}

ADD_CMD("timerdelay", timerdelay,"         Choose delay to print Awesome")

/*
 * FUNCTION	: freqCounter
 *
 * DESCRIPTION 	: This function measure the square wave frequency
 *
 * PARAMETERS	: void
 *
 * RETURNS	: int mode

 */

void freqCounter(int mode)
{
	static uint16_t measuredFreq=0;   // used to store the final measured frequency
	static uint16_t tempFreq=0;	  // used to store the new frequency measured which is 15hz higher/ lower than the previous freq
	if(strcmp(selectedMode,"fc")==0)
	{
		
		HAL_TIM_IC_Start_IT(&htim17,TIM_CHANNEL_1);  //start timer 17 in counter mode
		while(1)
		{
			measuredFreq = __HAL_TIM_GetCompare(&htim17,TIM_CHANNEL_1);  // get time measurment between to rising edge							
			measuredFreq = ONE_MHZ/measuredFreq;                         //  convert the clock cycles between two rising edge of 
            															 //  the input signal , into frequency based on timer 
                                                                         //  clock frequency                                                            
			
			if((measuredFreq-tempFreq)>MARGIN_ERROR || (tempFreq-measuredFreq)>MARGIN_ERROR)  // a change more than 15hz in the
													 										 // input frequency
			{
				tempFreq = measuredFreq;         				// the last measure frequency is 15hz higher/ lower than the previous freq, 
                                                                 // so stored it in temFreq 	
				if(freqFlag ==1)								 // frequency measurement is compleate
				{	
					printf("Frequency is : %uHz\n",(unsigned int)measuredFreq);     // print the new freaquency
					freqFlag=0;              										
				}
			}
		}
	}
	else
	{
		printf("Timer 17 is not in frequency counter mode\n");
		return;
	}			
}
ADD_CMD("freqcounter", freqCounter," Measure square wave frequency")
