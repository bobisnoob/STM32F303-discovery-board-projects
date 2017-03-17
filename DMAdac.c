/**
* File Name		 : DMAdac.c
* PROJECT		 : CNTR8000 Assignment #4 
* PROGRAMMERS	 : Abdel Alkuor, Nimal Kirshna (Group #2)
* FIRST VERSION  : 2017-02-08
* Description    : This program has three main functions
			       which enable the user to generate wave
				   forms such as triangular, sawtooth,
				   sinusoidal wave forms. The user has to
				   use adcinit first in minicom terminal
				   to initialize the two potentiometers 
				   which let him control the frequency 
				   and the amplitude of the generated wave
				   form, then followed by dacinit to enable
				   the digital to analog converter for PA4,
                   finally dac must be used followed by the
				   wave type to be generated.
				   (Note: adc pins and dac pin use DMA to
				   either send or receive data)			
**/

#include "common.h"
#include "stm32f3xx_hal.h"
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include "wavesgeneratorvalues.h"
#include <string.h>	
#define MARGIN_ERROR	 5
#define FACTOR_8BIT		255	
#define WAVE_SIZE		721
static int adcInitFlag = 0;    // To make sure it doesn't re initialize
							  // adc1 again if adcinit is typed in 
							  // minicom terminal more than once
							  // and it used to make sure that adc1 is
							  // initialized before using adc command	 
static int dacInitFlag = 0;   //  and same for dac initialization
volatile uint8_t amplitudeFlag;
volatile uint8_t frequencyFlag; 
volatile uint8_t amplitudeFactor;
volatile uint8_t frequencyFactor;
uint8_t potentiometerValue[2];
 
ADC_HandleTypeDef hadc1;
DMA_HandleTypeDef hdma_adc1;
DAC_HandleTypeDef hdac;
DMA_HandleTypeDef hdma_dac_ch1;
TIM_MasterConfigTypeDef sMasterConfig;
TIM_HandleTypeDef htim6;


/* FUNCTION      : adcinit
 * DESCRIPTION   : This function initialize two
				   pins PA1 and PA2 and it reads
				   the changing values from the pins
				   concurrently using DMA   
 * PARAMETERS    : int mode
 * RETURNS       : NULL
*/

void adcinit(int mode)
{			
	if (adcInitFlag == 0)
	{	
		ADC_ChannelConfTypeDef sConfig;
		ADC_MultiModeTypeDef multimode;	
		__GPIOA_CLK_ENABLE();
		hadc1.Instance = ADC1;
		hadc1.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV4;
		hadc1.Init.Resolution = ADC_RESOLUTION8b;
		hadc1.Init.ScanConvMode = ADC_SCAN_ENABLE;
		hadc1.Init.ContinuousConvMode = ENABLE;
		hadc1.Init.DiscontinuousConvMode = DISABLE;
		hadc1.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
		hadc1.Init.ExternalTrigConv = ADC_SOFTWARE_START;
		hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
		hadc1.Init.NbrOfConversion = 2;
		hadc1.Init.DMAContinuousRequests = ENABLE;
		hadc1.Init.EOCSelection = EOC_SINGLE_CONV;
		hadc1.Init.LowPowerAutoWait = DISABLE;
		hadc1.Init.Overrun = OVR_DATA_OVERWRITTEN;
	
		if(HAL_ADC_Init(&hadc1) != HAL_OK)
		{
			printf("Error in ADC_Init\n");
			return;
		}	
		
		multimode.Mode = ADC_MODE_INDEPENDENT;
		multimode.DMAAccessMode = ADC_DMAACCESSMODE_8_6_BITS;
		multimode.TwoSamplingDelay = ADC_TWOSAMPLINGDELAY_8CYCLES;
	
		if(HAL_ADCEx_MultiModeConfigChannel(&hadc1,&multimode) != HAL_OK)
		{
			printf(" ADC Multi channels Error\n");
		}
		sConfig.Channel = ADC_CHANNEL_2;
		sConfig.Rank = 1;
		sConfig.SingleDiff = ADC_SINGLE_ENDED;
		sConfig.SamplingTime = ADC_SAMPLETIME_181CYCLES_5;
		sConfig.OffsetNumber = ADC_OFFSET_NONE;
		sConfig.Offset = 0;
		
		if(HAL_ADC_ConfigChannel(&hadc1,&sConfig) != HAL_OK)
		{
			printf("Error in config. channel 2\n");
		}

		sConfig.Channel = ADC_CHANNEL_3;
		sConfig.Rank = 2;
			
		if(HAL_ADC_ConfigChannel(&hadc1,&sConfig) != HAL_OK)
        {
		    printf("Error in config. channel 3\n");
	    }

		adcInitFlag = 1;
	}
	else
	{
		printf("ADC1 has been already initialized\n");
	}
}
ADD_CMD("adcinit",adcinit,"Initialize ADC")


void DMA1_Channel1_IRQHandler(void) // adc dma interrupt handler
{
	HAL_DMA_IRQHandler(&hdma_adc1);
	//the margin error is used to prevent changing within certain range
	// due to the converstion error of ADC,i.e it takes first reading of
	//potentiometer as for example 213, and for next converstion it would
	//be for example 214 without changing the position of the potentiometer
	//therefore using margin error to eliminate uncertain changes in the
	//adc values 
	if((potentiometerValue[0] - amplitudeFactor) > MARGIN_ERROR || (amplitudeFactor -potentiometerValue[0]) > MARGIN_ERROR)
	{
		amplitudeFactor = potentiometerValue[0];
		amplitudeFlag = 1;
		HAL_ADC_Stop_DMA(&hadc1);
	}		
	
	if((potentiometerValue[1] - frequencyFactor) > MARGIN_ERROR || (frequencyFactor - potentiometerValue[1]) > MARGIN_ERROR)
	{
		frequencyFactor = potentiometerValue[1];
		frequencyFlag = 1;
		HAL_ADC_Stop_DMA(&hadc1);
	}

	
}

void DMA1_Channel3_IRQHandler(void) // DMA DAC interrupt handler
{
	HAL_DMA_IRQHandler(&hdma_dac_ch1);
}
 
/* FUNCTION      : dacinit
 * DESCRIPTION   : This function initializes PA4
				   in DMA mode using TIM6 as trigger
				   to output the data by DMA buffer
				   data
 * PARAMETERS    : int mode
 * RETURNS       : NULL
*/


void dacinit(int mode)
{
	if (dacInitFlag == 0)
	{	
	DAC_ChannelConfTypeDef sConfig;
	hdac.Instance = DAC;
	
	__GPIOA_CLK_ENABLE();	
	

	if(HAL_DAC_Init(&hdac) != HAL_OK)
	{
		printf("Error in DAC initialization\n");
	}

	sConfig.DAC_Trigger = DAC_TRIGGER_T6_TRGO;
	sConfig.DAC_OutputBuffer = DAC_OUTPUTBUFFER_ENABLE;
	
	if(HAL_DAC_ConfigChannel(&hdac, &sConfig, DAC_CHANNEL_1) != HAL_OK)
	{
		printf("Error in DAC Config.\n");
	}
		dacInitFlag = 1;
	}
	else
	{
		printf("DAC is already initialized\n");
	}	
	
}
ADD_CMD("dacinit",dacinit,"Initialize DAC")


void TIM6_Init(void)// this function is called in main.c to initialize
{					// enable TIM6 clock
	htim6.Instance = TIM6;
	htim6.Init.Prescaler = 0;
	htim6.Init.CounterMode = TIM_COUNTERMODE_UP;
	htim6.Init.Period = 200;
	__TIM6_CLK_ENABLE();	
	if(HAL_TIM_Base_Init(&htim6) != HAL_OK)
	{
		printf("Error in TIM6 Init.\n");
	}
		
	sMasterConfig.MasterOutputTrigger = TIM_TRGO_UPDATE;
	sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
	
	if(HAL_TIMEx_MasterConfigSynchronization(&htim6,&sMasterConfig) != HAL_OK)
	{
		printf("Error in TIM6 Config.\n");
	}
	
}
	
void DMA_Init(void) // this function is called in main.c to initialize
{                   // dma interrupts
	__GPIOF_CLK_ENABLE();
	__DMA1_CLK_ENABLE();
	HAL_NVIC_SetPriority(DMA1_Channel1_IRQn, 0, 0);
	HAL_NVIC_EnableIRQ(DMA1_Channel1_IRQn);
	HAL_NVIC_SetPriority(DMA1_Channel3_IRQn, 0, 0);
	HAL_NVIC_EnableIRQ(DMA1_Channel3_IRQn);
}

/* FUNCTION      : dac
 * DESCRIPTION   : This function takes input from the user
				   to generate either sinusoidal, sawtooth,
				   and triangular waves, this function also
					let the user use two potentiometers to
					control the amplitude and the frequency 
					of the generated wave form 
 * PARAMETERS    : int mode
 * RETURNS       : NULL
*/	
void dac(int mode)
{
	uint16_t manpulatedWave[WAVE_SIZE];
	uint16_t originalWave[WAVE_SIZE];
	char *waveName;
	uint16_t j;
	int rc;
	rc = fetch_string_arg(&waveName);
	if(rc)
	{
		printf("Enter wave type (sin,tri,saw)\n");
		return;
	}
	
	if(strcmp(waveName,"sin")==0)// sinusoidal wave
	{	
		for(j=0;j<WAVE_SIZE;j++)
		{
			originalWave[j]=sinWaveValues[j];
		}		
	}
	else if(strcmp(waveName,"tri")==0)// triangular wave
	{	
		for(j=0;j<WAVE_SIZE;j++)
        {
             originalWave[j]=triWaveValues[j];
        }
	}
	else if(strcmp(waveName,"saw")==0)// sawtooth wave
	{	
		for(j=0;j<WAVE_SIZE;j++)
        {
            originalWave[j]=sawWaveValues[j];
        }
	}
	else
	{
		printf("Unrecognized wave type\n");
		return;
	}
	HAL_ADC_Start_DMA(&hadc1, (uint32_t *)potentiometerValue, 2);
    HAL_TIM_Base_Start(&htim6);
    HAL_DAC_Start_DMA(&hdac, DAC_CHANNEL_1, (uint32_t *) originalWave, WAVE_SIZE, DAC_ALIGN_12B_R);
	
	while(1)
	{	
		if(amplitudeFlag==1)
		{
			HAL_DAC_Stop_DMA(&hdac,DAC_CHANNEL_1);		
			for(uint16_t i=0; i<WAVE_SIZE;i++)
			{
				manpulatedWave[i]=(potentiometerValue[0]*originalWave[i])/FACTOR_8BIT;// find new amplitued values
			}
			amplitudeFlag=0;
			HAL_DAC_Start_DMA(&hdac,DAC_CHANNEL_1,(uint32_t*) manpulatedWave,WAVE_SIZE,DAC_ALIGN_12B_R);
			HAL_ADC_Start_DMA(&hadc1,(uint32_t*) potentiometerValue,2);
			
		}
		if(frequencyFlag==1)
		{
			HAL_TIM_Base_Stop(&htim6);
			HAL_DAC_Stop_DMA(&hdac,DAC_CHANNEL_1);
			htim6.Init.Period = potentiometerValue[1]; // new frequency value
			if(HAL_TIM_Base_Init(&htim6) != HAL_OK)
			{
				printf("Error in TIM6 Init\n");
			}
			
			frequencyFlag = 0;
			HAL_TIM_Base_Start(&htim6);
			HAL_DAC_Start_DMA(&hdac,DAC_CHANNEL_1,(uint32_t*) manpulatedWave,WAVE_SIZE,DAC_ALIGN_12B_R);
			HAL_ADC_Start_DMA(&hadc1,(uint32_t*) potentiometerValue,2);
		}
	}

}

ADD_CMD("dac",dac,"<GeneratedWave> Select wave to generate")



void HAL_DAC_MspInit(DAC_HandleTypeDef* hdac)
{	
	GPIO_InitTypeDef GPIO_InitStruct;
	__DAC1_CLK_ENABLE();
	GPIO_InitStruct.Pin = GPIO_PIN_4;
	GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
	
	hdma_dac_ch1.Instance = DMA1_Channel3;
	hdma_dac_ch1.Init.Direction = DMA_MEMORY_TO_PERIPH;
	hdma_dac_ch1.Init.PeriphInc = DMA_PINC_DISABLE;
	hdma_dac_ch1.Init.MemInc = DMA_MINC_ENABLE;
	hdma_dac_ch1.Init.PeriphDataAlignment = DMA_PDATAALIGN_HALFWORD;
	hdma_dac_ch1.Init.MemDataAlignment = DMA_MDATAALIGN_HALFWORD;
	hdma_dac_ch1.Init.Mode = DMA_CIRCULAR;
	hdma_dac_ch1.Init.Priority = DMA_PRIORITY_LOW;
	if(HAL_DMA_Init(&hdma_dac_ch1) != HAL_OK)
	{
		printf("Error in DAC DMA init.\n");
	}

	__HAL_REMAPDMA_CHANNEL_ENABLE(HAL_REMAPDMA_TIM6_DAC1_CH1_DMA1_CH3);
	__HAL_LINKDMA(hdac,DMA_Handle1,hdma_dac_ch1);
}

void HAL_ADC_MspInit(ADC_HandleTypeDef* hadc)
{	

	GPIO_InitTypeDef GPIO_InitStruct;
	__ADC12_CLK_ENABLE();
	GPIO_InitStruct.Pin = GPIO_PIN_1 | GPIO_PIN_2;
	GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
	GPIO_InitStruct.Pull  = GPIO_NOPULL;
	HAL_GPIO_Init(GPIOA,&GPIO_InitStruct);
	hdma_adc1.Instance = DMA1_Channel1;
	hdma_adc1.Init.Direction = DMA_PERIPH_TO_MEMORY;
	hdma_adc1.Init.PeriphInc = DMA_PINC_DISABLE;
	hdma_adc1.Init.MemInc = DMA_MINC_ENABLE;
	hdma_adc1.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
	hdma_adc1.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
	hdma_adc1.Init.Mode = DMA_CIRCULAR;
	hdma_adc1.Init.Priority = DMA_PRIORITY_LOW;
		
	if(HAL_DMA_Init(&hdma_adc1) != HAL_OK)
	{
		printf("Error in ADC DMA initialization\n");
	}
	__HAL_LINKDMA(&hadc1,DMA_Handle,hdma_adc1);
	
}























































































































  








