#include "SST25V_flash.h"
#include "gpio.h"
#include "stm32f7xx_hal_spi.h"

#define FLASH_READ_SLOW				0x03
#define FLASH_WRITE_ENABLE			0x06
#define FLASH_WRITE_DISABLE			0x04
#define FLASH_ERASE_4K				0x20
#define FLASH_AAI					0xAD
#define FLASH_BYTE_PROGRAM			0x02
#define FLASH_ENABLE_SO				0x70
#define FLASH_ENABLE_WRITE_STATUS	0x50
#define FLASH_WRITE_STATUS			0x01

#define FLASH_SBIT_BUSY				0x01
#define FLASH_SBIT_WRITE_ENABLE		0x01

void SST25_start();
void SST25_end();
uint8_t SST25_get_status();
void SST25_sector_erase_4K(uint32_t address);
void SST25_enable_hardware_EOW();
void SST25_write_enable();
void SST25_write_status(uint8_t status);

void SST25_sector_erase_4K(uint32_t address)
{
	uint8_t cmd;

	SST25_write_status(0x00); // Clear all sector protection
	SST25_write_enable();

	SST25_start();
	cmd = FLASH_ERASE_4K;
	flash_SPI_write(&cmd, 1);
	flash_SPI_write((uint8_t *) &address, 3);
	SST25_end();

	while (SST25_get_status() & FLASH_SBIT_BUSY)
		__NOP();
}

void SST25_enable_hardware_EOW()
{
	uint8_t cmd;
	SST25_start();
	cmd = FLASH_ENABLE_SO;
	flash_SPI_write(&cmd, 1);
	SST25_end();
}

void SST25_write_enable()
{
	uint8_t cmd;
	SST25_start();
	cmd = FLASH_WRITE_ENABLE;
	flash_SPI_write(&cmd, 1);
	SST25_end();
}

void SST25_write_status(uint8_t status)
{
	uint8_t cmd;
	SST25_start();
	cmd = FLASH_ENABLE_WRITE_STATUS;
	flash_SPI_write(&cmd, 1);
	SST25_end();

	SST25_start();
	cmd = FLASH_WRITE_STATUS;
	flash_SPI_write(&cmd, 1);
	flash_SPI_write(&status, 1);
	SST25_end();
}

uint8_t SST25_get_status()
{
	SST25_start();
	flash_SPI_read_write_byte(0x05);
	uint8_t b = flash_SPI_read_write_byte(0);
	SST25_end();
	return b;
}

void SST25_write_byte(uint32_t address, uint8_t data)
{
	if (!(SST25_get_status() & FLASH_SBIT_WRITE_ENABLE))
		SST25_write_enable();

	address &= 0xFFFFFF;
	uint8_t cmd;

	SST25_start();
	cmd = FLASH_BYTE_PROGRAM;
	flash_SPI_write(&cmd, 1);
	flash_SPI_write((uint8_t *) &address, 3);
	flash_SPI_write(&data, 1);
	SST25_end();
}

void SST25_write(uint32_t address, uint8_t data)
{
	if (!(SST25_get_status() & FLASH_SBIT_WRITE_ENABLE))
		SST25_write_enable();

	address &= 0xFFFFFF;
	uint8_t cmd;

	SST25_start();
	cmd = FLASH_BYTE_PROGRAM;
	flash_SPI_write(&cmd, 1);
	flash_SPI_write((uint8_t *) &address, 3);
	flash_SPI_write(&data, 1);
	SST25_end();

	/// !!!!
}

void SST25_read(uint32_t address, uint8_t *read_buffer, uint16_t size)
{
	address &= 0xFFFFFF;

	SST25_start();
	uint8_t cmd = FLASH_READ_SLOW;
	flash_SPI_write(&cmd, 1);
	flash_SPI_write((uint8_t *) &address, 3);
	flash_SPI_read(read_buffer, size);
	SST25_end();
}

void SST25_start()
{
	flash_CS_pin(0);
}

void SST25_end()
{
	flash_CS_pin(1);
}

void SPI_write()
{
	//if (__HAL_SPI_GET_FLAG(hspi, SPI_FLAG_TXE))
		//hspi->Instance->DR = *((uint16_t *)pData);
}
