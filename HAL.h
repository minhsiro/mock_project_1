#ifndef _HAL_H_
#define _HAL_H_

/*******************************************************************************
* API
******************************************************************************/

/** @brief This function is used to open file.
 * @param buff - file path from user.
 * @return - Return 1 if file was opened successfully or 0 if failed to open file.
 */
bool kmc_open_file(uint8_t* buff);


/** @brief This function is used to read data from a sector into an array.
 * @param index - sector number that you want to read
 * @param buff - an array to store byte values after reading.
 * @return - Return a number of total bytes read.
 */
int32_t kmc_read_sector(uint32_t index, uint8_t* buff);


/** @brief This function is used to read data from multiple sectors into an array.
 * @param index - starting sector to read from.
 * @param num - number of sectors.
 * @param buff - an array to store byte values after reading.
 * @return - Return a number of total bytes read.
 */
int32_t kmc_read_multi_sector(uint32_t index, uint32_t num, uint8_t* buff);


/** @brief This function is used to update sector size after reading boot sector.
 * @param size - sector size (read from boot sector).
 * @return - Return value is not used.
 */
uint32_t kmc_update_sector_size (uint32_t size);


/** @brief This function is used to close a file. 
 * @param buff - file path from user.
 * @return - Return 1 if file was closed successfully or 0 if failed to close file.
 */
bool kmc_close_file(uint8_t* buff);

#endif /* _HAL_H_ */
