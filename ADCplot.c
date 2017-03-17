/**
* File Name		 : ADCplot.c
* PROJECT		 : CNTR8000 Assignment #3
* PROGRAMMERS	 : Abdel Alkuor
* FIRST VERSION  : 2017-02-01
* Description    : This program is used to initialize ADC1
				   for PA1,PA2 and PA3. Two main fucntions
				   can be run on the minicom terminal:
				   1) adcinit initializes all ADC1 
					  parameters except for selecting the
					  ADC channel
				   2) adc <channel> which the user can 
					  select which channel to activate 
					  ADC and after the samples are taken
					  for the selected channel, a graph 
					  function is used to plot the values
					  on y and x axis with 1 decimal 
					  accuracy.	 				
**/

#include "common.h"
#include "stm32f3xx_hal.h"
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>

#define ADC_12_BIT		 4034  	
#define VOLTAGE_FACTOR	 29										
#define SAMPLING_TIME	 3		// in seconds
#define SAMPLE_SIZE		 21
#define SAMPLING_DELAY	 3000
#define YAXIS_SIZE		 30


static int initFlag = 0;    // To make sure it doesn't re initialize
							// adc1 again if adcinit is typed in 
							// minicom terminal more than once
							// and it used to make sure that adc1 is
							// initialized before using adc command	 
static uint32_t channel[3] = {ADC_CHANNEL_2,ADC_CHANNEL_3,ADC_CHANNEL_4};
ADC_HandleTypeDef hdac1;
ADC_ChannelConfTypeDef sConfig;

void graph(uint32_t sampledADCValues[SAMPLE_SIZE]);
uint32_t getAnalogValue(uint32_t ADCValue); 

/* FUNCTION      : adcinit
 * DESCRIPTION   : This function initializes ADC1 except for
				   ADC channel. The fucntion sets the ADC 
				   resolution to 12bits and it updates the 
				   reading of ADC value every 3 seconds using
				   TIMER 1 interrupt	   
 * PARAMETERS    : int mode
 * RETURNS       : NULL
*/

void adcinit(int mode)
{			
	if (initFlag == 0)
	{
		ADC_MultiModeTypeDef multimode;	
		GPIO_InitTypeDef GPIO_InitStruct;
		__ADC12_CLK_ENABLE();	
		__GPIOA_CLK_ENABLE();	
		GPIO_InitStruct.Pin = (GPIO_PIN_1 | GPIO_PIN_2| GPIO_PIN_3);
		GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
		GPIO_InitStruct.Pull  = GPIO_NOPULL;
		HAL_GPIO_Init(GPIOA,&GPIO_InitStruct);
	
		hdac1.Instance= ADC1;
		hdac1.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV1;
		hdac1.Init.Resolution = ADC_RESOLUTION12b;
		hdac1.Init.ScanConvMode = ADC_SCAN_DISABLE;
		hdac1.Init.ContinuousConvMode = DISABLE;
		hdac1.Init.DiscontinuousConvMode = DISABLE;
		hdac1.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_RISING;
		hdac1.Init.ExternalTrigConv = ADC_EXTERNALTRIGCONV_T1_TRGO;
		hdac1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
		hdac1.Init.NbrOfConversion = 1;
		hdac1.Init.DMAContinuousRequests = DISABLE;
		hdac1.Init.EOCSelection = EOC_SINGLE_CONV;
		hdac1.Init.LowPowerAutoWait = DISABLE;
		hdac1.Init.Overrun = OVR_DATA_PRESERVED;
	
		if(HAL_ADC_Init(&hdac1) != HAL_OK)
		{
			printf("Error in ADC_Init\n");
			return;
		}
	
		multimode.DMAAccessMode = ADC_DMAACCESSMODE_DISABLED;
		multimode.TwoSamplingDelay = ADC_TWOSAMPLINGDELAY_1CYCLE;
	
		if(HAL_ADCEx_MultiModeConfigChannel(&hdac1,&multimode) != HAL_OK)
		{
			printf(" ADC Multi channels Error");
		}
	
		sConfig.Rank = 1;
		sConfig.SingleDiff = ADC_SINGLE_ENDED;
		sConfig.SamplingTime = ADC_SAMPLETIME_61CYCLES_5;
		sConfig.OffsetNumber = ADC_OFFSET_NONE;
		sConfig.Offset = 0;
		initFlag = 1;
	}
	else
	{
		printf("ADC1 has been already initialized\n");
	}
}
ADD_CMD("adcinit",adcinit,"Initialize ADC")

/* FUNCTION      : adc 
 * DESCRIPTION   : This function lets the user choose between
				   the channels 1,2 or 3 from ADC1.To use this
				   function adcinit function must be used before
				   seleting any channel. The function collects
				   20 ADC samples value from the selected channgel
     			   and it uses graph fucntion to plot it on 
				   xy axis with time interval of 3 seconds between
				   each adc sample is taken 				    
 * PARAMETERS    : int mode
 * RETURNS       : NULL
*/

void adc(int mode)
{	
	if(initFlag!=1)
	{
		printf("Initialize ADC1 first by using adcinit command\n");
		return;
	}
	uint32_t selectedChannel;
	uint32_t ADCvalue = 0;
	uint8_t  sampleCounter = 0;
	uint8_t  sampleEndFlag = 1;
	uint32_t sampledADCValues[SAMPLE_SIZE] = {0};
	uint32_t analogValue = 0;
	int rc;
	int sampleNumber = 1;
	rc = fetch_uint32_arg(&selectedChannel);
	if(rc)
	{
		printf("Please enter channel number(1,2,3)\n");
		return;
	}
	
	if(selectedChannel < 1 || selectedChannel > 3)
	{
		printf("Channel number must be 1,2, or 3\n");
		return;
	}

	HAL_ADC_Stop(&hdac1);// stop ADC before initializing the next selected channel
	sConfig.Channel =channel[selectedChannel-1];
     
    if(HAL_ADC_ConfigChannel(&hdac1, &sConfig) != HAL_OK)
    {
        printf("Error in Channel Config\n");
    }
     
    HAL_ADC_Start(&hdac1);
    printf("Time(s) Voltage(v/10)\n");

	while(sampleEndFlag != 0)
	{
		if(sampleCounter < SAMPLE_SIZE)
		{	
			HAL_Delay(SAMPLING_DELAY);// delay between each sample's taken
	        ADCvalue = HAL_ADC_GetValue(&hdac1);
			analogValue = getAnalogValue(ADCvalue);			
	        printf("%i",sampleNumber*SAMPLING_TIME);// print out the current sampling time
	        printf("           %u\n",(unsigned int)analogValue);
	        sampleNumber++;
			sampledADCValues[sampleCounter] = analogValue;
			sampleCounter++;
		}
		else
		{
			graph(sampledADCValues);
			sampleEndFlag = 0;
		}
	}

}
ADD_CMD("adc",adc,"<channel>    choose 1,2,or 3")

/* FUNCTION      : graph
 * DESCRIPTION   : This function plots the collected data
				   from ADC1's selected channel in x-y axises
				   where x the time axis with spaced interval
				   of 3 seconds and y axis the value of the
				   analog voltage with an accuracy of 
				   0.1V    
 * PARAMETERS    : uint32_t sampledADCValue[]
 * RETURNS       : NULL
*/

void graph(uint32_t sampledADCValue[SAMPLE_SIZE])
{
    char graphPoint='.';
    char matchedValue= 'o';
	char yAxis[YAXIS_SIZE][4] = {"0.0","0.1","0.2","0.3","0.4","0.5","0.6","0.7","0.8","0.9",
								 "1.0","1.1","1.2","1.3","1.4","1.5","1.6","1.7","1.8","1.9",
								 "2.0","2.1","2.2","2.3","2.4","2.5","2.6","2.7","2.8","2.9"};	     		 		 	
    int i,j;
    uint8_t yIndex = 29;
    uint8_t xAxisLine = 20;
	
    for(j=(YAXIS_SIZE-1); j>=0; j--)
	{ 
		printf("%s+ ",yAxis[yIndex]);// print out y axis values

		for(i=0; i<SAMPLE_SIZE;i++)
	    {
			if(sampledADCValue[i]==j)// compare y value with x value(from sampled array)
	        {
				printf("%c  ",matchedValue);
            }
			else
            {   
			    printf("%c  ",graphPoint);
                    
            }
		}

        printf("\n");
        yIndex-=1;
    }

    printf("++++");

    for(i=0;i<xAxisLine;i++)// draw x axis line
    {
        printf("+++");
    }
	
    printf("\n   + ");
    
    for(i=1;i<SAMPLE_SIZE;i++)// print out x axis values
    {   
        if(i<4) // making the number matches with above drawn '.' line
		{
			printf("%d  ",i*SAMPLING_TIME);
        }
        else
        {
			printf("%d ",i*SAMPLING_TIME);
        }
    }  
    
    printf("\n");


}

/* FUNCTION      : getAnalogValue
 * DESCRIPTION   : This function converts the digital value of
					ADC1's selected channel to analog value 
					with maximum analog value of 2.9V   
 * PARAMETERS    : uint32_t ADCValue
 * RETURNS       : Analog value 
*/

uint32_t getAnalogValue(uint32_t ADCValue)
{
	return (ADCValue*VOLTAGE_FACTOR)/ADC_12_BIT;
}
