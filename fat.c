/*******************************************************************************
* Includes
******************************************************************************/
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include "fat.h"
#include "HAL.h"

/*******************************************************************************
* Definitions
******************************************************************************/
#pragma pack(1)

typedef struct
{
    uint8_t jump[3];
    uint8_t name[8];
    uint16_t bytes_per_sector;
    uint8_t sectors_per_cluster;
    uint16_t size_of_reserved_area;
    uint8_t numbers_of_fats;
    uint16_t max_root_entries;
    uint16_t total_sectors;
    uint16_t fat_size;
    uint8_t label_type[8];
    uint8_t signature[2];
} fat_boot_info_struct_t;

enum fat_EOF
{
    fat_EOF_12 = 0xFFF,
    fat_EOF_16 = 0xFFFF,
    fat_EOF_32 = 0x0FFFFFFF
};

/*******************************************************************************
* Prototypes
******************************************************************************/
static void fat_read_boot_info(void);
static void fat_read_root();
static void fat_read_entries(uint8_t* buff,uint32_t bytes_count);
static void free_entries(fat_entry** head_temp);
bool fat_read_option(uint32_t option,fat_entry** headTemp);

/*******************************************************************************
* Variables
******************************************************************************/
static uint8_t boot_info[512];
static uint32_t boot_first_index = 0;
static uint32_t fat1_first_index = 0;
static uint32_t fat2_first_index = 0;
static uint32_t root_first_index = 0; // fat12 and fat16 only
static uint32_t data_first_index = 0;
static uint32_t root_size = 0; // in sectors
static uint32_t end_of_file = 0;
static uint32_t linked_list_count = 0;
static fat_boot_info_struct_t fat;
fat_entry* entry_head = NULL;

/*******************************************************************************
* Code
******************************************************************************/
bool fat_init(uint8_t* file_path,fat_entry** headTemp)
{
    bool retValue = true;
    if(kmc_open_file(file_path))
    {
        fat_read_boot_info();
        fat_read_root();
        *headTemp = entry_head;
    }
    else
    {
        retValue = false;
    }
    return retValue;
}

static void fat_read_boot_info(void)
{
    kmc_read_sector(boot_first_index,&boot_info[0]);
    strncpy(fat.name,boot_info+3,8);
    fat.bytes_per_sector = READ_16_BITS(boot_info[0x0b],boot_info[0x0c]);
    fat.sectors_per_cluster = boot_info[0x0d];
    fat.size_of_reserved_area = READ_16_BITS(boot_info[0x0e],boot_info[0x0f]);
    fat.numbers_of_fats = boot_info[0x10];
    fat.max_root_entries = READ_16_BITS(boot_info[0x11],boot_info[0x12]);
    fat.total_sectors = READ_16_BITS(boot_info[0x013],boot_info[0x14]);
    fat.fat_size = READ_16_BITS(boot_info[0x016],boot_info[0x17]);
    if(fat.max_root_entries != 0) /* FAT12/16 */
    {
        strncpy(fat.label_type,boot_info+0x36,8);
    }
    else /* FAT32 */
    {
        strncpy(fat.label_type,boot_info+0x52,8);
    }
    kmc_update_sector_size(fat.bytes_per_sector); /***** update sector size in HAL.c file *****/
    fat1_first_index = boot_first_index + 1;
    fat2_first_index = fat1_first_index + fat.fat_size;
    if(fat.max_root_entries != 0) /* FAT12/16 */
    {
        root_first_index = fat2_first_index + fat.fat_size;
        root_size = (32*fat.max_root_entries)/fat.bytes_per_sector;
        data_first_index = root_first_index + root_size;
    }
    else /* FAT32 */
    {
        data_first_index = fat2_first_index + fat.fat_size;
    }
    if((fat.total_sectors/fat.sectors_per_cluster) < 4085) /* find total clusters,FAT12 */
    {
        end_of_file = fat_EOF_12;
    }
    else if ((fat.total_sectors/fat.sectors_per_cluster) < 65525) /* find total clusters,FAT16 */
    {
        end_of_file = fat_EOF_16;
    }
    else /* find total clusters,FAT32 */
    {
        end_of_file = fat_EOF_32;
    }
}

static void fat_read_root(void)
{
    uint8_t* buff_root = NULL;
    uint8_t* buff_FAT = NULL;
    uint16_t i = 0;
    uint16_t current_cluster = 0;
    uint16_t fat_index = 0;
    uint16_t next_cluster = 0;
    uint16_t first_sector = 0;
    uint32_t count = 1;
    uint32_t total_bytes_read = 0;

    if(end_of_file == fat_EOF_12 || end_of_file == fat_EOF_16)
    {
        buff_root = (uint8_t*)malloc((sizeof(uint8_t)* \
                                root_size* \
                                fat.bytes_per_sector));
        if(buff_root == NULL)
        {
            printf("\nunable to allocate memory");
            exit(EXIT_FAILURE);
        }
        total_bytes_read = kmc_read_multi_sector(root_first_index,
                                                root_size,
                                                buff_root);
    }
    else if (end_of_file == fat_EOF_32) // FAT32
    {
        buff_FAT = (uint8_t*)malloc(sizeof(uint8_t)* \
                                fat.bytes_per_sector* \
                                fat.sectors_per_cluster* \
                                fat.fat_size);
        if(buff_FAT == NULL)
        {
            printf("\nunable to allocate memory!");
            exit(EXIT_FAILURE);
        }
        kmc_read_multi_sector(fat1_first_index,fat.fat_size,buff_FAT);
        buff_root = (uint8_t*)malloc(sizeof(uint8_t)* \
                                    fat.bytes_per_sector* \
                                    fat.sectors_per_cluster* \
                                    count);
        if(buff_root == NULL)
        {
            printf("\nunable to allocate memory!");
            exit(0);
        }
        /*
         *   current_cluster = READ_32_BITS(boot_info[0x2C],
         *                                   boot_info[0x2D],
         *                                   boot_info[0x2E],
         *                                   boot_info[0x2F]);
         *  first cluster of FAT32 root is usually 2 but not always
         *  and I don't have a FAT32 image disk to test
         */
        current_cluster = 2;
        fat_index = current_cluster * 4;
        first_sector = 1 + \
                    fat.fat_size * 2 + \
                    (current_cluster-2) * fat.sectors_per_cluster;
        total_bytes_read = kmc_read_multi_sector(first_sector,
                                                fat.sectors_per_cluster,
                                                buff_root);
        next_cluster = READ_32_BITS(buff_FAT[fat_index],
                                    buff_FAT[fat_index+1],
                                    buff_FAT[fat_index+2],
                                    buff_FAT[fat_index+3]);
        while(next_cluster != end_of_file)
        {
            count += 1;
            current_cluster = next_cluster;
            fat_index = current_cluster*4;
            first_sector = 1 + \
                        fat.fat_size * 2 + \
                        (current_cluster-2) * fat.sectors_per_cluster;
            buff_root = realloc(buff_root,sizeof(uint8_t)* \
                        fat.bytes_per_sector* \
                        fat.sectors_per_cluster* \
                        count);
            if(buff_root == NULL)
            {
                printf("\nunable to allocate memory!");
                exit(EXIT_FAILURE);
            }
            total_bytes_read += kmc_read_multi_sector(
                                first_sector,
                                fat.sectors_per_cluster,
                                buff_root + \
                                fat.bytes_per_sector*fat.sectors_per_cluster);
            next_cluster = READ_32_BITS(buff_FAT[fat_index],
                                        buff_FAT[fat_index+1],
                                        buff_FAT[fat_index+2],
                                        buff_FAT[fat_index+3]);
        }
    }
    fat_read_entries(buff_root,total_bytes_read);
    free(buff_root);
    buff_root = NULL;
}

void fat_read_entries(uint8_t* buff,uint32_t bytes_count)
{
    uint32_t i = 0;
    uint32_t j = 0;
    fat_entry* temp = NULL;
    fat_entry* new_entry = NULL;
    uint8_t k = 0;
    linked_list_count = 0;

    free_entries(&entry_head);
    while(i<bytes_count)
    {
        if(buff[i] != 0x00) /* empty entry */
        {
            new_entry = (fat_entry*)malloc(sizeof(fat_entry));
            if(new_entry == NULL)
            {
                printf("\nunable to allocate memory");
                exit(EXIT_FAILURE);
            }
            temp = entry_head;
            new_entry->next = NULL;
            if(entry_head == NULL)
            {
                entry_head = new_entry;
            }
            else
            {
                while(temp->next != NULL)
                {
                    temp = temp->next;
                }
                temp->next = new_entry;
            }
            linked_list_count += 1;
            if(buff[i+0x0B] == 0x0F) /* long file name */
            {
                j = i;
                strcpy(new_entry->LFN,""); // new_entry->LFN == temp->LFN => you are fucked
                while(buff[j+0x0B] == 0x0F)
                {
                    // do something
                    j += 32; /* need more 32 bytes for info */
                }
                i = j; /*condition*/
            }
            if (buff[i+0x0B] != 0x0F)
            {
                strncpy(new_entry->SFN,&buff[i+0x00],8);
                new_entry->SFN[8] = '\0';
                strncpy(new_entry->extension,&buff[i+0x08],3);
                new_entry->extension[3] = '\0';
                new_entry->attribute = buff[i+0x0b];
                strncpy(new_entry->first_cluster_fat32,&buff[i+0x14],2);
                strncpy(new_entry->modified_time,&buff[i+0x16],2);
                strncpy(new_entry->modified_date,&buff[i+0x18],2);
                strncpy(new_entry->first_cluster_fat12_fat16,&buff[i+0x1A],2);
                /*
                 * strncpy(new_entry->size,&buff[i+0x1C],4); => undefined behavior
                 */
                new_entry->size[0] = buff[i + 0x1C];
                new_entry->size[1] = buff[i + 0x1D];
                new_entry->size[2] = buff[i + 0x1E];
                new_entry->size[3] = buff[i + 0x1F];
            }
        }
        i+=32;
    }
}

uint8_t fat_read(uint32_t option,fat_entry** headTemp,uint8_t** buff_file,uint32_t* byte_count)
{
    uint8_t retValue = 0;
    uint8_t* buff = NULL;
    uint8_t* buff_FAT = NULL;
    uint16_t i = 0;
    uint16_t current_cluster = 0;
    uint16_t fat_index = 0;
    uint16_t next_cluster = 0;
    uint16_t first_sector = 0;
    uint32_t count = 1;
    uint32_t total_bytes_read = 0;
    fat_entry* temp_val = entry_head;

    for(i=0;i<option;i++)
    {
        temp_val = temp_val->next;
    }
    if(READ_16_BITS(temp_val->first_cluster_fat12_fat16[0],temp_val->first_cluster_fat12_fat16[1]) == 0 || fat.max_root_entries == 0)
    {
        fat_read_root();
        *headTemp = entry_head;
        retValue = 0;
    }
    else
    {
        buff_FAT = (uint8_t*)malloc(sizeof(uint8_t)* \
                                    fat.bytes_per_sector* \
                                    fat.sectors_per_cluster* \
                                    fat.fat_size);
        if(buff_FAT == NULL)
        {
            printf("\nunable to allocate memory!");
            exit(EXIT_FAILURE);
        }
        kmc_read_multi_sector(fat1_first_index,fat.fat_size,buff_FAT);
        buff = (uint8_t*)malloc(sizeof(uint8_t)* \
                                    fat.bytes_per_sector* \
                                    fat.sectors_per_cluster* \
                                    count);
        if(buff == NULL)
        {
            printf("\nunable to allocate memory!");
            exit(0);
        }
        current_cluster = READ_16_BITS(temp_val->first_cluster_fat12_fat16[0],
                                        temp_val->first_cluster_fat12_fat16[1]);
        /*
        * read next cluster according to FAT12 or FAT 16 or FAT 32
        */
        if(end_of_file == fat_EOF_12)
        {
            fat_index = current_cluster * 1.5;
            first_sector = 1 + \
                            fat.fat_size * 2 + \
                            root_size + \
                            (current_cluster-2) * fat.sectors_per_cluster;
            if((current_cluster % 2) == 0)
            {
                next_cluster = READ_12_BITS_EVEN(buff_FAT[fat_index],
                                                buff_FAT[fat_index+1]);
            }
            else if ((current_cluster % 2) != 0)
            {
                next_cluster = READ_12_BITS_ODD(buff_FAT[fat_index],
                                                buff_FAT[fat_index+1]);
            } 
        }
        else if (end_of_file == fat_EOF_16)
        {
            fat_index = current_cluster * 2;
            first_sector = 1 + \
                            fat.fat_size * 2 + \
                            root_size + \
                            (current_cluster-2) * fat.sectors_per_cluster;
            next_cluster = READ_16_BITS(buff_FAT[fat_index],
                                        buff_FAT[fat_index+1]);
        }
        else if (end_of_file == fat_EOF_32)
        {
            fat_index = current_cluster * 4;
            first_sector = 1 + \
                            fat.fat_size * 2 + \
                            (current_cluster-2) * fat.sectors_per_cluster; // no root
            next_cluster = READ_32_BITS(buff_FAT[fat_index],
                                        buff_FAT[fat_index+1],
                                        buff_FAT[fat_index+2],
                                        buff_FAT[fat_index+3]);
        }
        total_bytes_read = kmc_read_multi_sector(first_sector,
                                                fat.sectors_per_cluster,
                                                buff);
        while(next_cluster != end_of_file)
        {
            count += 1;
            current_cluster = next_cluster;
            fat_index = current_cluster*1.5;
            first_sector = 1 + \
                        fat.fat_size * 2 + \
                        root_size + \
                        (current_cluster-2)* \
                        fat.sectors_per_cluster;
            buff = realloc(buff,sizeof(uint8_t)* \
                        fat.bytes_per_sector* \
                        fat.sectors_per_cluster* \
                        count);
            total_bytes_read += kmc_read_multi_sector(
                                first_sector,
                                fat.sectors_per_cluster,
                                buff + \
                                fat.bytes_per_sector*fat.sectors_per_cluster);

            /*
            * read next cluster according to FAT12 or FAT 16 or FAT 32
            */
            if(end_of_file == fat_EOF_12)
            {
                if((current_cluster % 2) == 0)
                {
                    next_cluster = READ_12_BITS_EVEN(buff_FAT[fat_index],
                                                    buff_FAT[fat_index+1]);
                }
                else if ((current_cluster % 2) != 0)
                {
                    next_cluster = READ_12_BITS_ODD(buff_FAT[fat_index],
                                                    buff_FAT[fat_index+1]);
                } 
            }
            else if (end_of_file == fat_EOF_16)
            {
                next_cluster = READ_16_BITS(buff_FAT[fat_index],
                                            buff_FAT[fat_index+1]);
            }
            else if (end_of_file == fat_EOF_32)
            {
                next_cluster = READ_32_BITS(buff_FAT[fat_index],
                                            buff_FAT[fat_index+1],
                                            buff_FAT[fat_index+2],
                                            buff_FAT[fat_index+3]);
            }
        }
        if(temp_val->attribute == 0x10)
        {
            fat_read_entries(buff,total_bytes_read);
            retValue = 1;
        }
        else if (temp_val->attribute == 0x00)
        {
            *byte_count = total_bytes_read;
            retValue = 2;
        }
        *headTemp = entry_head;
        *buff_file = buff;
        //free(buff);
        free(buff_FAT);
        buff = NULL;
        buff_FAT = NULL;
    }

    return retValue;
}

static void free_entries(fat_entry** head_temp)
{
    fat_entry* current = *head_temp;
    fat_entry* next = NULL;

    while(current != NULL)
    {
        next = current->next;
        free(current);
        current = next;
    }
    *head_temp = NULL;
}
