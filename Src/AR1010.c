#include "AR1010.h"
#include "stm32f7xx_hal.h"
#include <stdint.h>

int addr = 0x10;

uint16_t initialRegisters[18] = {
	0xFFFB,		// R0:  1111 1111 1111 1011
	0x5B15,		// R1:  0101 1011 0001 0101 - Mono (D3), Softmute (D2), Hardmute (D1)  !! SOFT-MUTED BY DEFAULT !!
	0xD0B9,		// R2:  1101 0000 1011 1001 - Tune/Channel
	0xA410,		// R3:  1010 0100 0001 0000 - Seekup (D15), Seek bit (D14), Space 100kHz (D13), Seek threshold: 16 (D6-D0)
	0x0780,		// R4:  0000 0111 1000 0000
	0x28AB,		// R5:  0010 1000 1010 1011
	0x6400,		// R6:  0110 0100 0000 0000
	0x1EE7,		// R7:  0001 1110 1110 0111
	0x7141,		// R8:  0111 0001 0100 0001
	0x007D,		// R9:  0000 0000 0111 1101
	0x82C6,		// R10: 1000 0010 1100 0110 - Seek wrap (D3)
	0x4E55,		// R11: 0100 1110 0101 0101
	0x970C,		// R12: 1001 0111 0000 1100
	0xB845,		// R13: 1011 1000 0100 0101
	0xFC2D,		// R14: 1111 1100 0010 1101 - Volume control 2 (D12-D15)
	0x8097,		// R15: 1000 0000 1001 0111
	0x04A1,		// R16: 0000 0100 1010 0001
	0xDF61		// R17: 1101 1111 0110 0001
};

static uint8_t volume1_conv[19] = {0xF, 0xF, 0xF, 0xF, 0xB, 0xB, 0xB, 0xA, 0x9, 0x8, 0x7, 0x6, 0x6, 0x6, 0x3, 0x3, 0x2, 0x1, 0x0};
static uint8_t volume2_conv[19] = {0x0, 0xC, 0xD, 0xF, 0xC, 0xD, 0xF, 0xF, 0xF, 0xF, 0xF, 0xD, 0xE, 0xF, 0xE, 0xF, 0xF, 0xF, 0xF};

static uint8_t reg_write(uint8_t memAddr, uint16_t inputWord);
static uint16_t reg_read(uint8_t memAddr);
static uint8_t memXOR(uint8_t memAddr, uint16_t mask);
static uint8_t memAND(uint8_t memAddr, uint16_t mask);
static uint8_t memOR(uint8_t memAddr, uint16_t mask);
static uint8_t memNOT(uint8_t memAddr);
static uint8_t memHigh(uint8_t memAddr, uint16_t mask);
static uint8_t memLow(uint8_t memAddr, uint16_t mask);
static uint16_t memSubRead(uint8_t memAddr, uint16_t mask);
static uint8_t memSubWrite(uint8_t memAddr, uint16_t inputWord, uint16_t mask);

void AR1010_init()
{
	for (uint8_t i = 1; i < 18; i++)
		reg_write(i, initialRegisters[i]);
	reg_write(0x00, initialRegisters[0]);
	//while(!memSubRead(0x13, 0x0020));
	memLow(0x01, 0x000E); //disable HMUTE and SMUTE
	memSubWrite(0x03, 0B11 << 3, 0B11 << 3); //Setup Band and Space
	memSubWrite(0x03, 0B1000 << 7, 0B1111 << 7); //Set Volume

}

uint8_t reg_write(uint8_t memAddr, uint16_t inputWord)
{
	uint8_t upper = (inputWord & 0xFF00) >> 8;
	uint8_t lower = (inputWord & 0x00FF);
	uint8_t data[3];
	data[0] = memAddr;
	data[1] = upper;
	data[2] = lower;
	I2C_write(addr, data, 3);
	HAL_Delay(10);
	return 0;
}

uint16_t reg_read(uint8_t memAddr)
{
	I2C_write(addr, &memAddr, 1);
	uint8_t read[2];
	I2C_read(addr, read, 2);
	uint8_t upper = read[0];
	uint8_t lower = read[1];
	uint16_t outputWord = (upper << 8) + lower;
	HAL_Delay(10);
	return outputWord;
}

uint8_t memXOR(uint8_t memAddr, uint16_t mask)
{
	uint16_t opWord = reg_read(memAddr);
	opWord = opWord ^ mask;
	return reg_write(memAddr, opWord);
}

uint8_t memAND(uint8_t memAddr, uint16_t mask)
{
	uint16_t opWord = reg_read(memAddr);
	opWord = opWord & mask;
	return reg_write(memAddr, opWord);
}

uint8_t memOR(uint8_t memAddr, uint16_t mask)
{
	uint16_t opWord = reg_read(memAddr);
	opWord = opWord | mask;
	return reg_write(memAddr, opWord);
}

uint8_t memNOT(uint8_t memAddr)
{
	uint16_t opWord = reg_read(memAddr);
	opWord = ~opWord;
	return reg_write(memAddr, opWord);
}

uint8_t memHigh(uint8_t memAddr, uint16_t mask)
{
	return memOR(memAddr, mask);
}

uint8_t memLow(uint8_t memAddr, uint16_t mask)
{
	return memAND(memAddr, ~mask);
}

uint16_t memSubRead(uint8_t memAddr, uint16_t mask)
{
	uint16_t opWord = reg_read(memAddr);
	return opWord & mask;
}

uint8_t memSubWrite(uint8_t memAddr, uint16_t inputWord, uint16_t mask)
{
	uint16_t opWord = reg_read(memAddr);
	opWord = opWord & ~mask;
	inputWord = inputWord & mask;
	uint16_t outputWord = opWord | inputWord;
	return reg_write(memAddr, outputWord);
}

void AR1010_scan(uint16_t lower, uint16_t upper)
{
	uint16_t i;
	for (i = lower; i <= upper; i++)
	{
		AR1010_tune((float) (i * 0.1), 1);
		HAL_Delay(500);
	}
	// !!!
}

void AR1010_tune(float freq, uint8_t convert)
{ //freq in MHz as a float
	uint16_t chan;
	if (convert)
		chan = (uint16_t) (freq * 10 - 690);
	else
		chan = (uint16_t) freq;

	memHigh(0x01, 0x0002);                //Set hmute
	memLow(0x02, 0x0200);                 //Clear TUNE
	memLow(0x03, 0x4000);                 //Clear SEEK
	memSubWrite(0x02, chan, 0x01FF);      //Set CHAN
	memHigh(0x02, 0x0200);                //Enable TUNE
	while (!memSubRead(0x13, 0x0020))
		;     //Wait STC
	memLow(0x01, 0x0002);                 //Clear hmute
}

void AR1010_auto_tune(float freq, uint8_t convert)
{ //freq in MHz as float
	uint16_t chan;
	if (convert)
		chan = (uint16_t) (freq * 10 - 690);
	else
		chan = (uint16_t) freq;

	memHigh(0x01, 0x0002);										//Set hmute
	memLow(0x02, 0x0200);										//Clear TUNE
	memLow(0x03, 0x4000);										//Clear SEEK
	memSubWrite(0x02, chan, 0x01FF);							//Set CHAN
														//Read Low-side LO injection
	memSubWrite(0x0B, 0x0000, 0x8005);							//Set R11 (Clear D15, Clear D0/D2)
	memHigh(0x02, 0x0200);										//Enable TUNE
	while (!memSubRead(0x13, 0x0020))
		;                   //Wait for STC flag
	uint8_t RSSI1 = memSubRead(0x12, 0xFE00);					//Get RSSI1
	memLow(0x02, 0x0200);										//Clear TUNE
														//Read High-side LO injection
	memSubWrite(0x0B, 0x8005, 0x8005);							//Set R11 (Set D15, Set D0/D2)
	memHigh(0x02, 0x0200);										//Enable TUNE
	while (!memSubRead(0x13, 0x0020))
		;                   //Wait for STC flag
	uint8_t RSSI2 = memSubRead(0x12, 0xFE00);					//Get RSSI2
	memLow(0x02, 0x0200);										//Clear TUNE
														//Compare Hi-Lo strength
	if (RSSI1 > RSSI2)
		memSubWrite(0x0B, 0x0005, 0x8005);						//(RSSI1>RSSI2)?R11(Clear D15, Set D0/D2)
	else
		memSubWrite(0x0B, 0x0000, 0x8000);						//:R11(Set D11, Clear D0/D2)
	memHigh(0x02, 0x0200);										//Enable TUNE
	while (!memSubRead(0x13, 0x0020));							//Wait STC
	memLow(0x01, 0x0002);										//Clear hmute
}

void AR1010_seek()
{ //NEEDS WORK
	memHigh(0x01, 0x0002);                                  //Set hmute
	memLow(0x02, 0x0200);                                   //Clear TUNE
	memSubWrite(0x02, memSubRead(0x13, 0xFF80) >> 7, 0x01FF); //Set CHAN = READCHAN
	memLow(0x03, 0x4000);                                   //Clear SEEK
	memHigh(0x03, 0x8010);                   //Set SEEKUP/SEEKTH !!!TWEAK SEEKTH
	memHigh(0x03, 0x4000);                                  //Enable SEEK
	while (!memSubRead(0x13, 0x0020));                       //Wait STC
	memLow(0x01, 0x0002);                                   //Clear hmute
}

void AR1010_auto_seek()
{ //NEEDS WORK
	memHigh(0x01, 0x0002);                                  //Set hmute
	memLow(0x02, 0x0200);                                   //Clear TUNE
	memSubWrite(0x02, memSubRead(0x13, 0xFF80) >> 7, 0x01FF); //Set CHAN = READCHAN
	memLow(0x03, 0x4000);                                   //Clear SEEK
	memHigh(0x03, 0x8010);                   //Set SEEKUP/SEEKTH !!!TWEAK SEEKTH
	memHigh(0x03, 0x4000);                                  //Enable SEEK
	while (!memSubRead(0x13, 0x0020));                       //Wait STC
	if (!memSubRead(0x13, 0x0010))                           //If !SF
		AR1010_auto_tune((float) (memSubRead(0x13, 0xFF80) >> 7), 0); //autoTune with READCHAN                                             //
	memLow(0x01, 0x0002);                                   //Clear hmute
}

void AR1010_set_volume(uint8_t volume)
{
	if (volume > 18)
		volume = 18;
	int write1 = volume1_conv[volume];
	int write2 = volume2_conv[volume];
	memSubWrite(0x03, write1 << 7, 0B1111 << 7);
	memSubWrite(0x0E, write2 << 12, 0B1111 << 12);
}
