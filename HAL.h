#ifndef _HAL_H_
#define _HAL_H_

/*******************************************************************************
* API
******************************************************************************/
bool kmc_open_file(uint8_t* buff);
int32_t kmc_read_sector(uint32_t index, uint8_t* buff);
int32_t kmc_read_multi_sector(uint32_t index, uint32_t num, uint8_t* buff);
uint32_t kmc_update_sector_size (uint32_t size);

#endif /* _HAL_H_ */
