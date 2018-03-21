#include "SST25V_flash.h"
#include "gpio.h"
#include "spi.h"

#define FLASH_READ_SLOW				0x03
#define FLASH_READ_FAST				0x0B
#define FLASH_WRITE_ENABLE			0x06
#define FLASH_WRITE_DISABLE			0x04
#define FLASH_ERASE_4K				0x20
#define FLASH_AAI					0xAD
#define FLASH_BYTE_PROGRAM			0x02
#define FLASH_ENABLE_SO				0x70
#define FLASH_DISABLE_SO			0x80
#define FLASH_ENABLE_WRITE_STATUS	0x50
#define FLASH_WRITE_STATUS			0x01
#define FLASH_READ_STATUS			0x05

#define FLASH_SBIT_BUSY				0x01
#define FLASH_SBIT_WRITE_ENABLE		0x01

#define DWT_CYCCNT ((volatile uint32_t *)0xE0001004)
#define CPU_CYCLES *DWT_CYCCNT

static void SST25_start();
static void SST25_end();
static uint8_t SST25_get_status();
static void SST25_enable_hardware_EOW();
static void SST25_write_enable();
static void SST25_write_status(uint8_t status);
static void delay();

void SST25_sector_erase_4K(uint32_t address)
{
	address &= 0xFFFFFF;

	SST25_write_status(0x00); // Clear all sector protection
	SST25_write_enable();

	SST25_start();
	uint8_t cmd[] = {
			FLASH_ERASE_4K,
			(address >> 14) & 0xFF,
			(address >> 8) & 0xFF,
			address & 0xFF
	};
	flash_SPI_write(cmd, 4);
	SST25_end();

	while (SST25_get_status() & FLASH_SBIT_BUSY);
}

void SST25_enable_hardware_EOW()
{
	SST25_start();
	flash_SPI_read_write_byte(FLASH_ENABLE_SO);
	SST25_end();
}

void SST25_write_enable()
{
	SST25_start();
	flash_SPI_read_write_byte(FLASH_WRITE_ENABLE);
	SST25_end();
}

void SST25_write_status(uint8_t status)
{
	SST25_start();
	flash_SPI_read_write_byte(FLASH_ENABLE_WRITE_STATUS);
	SST25_end();

	SST25_start();
	uint8_t cmd[] = {FLASH_WRITE_STATUS, status};
	flash_SPI_write(cmd, 2);
	SST25_end();
}

uint8_t SST25_get_status()
{
	HAL_StatusTypeDef hal_result;

	SST25_start();
	uint8_t cmd[] = {FLASH_READ_STATUS, 0x00};
	uint8_t received[2];

	if ((hal_result = HAL_SPI_TransmitReceive(&hspi5, cmd, received, 2, 1000)) != HAL_OK)
		Error_Handler();

	SST25_end();
	return received[1];
}

void SST25_write_byte(uint32_t address, uint8_t data)
{
	if (!(SST25_get_status() & FLASH_SBIT_WRITE_ENABLE))
		SST25_write_enable();

	address &= 0xFFFFFF;

	SST25_start();
	uint8_t cmd[] = {
			FLASH_BYTE_PROGRAM,
			(address >> 14) & 0xFF,
			(address >> 8) & 0xFF,
			address & 0xFF,
			data
	};
	flash_SPI_write(cmd, 5);
	SST25_end();
}

void SST25_write(uint32_t address, uint8_t *data, uint16_t size)
{
	if (!(SST25_get_status() & FLASH_SBIT_WRITE_ENABLE))
		SST25_write_enable();

	SST25_enable_hardware_EOW();

	address &= 0xFFFFFF;
	uint8_t address_cmd[] = {
			FLASH_AAI,
			(address >> 14) & 0xFF,
			(address >> 8) & 0xFF,
			address & 0xFF,
	};
	uint8_t data_cmd[3] = {FLASH_AAI, 0x00, 0x00};
	uint16_t written = 0;
	uint16_t remaining;

	while ((remaining = size - written) > 1)
	{
		SST25_start();
		if (!written)
			flash_SPI_write(address_cmd, 4);

		data_cmd[1] = data[written++];
		data_cmd[2] = data[written++];

		if (written <= 2)
			flash_SPI_write(data_cmd + 1, 2);
		else
			flash_SPI_write(data_cmd, 3);
		SST25_end();

		SST25_start();
		while (!HAL_GPIO_ReadPin(FLASH_MISO_GPIO_Port, FLASH_MISO_Pin));
		SST25_end();
	}
	SST25_start();
	flash_SPI_read_write_byte(FLASH_WRITE_DISABLE);
	SST25_end();
	SST25_start();
	flash_SPI_read_write_byte(FLASH_DISABLE_SO);
	SST25_end();

	if (remaining)
		SST25_write_byte(address + written, data[written]);
}

void SST25_read(uint32_t address, uint8_t *read_buffer, uint16_t size)
{
	address &= 0xFFFFFF;

	SST25_start();
	uint8_t cmd[] = {
			FLASH_READ_FAST,
			(address >> 14) & 0xFF,
			(address >> 8) & 0xFF,
			address & 0xFF,
			0xFF // Dummy byte
	};
	flash_SPI_write(cmd, 5);
	flash_SPI_read(read_buffer, size);
	SST25_end();
}

inline void SST25_start()
{
	FLASH_SS_GPIO_Port->BSRR = (uint32_t) FLASH_SS_Pin << 16;
	delay();
}

inline void SST25_end()
{
	FLASH_SS_GPIO_Port->BSRR = FLASH_SS_Pin;
	delay();
}

inline void delay() { for (uint32_t c = CPU_CYCLES; CPU_CYCLES < c + 10;); }
