#ifndef _FAT_H_
#define _FAT_H_

/*******************************************************************************
* Definitions
******************************************************************************/
#define READ_12_BITS_ODD(a,b)       (((a)>>4) + ((b)<<4))
#define READ_12_BITS_EVEN(a,b)      ((a) + (((b) & 0x0F) << 8))
#define READ_16_BITS(a,b)           ((b<<8) + (a))
#define READ_32_BITS(a,b,c,d)       ((d<<24) + (c<<16) + (b<<8) + (a))
#define TIME_SECOND(a)              (((a) & 0x001F) * 2)
#define TIME_MINUTE(a)              (((a) & 0x07E0) >> 5)
#define TIME_HOUR(a)                (((a) & 0xF800) >> 11)
#define DATE_DAY(a)                 ((a) & 0x001F)
#define DATE_MONTH(a)               (((a) & 0x01E0) >> 5)
#define DATE_YEAR(a)                ((((a) & 0xFE00) >> 9) + 1980)

enum Fat_Read_Result
{
    FAT_ROOT = 0,
    FAT_SUB_DIR = 1,
    FAT_FILE = 2
};

typedef struct entry
{
    uint8_t LFN[256];                       /*      3 parts               */
    uint8_t SFN[9];                         /*      0x00-0x07 (0-7)       */
    uint8_t extension[4];                   /*      0x08-0x0A (8-10)      */
    uint8_t attribute;                      /*      0x0B (11)             */
    uint8_t reserved;                       /*      0x0C (12)             */
    uint8_t file_creation_time;             /*      0x0D (13)             */
    uint8_t creation_time[2];               /*      0x0E-0x0F (14-15)     */
    uint8_t creation_date[2];               /*      0x10-0x11 (16-17)     */
    uint8_t access_date [2];                /*      0x12-0x13 (18-19)     */
    uint8_t high_first_cluster[2];          /*      0x14-0x15 (20-21)     */
    uint8_t modified_time[2];               /*      0x16-0x17 (22-23)     */
    uint8_t modified_date[2];               /*      0x17-0x18 (24-25)     */
    uint8_t low_first_cluster[2];           /*      0x1A-0x1B (26-27)     */
    uint8_t size[4];                        /*      0x1C-0x1F (28-31)     */
    struct entry* next;
} fat_entry;

/*******************************************************************************
* API
******************************************************************************/

/** @brief This function will call a function in HAL.c to open a file,
 * read data from boot sector, read data from root.
 * @param file_path - file path from user.
 * @param head_temp - a pointer to the linked list in fat.c for first time reading root.
 * @param boot_info - store boot info data for further uses.
 * @return - Return 1 if file was opened successfully or 0 if failed to open file.
 */
bool fat_init(uint8_t* file_path,fat_entry** head_temp,uint8_t* boot_info);


/** @brief This function store data into a linked list pointer and an array.
 * @param option - user choice to open a directory or a file.
 * @param head_temp - a pointer to the linked list in fat.c.
 * @param buff_file - an array that stores data.
 * @return - Return an enum value (FAT_ROOT, FAT_SUB_DIR, FAT_FILE).
 */
uint8_t fat_read(uint32_t option,fat_entry** head_temp,uint8_t** buff_file);


/** @brief This function will call a function in HAL.c to close a file.
 * @param file_path - file path from user.
 * @return - Return 1 if file was closed successfully or 0 if failed to close file.
 */
bool fat_deinit(uint8_t* file_path);

#endif /* _FAT_H_ */
