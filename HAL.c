/*******************************************************************************
* Includes
******************************************************************************/
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

/*******************************************************************************
* Definitions
******************************************************************************/
#define KMC_DEFAULT_SECTOR_SIZE (512U)

/*******************************************************************************
* Variables
******************************************************************************/
FILE* floppy = NULL;
static uint16_t kmc_sector_size = KMC_DEFAULT_SECTOR_SIZE;

/*******************************************************************************
* Code
******************************************************************************/
bool kmc_open_file(uint8_t* buff)
{
    bool condition = true;

    floppy = fopen(buff,"rb");
    if(floppy == NULL)
    {
        condition = false;
    }
    /*
     * if users read multiple files in one program's lifetime,
     * set kmc_sector_size back to the default value to avoid
     * errors.
     */
    kmc_sector_size = KMC_DEFAULT_SECTOR_SIZE;
    return condition;
}

uint16_t kmc_update_sector_size (uint16_t size)
{
    uint16_t retVal  = 0;

    if((size % kmc_sector_size) == 0)
    {
        kmc_sector_size = size;
    }
    return retVal;
}

int32_t kmc_read_sector(uint32_t index, uint8_t* buff)
{
    uint32_t ret_value = 0;

    rewind(floppy);
    if((fseek(floppy,1l*index*kmc_sector_size,SEEK_CUR)) == 0)
    {
        ret_value = fread(buff,sizeof(uint8_t),kmc_sector_size,floppy);
    }
    return ret_value;
}

int32_t kmc_read_multi_sector(uint32_t index, uint32_t num, uint8_t* buff)
{
    uint32_t ret_value = 0;

    rewind(floppy);
    if((fseek(floppy,1l*index*kmc_sector_size,SEEK_CUR)) == 0)
    {
        ret_value = fread(buff,sizeof(uint8_t),kmc_sector_size*num,floppy);
    }
    return ret_value;
}

bool kmc_close_file(uint8_t* buff)
{
    bool condition = true;

    if(fclose(floppy) != 0)
    {
        condition = false;
    }
    return condition;
}