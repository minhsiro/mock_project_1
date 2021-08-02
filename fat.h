#ifndef _FAT_H_
#define _FAT_H_

/*******************************************************************************
* API
******************************************************************************/
#define READ_12_BITS_ODD(a,b)     (((a)>>4) + ((b)<<4))
#define READ_12_BITS_EVEN(a,b)    ((a) + (((b) & 0x0F) << 8))
#define READ_16_BITS(a,b)       ((b<<8) + (a))
#define READ_32_BITS(a,b,c,d)   ((d<<24) + (c<<16) + (b<<8) + (a))

typedef struct entry
{
    uint8_t LFN[256]; // 3 parts
    uint8_t SFN[9]; // 0x00-0x07
    uint8_t extension[4]; // 0x08-0x0A
    uint8_t attribute; // 0x0b
    uint8_t first_cluster_fat32[2]; // 0x014-0x015
    uint8_t modified_time[2]; // 0x16-0x17
    uint8_t modified_date[2]; // 0x17-0x18
    uint8_t first_cluster_fat12_fat16[2]; // 0x1A-0x1B
    uint8_t size[4]; // 0x1C-0x1F
    struct entry* next;
} fat_entry;

bool fat_init(uint8_t* file_path,fat_entry** headTemp);
bool fat_read_option(uint32_t option,fat_entry** headTemp);
uint8_t fat_read(uint32_t option,fat_entry** headTemp,uint8_t** buff_file,uint32_t* byte_count);

#endif /* _FAT_H_ */
