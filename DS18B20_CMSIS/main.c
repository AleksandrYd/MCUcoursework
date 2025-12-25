#include "stm32f10x.h"
#include "ds18b20.h"

#define MAX_SENSORS 2

#define SENSOR_OK          0x00  // sensor connected and answer is correct
#define SENSOR_NO_RESPONSE 0x01  // all sensors disconnected
#define SENSOR_CRC_ERROR   0x02  // crc wrong

static uint8_t command_buffer[10];  // 1 comand byte, 1 bit depth byte, 8 address byte
static uint8_t byte_counter = 0;
static uint8_t header_buffer[2];
uint8_t header_found = 0;
uint8_t is_command_received = 0;

uint8_t new_resolution;
uint8_t* device_address;
volatile Sensor sensors[MAX_SENSORS];	

void ChangeResolution(uint8_t new_resolution, uint8_t* device_address) 
{
    uint8_t config_value;
    switch(new_resolution) 
		{
        case 9:  config_value = RESOLUTION_9BIT; break;  // 9 bit  = 00
        case 10: config_value = RESOLUTION_10BIT; break;  // 10 bit = 01
        case 11: config_value = RESOLUTION_11BIT; break;  // 11 bit = 10
        case 12: config_value = RESOLUTION_12BIT; break;  // 12 bit = 11
        default: return;
    }
    
		ds18b20_MatchRom(device_address);
		ds18b20_WriteByte(WRITE_SCRATCHPAD);
    ds18b20_WriteScratchpad(config_value);  // Write Scratchpad
    
		ds18b20_MatchRom(device_address);
    ds18b20_WriteByte(0x48);  // Copy Scratchpad
    DelayMicro(10000);
}


void Init_Sensors (void) {													// reset all values in sensors
		for (uint8_t i = 0; i < MAX_SENSORS; i++)
		{
				sensors[i].raw_temp = 0x0;
				sensors[i].temp = 0.0;
				sensors[i].crc8_rom = 0x0;
				sensors[i].crc8_data = 0x0;
				sensors[i].crc8_rom_error = 0x0;
				sensors[i].crc8_data_error = 0x0;
			
				for (uint8_t j = 0; j < 9; j++)
				{
						sensors[i].scratchpad_data[j] = 0x00;
				}
		}
		sensors[0].ROM_code[0] = 0x28;
		sensors[0].ROM_code[1] = 0x61;
		sensors[0].ROM_code[2] = 0x64;
		sensors[0].ROM_code[3] = 0x0b;
		sensors[0].ROM_code[4] = 0x4b;
		sensors[0].ROM_code[5] = 0x5a;
		sensors[0].ROM_code[6] = 0xac;
		sensors[0].ROM_code[7] = 0x1c;
	
		sensors[1].ROM_code[0] = 0x28;
		sensors[1].ROM_code[1] = 0x61;
		sensors[1].ROM_code[2] = 0x64;
		sensors[1].ROM_code[3] = 0x0b;
		sensors[1].ROM_code[4] = 0x49;
		sensors[1].ROM_code[5] = 0xb8;
		sensors[1].ROM_code[6] = 0x80;
		sensors[1].ROM_code[7] = 0x37;
}

void GPIO_Init (void) {

  RCC->APB2ENR |= (1UL << 2);                /* Enable GPIOA clock            */

  /* Configure LED (PA.5) pins as output */
  GPIOA->CRL   &= ~((15ul << 4*5));
  GPIOA->CRL   |=  (( 1ul << 4*5));
}

void USART1_Init(void) 
	{
    // 1. Clocking
    RCC->APB2ENR |= RCC_APB2ENR_USART1EN;  // USART1 APB2
    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN;    // GPIOA
    
    // 2. PA9 (TX) & PA10 (RX)
    // PA9: Alternate Function Output Push-Pull, max speed 50MHz
    GPIOA->CRH &= ~(GPIO_CRH_MODE9 | GPIO_CRH_CNF9);
    GPIOA->CRH |= GPIO_CRH_MODE9_1 | GPIO_CRH_MODE9_0; // MODE9 = 11 (50 MHz)
    GPIOA->CRH |= GPIO_CRH_CNF9_1;                     // CNF9 = 10 (AF Push-Pull)
    
    // PA10: Input Floating
    GPIOA->CRH &= ~(GPIO_CRH_MODE10 | GPIO_CRH_CNF10);
    GPIOA->CRH |= GPIO_CRH_CNF10_0; // CNF10 = 01 (Input Floating)
    
    // 3. Speed (SystemCoreClock = 72MHz)
    // USARTDIV = 72000000 / (16 * 9600) = 468.75
    // BRR = 468.75 * 16 = 7500
    USART1->BRR = 7500;
    
    // 4. turn on USART
    USART1->CR1 |= USART_CR1_TE | USART_CR1_RE | USART_CR1_UE;
    
    // USART1->CR1 |= USART_CR1_RXNEIE;
    // NVIC_EnableIRQ(USART1_IRQn);
}

void USART_Init (void) 
	{
	
	RCC->APB1ENR |= RCC_APB1ENR_USART2EN; 
	RCC->APB2ENR |= RCC_APB2ENR_IOPAEN; 
	
	// CNF2 = 10, MODE2 = 01
	GPIOA->CRL &= (~GPIO_CRL_CNF2_0); 
	GPIOA->CRL |= (GPIO_CRL_CNF2_1 | GPIO_CRL_MODE2);

	// CNF3 = 10, MODE3 = 00, ODR3 = 1
	GPIOA->CRL &= (~GPIO_CRL_CNF3_0);
	GPIOA->CRL |= GPIO_CRL_CNF3_1;
	GPIOA->CRL &= (~(GPIO_CRL_MODE3));
	GPIOA->BSRR |= GPIO_ODR_ODR3;

	//USARTDIV = SystemCoreClock / (16 * BAUD) = 72000000 / (16 * 9600) = 468,75

	USART2->BRR = 7500; 

	USART2->CR1 |= USART_CR1_TE | USART_CR1_RE | USART_CR1_UE | USART_CR1_RXNEIE;
	USART2->CR2 = 0;
	USART2->CR3 = 0;
	
	NVIC_EnableIRQ(USART2_IRQn);
  NVIC_SetPriority(USART2_IRQn, 0);

}
	

void USART1_SendByte(uint8_t byte) {
    while (!(USART1->SR & USART_SR_TXE));
    USART1->DR = byte;
}

void USART_SendByte(uint8_t byte) {
    while ((USART2->SR & USART_SR_TXE) == 0) {}
    USART2->DR = byte;
}

void USART2_IRQHandler(void) 
{   
	if (USART2->SR & USART_SR_RXNE) 
	{
		uint8_t received_byte = (uint8_t)(USART2->DR & 0xFF);
		if (header_found == 0)
		{
			header_buffer[byte_counter++] = received_byte;
			if (byte_counter == 2) 
			{
				if (header_buffer[0] == 0xAA && header_buffer[1] == 0x55) 
				{
					header_found = 1;
					byte_counter = 0; 
				} 
				else 
				{
					header_buffer[0] = header_buffer[1];
					byte_counter = 1;
				}
			}
		}			
		else
		{
			if (byte_counter < 10) 
			{
				command_buffer[byte_counter++] = received_byte;
				if (byte_counter == 10) 
					{
						if (command_buffer[0] == 0x01) 
						{
							new_resolution = command_buffer[1];
							device_address = &command_buffer[2];
							
							is_command_received = 1;
						}
						header_found = 0;
						byte_counter = 0;
					}
			}
		}

	}
}



void SendSensorData(uint8_t sensor_index) 
	{
		uint8_t header1 = 0xAA;
		uint8_t header2 = 0x55;
		USART_SendByte(header1);
		USART_SendByte(header2);
		
    for (int i = 0; i < 8; i++) {
        USART_SendByte(sensors[sensor_index].ROM_code[i]);
    }
		
		USART_SendByte(sensors[sensor_index].status);
    
    float temperature;
    if (!sensors[sensor_index].crc8_data_error) {
        temperature = sensors[sensor_index].temp;
    } else {
        temperature = -999.0f;
    }
    
    uint8_t *temp_bytes = (uint8_t*)&temperature;
    for (int i = 0; i < 4; i++) {
        USART_SendByte(temp_bytes[i]);
    }
    
    uint8_t config = (sensors[sensor_index].scratchpad_data[4] >> 5) & 0x03;
    uint8_t resolution;
    switch(config) {
        case 0x00: resolution = 9; break;  // 00 = 9 bit
        case 0x01: resolution = 10; break; // 01 = 10 bit
        case 0x02: resolution = 11; break; // 10 = 11 bit
        case 0x03: resolution = 12; break; // 11 = 12 bit
        default: resolution = 12;
    }
    
    USART_SendByte(resolution);
		
		
		USART1_SendByte(header1);
		USART1_SendByte(header2);
		for (int i = 0; i < 8; i++) {
        USART1_SendByte(sensors[sensor_index].ROM_code[i]);
    }
		USART1_SendByte(sensors[sensor_index].status);
    for (int i = 0; i < 4; i++) {
        USART1_SendByte(temp_bytes[i]);
    }
    USART1_SendByte(resolution);
}

void SystemCoreClockConfigure(void) 
{
		RCC->CR |= ((uint32_t)RCC_CR_HSEBYP);                    
		while ((RCC->CR & RCC_CR_HSERDY) == 0);                  

		RCC->CFGR = RCC_CFGR_SW_HSE;                             
		while ((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_HSE);  
	
	  RCC->CFGR |= RCC_CFGR_HPRE_DIV1;                          // HCLK = SYSCLK
		RCC->CFGR |= RCC_CFGR_PPRE1_DIV1;                        // APB1 = HCLK/2
		RCC->CFGR |= RCC_CFGR_PPRE2_DIV1;                        // APB2 = HCLK/2

		RCC->CR &= ~RCC_CR_PLLON;

		//  PLL configuration:  = HSE * 9 = 72 MHz
		RCC->CFGR &= ~(RCC_CFGR_PLLSRC | RCC_CFGR_PLLMULL);
		RCC->CFGR |=  (RCC_CFGR_PLLSRC_HSE | RCC_CFGR_PLLMULL9);

		RCC->CR |= RCC_CR_PLLON;                                
		while((RCC->CR & RCC_CR_PLLRDY) == 0) __NOP();           

		RCC->CFGR &= ~RCC_CFGR_SW;                               
		RCC->CFGR |=  RCC_CFGR_SW_PLL;
		while ((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_PLL);  
}

int main(void)
{					
		uint16_t devCount, i = 0;

		SystemCoreClockConfigure();
		SystemCoreClockUpdate();
		SysTick_Config(SystemCoreClock / 1000000);                  // SysTick 1 msec interrupts

		//GPIO_Init();
		USART_Init();
		USART1_Init();

		Init_Sensors();

		ds18b20_PortInit();	

		//	Init PortB.11 for data transfering 
		while (ds18b20_Reset())
		{
			sensors[0].status = SENSOR_NO_RESPONSE;
			sensors[1].status = SENSOR_NO_RESPONSE;
			SendSensorData(0);
			SendSensorData(1);
		}

		sensors[0].status = SENSOR_OK;
		sensors[1].status = SENSOR_OK;

		devCount = Search_ROM(0xF0,&sensors);	 																																										// 	Init Search ROM routine
																																																													
		while (1) {
				for (i = 0; i < devCount; i++) {
						ds18b20_ConvertTemp(1, sensors[i].ROM_code);																																// 	Measure Temperature (Send Convert Temperature Command)

						DelayMicro(750000); 																																														// 	Delay for conversion (max 750 ms for 12 bit)
						ds18b20_ReadStratchpad(1, sensors[i].scratchpad_data, sensors[i].ROM_code); 																		// 	Read measured temperature
						sensors[i].crc8_data = Compute_CRC8(sensors[i].scratchpad_data,8);																							// 	Compute CRC for data
						sensors[i].crc8_data_error = Compute_CRC8(sensors[i].scratchpad_data,9) == 0 ? 0 : 1;														// 	Get CRC Error Signal
						if (!sensors[i].crc8_data_error) {																																							// 	Check if data correct
								sensors[i].raw_temp = ((uint16_t)sensors[i].scratchpad_data[1] << 8) | sensors[i].scratchpad_data[0]; 			// 	Get 16 bit temperature with sign
								sensors[i].temp = sensors[i].raw_temp * 0.0625;																															// 	Convert to float
								sensors[i].status = SENSOR_OK;
						}																				
						else
						{
							sensors[i].status = SENSOR_CRC_ERROR;
						}
						SendSensorData(i);
						DelayMicro(100000);
						
						if(is_command_received)
						{
							ChangeResolution(new_resolution, device_address);
							is_command_received = 0;
						}
				}
		}
}