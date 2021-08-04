#ifndef _FAT_H_
#define _FAT_H_

/*******************************************************************************
* API
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


enum fat_read_result
{
    FAT_ROOT = 0,
    FAT_SUB_DIR = 1,
    FAT_FILE = 2
};

typedef struct entry
{
    uint8_t LFN[256];                       /* 3 parts                  */
    uint8_t SFN[9];                         /* 0x00-0x07 (0-7)          */
    uint8_t extension[4];                   /* 0x08-0x0A (8-10)         */
    uint8_t attribute;                      /* 0x0B (11)                */
    uint8_t reserved;                       /* 0x0C (12)                */
    uint8_t file_creation_time;             /* 0x0D (13)                */
    uint8_t creation_time[2];               /* 0x0E-0x0F (14-15)        */
    uint8_t creation_date[2];               /* 0x10-0x011 (16-17)       */
    uint8_t access_date [2];                /* 0x12-0x13 (18-19)        */
    uint8_t high_first_cluster[2];          /* 0x014-0x015 (20-21)      */ 
    uint8_t modified_time[2];               /* 0x16-0x17 (22-23)        */
    uint8_t modified_date[2];               /* 0x17-0x18 (24-25)        */
    uint8_t low_first_cluster[2];           /* 0x1A-0x1B (26-27)        */
    uint8_t size[4];                        /* 0x1C-0x1F (28-31)        */
    struct entry* next;
} fat_entry;

bool fat_init(uint8_t* file_path,fat_entry** headTemp);
uint8_t fat_read(uint32_t option,fat_entry** headTemp,uint8_t** buff_file);

#endif /* _FAT_H_ */
