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
#pragma pack(1) /* allow structure packing */

enum Fat_EOF
{
    FAT_EOF_12 = 0xFFF,
    FAT_EOF_16 = 0xFFFF,
    FAT_EOF_32 = 0x0FFFFFFF
};

typedef struct
{
    uint8_t jump[3];                            /*      0x00-0x02 (0-2)       */
    uint8_t name[8];                            /*      0x03-0x0A (3-10)      */
    uint16_t bytes_per_sector;                  /*      0x0B-0x0C (11-12)     */
    uint8_t sectors_per_cluster;                /*      0x0D (13)             */
    uint16_t size_of_reserved_area;             /*      0x0E-0x0F (14-15)     */
    uint8_t numbers_of_fats;                    /*      0x10 (16)             */
    uint16_t max_root_entries;                  /*      0x11-0x12 (17-18)     */
    uint16_t total_sectors;                     /*      0x13-0x14 (19-20)     */
    uint8_t media_type;                         /*      0x15 (21)             */
    uint16_t fat_size;                          /*      0x16-0x17 (22-23)     */
    uint16_t track_size;                        /*      0x18-0x19 (24-25)     */
    uint16_t number_of_heads;                   /*      0x1A-0x1B (26-27)     */
    uint16_t number_of_hidden_sectors;          /*      0x1C-0x1D (28-29)     */
    uint8_t signature[2];                       /*      0x1FE-0x1FF (510-511) */
} fat_boot_info_struct_t;

/*******************************************************************************
* Prototypes
******************************************************************************/

/** @brief This function is used to read boot info data from disk into an array
 *  and use those datas to set value to some global variables.
 */
static void read_boot_info(void);


/** @brief This function is used to read data from root region and store in a
 *  linked list.
 */
static void read_root();


/** @brief This function reads data from an array then store data in a linked list.
 * @param buff - an array to be read from.
 * @param bytes_count - total number of bytes to be read.
 */
static void read_entries(uint8_t* buff,uint32_t bytes_count);


/** @brief This function will delete a linked list.
 * @param head_temp - head of the linked list.
 */
static void free_entries(fat_entry** head_temp);


/** @brief This function checks if a pointer is equal to NULL (unable to allocate memory).
 * @param ptr - pointer variable to be checked.
 */
static void check_null(void* ptr);

/*******************************************************************************
* Variables
******************************************************************************/
static uint8_t g_boot_info[512];
static uint32_t g_boot_first_index = 0;           /*                usually 0                        */
static uint32_t g_fat1_first_index = 0;
static uint32_t g_fat2_first_index = 0;
static uint32_t g_root_first_index = 0;           /*               FAT12/FAT16 only                  */
static uint32_t g_root_first_cluster = 0;         /* 0 for FAT12/FAT16 - 2 for FAT32(but not always) */
static uint32_t g_data_first_index = 0;
static uint32_t g_root_size = 0;                  /* number of sectors in root (FAT12/FAT16 only)    */
static uint32_t g_end_of_file = 0;
static fat_boot_info_struct_t fat;
fat_entry* entry_head = NULL;

/*******************************************************************************
* Code
******************************************************************************/
bool fat_init(uint8_t* file_path,fat_entry** head_temp,uint8_t* boot_info)
{
    bool retValue = true;
    uint16_t i = 0;
    if(kmc_open_file(file_path))
    {
        read_boot_info();
        read_root();
        *head_temp = entry_head;
        for(i = 0;i < 512;i++)
        {
            boot_info[i] = g_boot_info[i];
        }
    }
    else
    {
        retValue = false;
    }
    return retValue;
}

static void read_boot_info(void)
{
    kmc_read_sector(g_boot_first_index,&g_boot_info[0]);
    strncpy(fat.name,g_boot_info+3,8);
    fat.bytes_per_sector = READ_16_BITS(g_boot_info[0x0B],g_boot_info[0x0C]);
    fat.sectors_per_cluster = g_boot_info[0x0D];
    fat.size_of_reserved_area = READ_16_BITS(g_boot_info[0x0E],g_boot_info[0x0F]);
    fat.numbers_of_fats = g_boot_info[0x10];
    fat.max_root_entries = READ_16_BITS(g_boot_info[0x11],g_boot_info[0x12]);
    fat.total_sectors = READ_16_BITS(g_boot_info[0x013],g_boot_info[0x14]);
    fat.fat_size = READ_16_BITS(g_boot_info[0x016],g_boot_info[0x17]);

    /* update sector size in HAL.c */
    kmc_update_sector_size(fat.bytes_per_sector);

    g_fat1_first_index = g_boot_first_index + 1;
    g_fat2_first_index = g_fat1_first_index + fat.fat_size;
    if(fat.max_root_entries != 0) /* FAT12/16 */
    {
        g_root_first_index = g_fat2_first_index + fat.fat_size;
        g_root_size = (32*fat.max_root_entries)/fat.bytes_per_sector;
        g_data_first_index = g_root_first_index + g_root_size;
        g_root_first_cluster = 0; /* important */
    }
    else /* FAT32 */
    {
        g_data_first_index = g_fat2_first_index + fat.fat_size;
        g_root_first_cluster = READ_32_BITS(g_boot_info[0x2C],g_boot_info[0x2D],g_boot_info[0x2E],g_boot_info[0x2F]); /* important */
    }

    if((fat.total_sectors/fat.sectors_per_cluster) < 4085) /* find total clusters,FAT12 */
    {
        g_end_of_file = FAT_EOF_12;
    }
    else if ((fat.total_sectors/fat.sectors_per_cluster) < 65525) /* find total clusters,FAT16 */
    {
        g_end_of_file = FAT_EOF_16;
    }
    else /* find total clusters,FAT32 */
    {
        g_end_of_file = FAT_EOF_32;
    }
}

static void read_root(void)
{
    uint8_t* p_buff_root = NULL;
    uint8_t* p_buff_FAT = NULL;
    uint16_t i = 0;
    uint16_t current_cluster = 0;
    uint16_t fat_index = 0;
    uint16_t next_cluster = 0;
    uint16_t first_sector = 0;
    uint32_t count = 1;
    uint32_t total_bytes_read = 0;

    if(g_end_of_file == FAT_EOF_12 || g_end_of_file == FAT_EOF_16)
    {
        p_buff_root = (uint8_t*)malloc((sizeof(uint8_t)*g_root_size*fat.bytes_per_sector));
        check_null(p_buff_root);
        total_bytes_read = kmc_read_multi_sector(g_root_first_index,g_root_size,p_buff_root);
    }
    else if (g_end_of_file == FAT_EOF_32)
    {
        p_buff_FAT = (uint8_t*)malloc(sizeof(uint8_t)*fat.bytes_per_sector*fat.sectors_per_cluster*fat.fat_size);
        check_null(p_buff_FAT);
        p_buff_root = (uint8_t*)malloc(sizeof(uint8_t)*fat.bytes_per_sector*fat.sectors_per_cluster*count);
        check_null(p_buff_root);
        kmc_read_multi_sector(g_fat1_first_index,fat.fat_size,p_buff_FAT);
        current_cluster = g_root_first_cluster;
        fat_index = current_cluster * 4;
        first_sector = 1 + fat.fat_size * 2 + (current_cluster-2) * fat.sectors_per_cluster;
        total_bytes_read = kmc_read_multi_sector(first_sector,fat.sectors_per_cluster,p_buff_root);
        next_cluster = READ_32_BITS(p_buff_FAT[fat_index],p_buff_FAT[fat_index+1],p_buff_FAT[fat_index+2],p_buff_FAT[fat_index+3]);
        while(next_cluster != g_end_of_file)
        {
            count += 1;
            current_cluster = next_cluster;
            fat_index = current_cluster * 4;
            first_sector = 1 + fat.fat_size * 2 + (current_cluster-2) * fat.sectors_per_cluster;
            p_buff_root = realloc(p_buff_root,sizeof(uint8_t)*fat.bytes_per_sector*fat.sectors_per_cluster*count);
            check_null(p_buff_root);
            total_bytes_read += kmc_read_multi_sector(first_sector,fat.sectors_per_cluster,p_buff_root + fat.bytes_per_sector*fat.sectors_per_cluster);
            next_cluster = READ_32_BITS(p_buff_FAT[fat_index],p_buff_FAT[fat_index+1],p_buff_FAT[fat_index+2],p_buff_FAT[fat_index+3]);
        }
    }
    read_entries(p_buff_root,total_bytes_read);
    free(p_buff_root);
    p_buff_root = NULL;
}

void read_entries(uint8_t* buff,uint32_t bytes_count)
{
    uint32_t i = 0;
    uint32_t j = 0;
    uint8_t k = 0;
    uint8_t LFN_entries = 0;
    fat_entry* temp = NULL;
    fat_entry* new_entry = NULL;

    free_entries(&entry_head);
    while(i < bytes_count)
    {
        if(buff[i] == 0x00 || buff[i] == 0xE5) /* empty entry or deleted entry */
        {
            /* just check, don't do anything */
        }
        else
        {
            new_entry = (fat_entry*)malloc(sizeof(fat_entry));
            check_null(new_entry);
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

            strcpy(new_entry->LFN,"");
            if(buff[i + 0x0B] == 0x0F) /* long filename */
            {
                j = i;
                LFN_entries = buff[j] & 0x1F; /* clear bits 5->7, keep bits 0->4 */
                i = i + LFN_entries * 32; /* move i to SFN */
                while(LFN_entries > 0)
                {
                    for(k = 0x01;k < 0x0A;k++)
                    {
                        if((buff[j + (LFN_entries - 1)*32 + k] == 0x00) || (buff[j + (LFN_entries - 1)*32 + k] == 0xFF))
                        {
                            /* just check, don't do anything */
                        }
                        else
                        {
                            strncat(new_entry->LFN,&buff[j + (LFN_entries - 1)*32 + k],1);
                        }
                    }

                    for(k = 0x0E;k < 0x19;k++)
                    {
                        if((buff[j + (LFN_entries - 1)*32 + k] == 0x00) || (buff[j + (LFN_entries - 1)*32 + k] == 0xFF))
                        {
                            /* just check, don't do anything */
                        }
                        else
                        {
                            strncat(new_entry->LFN,&buff[j + (LFN_entries - 1)*32 + k],1);
                        }
                    }

                    for(k = 0x1C;k < 0x1F;k++)
                    {
                        if((buff[j + (LFN_entries - 1)*32 + k] == 0x00) || (buff[j + (LFN_entries - 1)*32 + k] == 0xFF))
                        {
                            /* just check, don't do anything */
                        }
                        else
                        {
                            strncat(new_entry->LFN,&buff[j + (LFN_entries - 1)*32 + k],1);
                        }
                    }
                    LFN_entries = LFN_entries - 1;
                }
            }

            strncpy(new_entry->SFN,&buff[i + 0x00],8);
            new_entry->SFN[8] = '\0';
            strncpy(new_entry->extension,&buff[i+0x08],3);
            new_entry->extension[3] = '\0';
            if(strcmp(new_entry->LFN,"") == 0)
            {
                strncpy(new_entry->LFN,new_entry->SFN,8);
                new_entry->LFN[8] = '\0';
            }
            new_entry->attribute = buff[i + 0x0b];
            strncpy(new_entry->high_first_cluster,&buff[i + 0x14],2);
            strncpy(new_entry->modified_time,&buff[i + 0x16],2);
            strncpy(new_entry->modified_date,&buff[i + 0x18],2);
            strncpy(new_entry->low_first_cluster,&buff[i + 0x1A],2);
            /* don't use "strncpy(new_entry->size,&buff[i+0x1C],4);" here, it will cause undefined behavior */
            new_entry->size[0] = buff[i + 0x1C];
            new_entry->size[1] = buff[i + 0x1D];
            new_entry->size[2] = buff[i + 0x1E];
            new_entry->size[3] = buff[i + 0x1F];
        }
        i += 32;
    }
}

uint8_t fat_read(uint32_t option,fat_entry** head_temp,uint8_t** buff_file)
{
    uint8_t retValue = 0;
    uint8_t* p_buff = NULL;
    uint8_t* p_buff_FAT = NULL;
    uint16_t i = 0;
    uint32_t current_cluster = 0;
    uint16_t fat_index = 0;
    uint32_t next_cluster = 0;
    uint16_t first_sector = 0;
    uint32_t count = 1;
    uint32_t total_bytes_read = 0;
    fat_entry* temp = entry_head;

    for(i = 0;i < option;i++)
    {
        temp = temp->next;
    }
    current_cluster = READ_32_BITS(temp->low_first_cluster[0],temp->low_first_cluster[1],temp->high_first_cluster[0],temp->high_first_cluster[1]);

    if(current_cluster == g_root_first_cluster)
    {
        read_root();
        *head_temp = entry_head;
        retValue = FAT_ROOT;
    }
    else
    {
        p_buff_FAT = (uint8_t*)malloc(sizeof(uint8_t)*fat.bytes_per_sector*fat.sectors_per_cluster*fat.fat_size);
        check_null(p_buff_FAT);
        p_buff = (uint8_t*)malloc(sizeof(uint8_t)*fat.bytes_per_sector*fat.sectors_per_cluster*count);
        check_null(p_buff);
        kmc_read_multi_sector(g_fat1_first_index,fat.fat_size,p_buff_FAT);

        /* read next cluster according to FAT12 or FAT 16 or FAT 32 */
        if(g_end_of_file == FAT_EOF_12)
        {
            fat_index = current_cluster * 1.5;
            first_sector = 1 + fat.fat_size * 2 + g_root_size + (current_cluster-2) * fat.sectors_per_cluster;
            if((current_cluster % 2) == 0)
            {
                next_cluster = READ_12_BITS_EVEN(p_buff_FAT[fat_index],p_buff_FAT[fat_index+1]);
            }
            else if ((current_cluster % 2) != 0)
            {
                next_cluster = READ_12_BITS_ODD(p_buff_FAT[fat_index],p_buff_FAT[fat_index+1]);
            }
        }
        else if (g_end_of_file == FAT_EOF_16)
        {
            fat_index = current_cluster * 2;
            first_sector = 1 + fat.fat_size * 2 + g_root_size + (current_cluster-2) * fat.sectors_per_cluster;
            next_cluster = READ_16_BITS(p_buff_FAT[fat_index],p_buff_FAT[fat_index+1]);
        }
        else if (g_end_of_file == FAT_EOF_32)
        {
            fat_index = current_cluster * 4;
            first_sector = 1 + fat.fat_size * 2 + (current_cluster-2) * fat.sectors_per_cluster;
            next_cluster = READ_32_BITS(p_buff_FAT[fat_index],p_buff_FAT[fat_index+1],p_buff_FAT[fat_index+2],p_buff_FAT[fat_index+3]);
        }
        total_bytes_read = kmc_read_multi_sector(first_sector,fat.sectors_per_cluster,p_buff);

        while(next_cluster != g_end_of_file)
        {
            count += 1;
            current_cluster = next_cluster;
            if(g_end_of_file == FAT_EOF_12)
            {
                fat_index = current_cluster * 1.5;
                first_sector = 1 + fat.fat_size * 2 + g_root_size + (current_cluster-2) * fat.sectors_per_cluster;
                if((current_cluster % 2) == 0)
                {
                    next_cluster = READ_12_BITS_EVEN(p_buff_FAT[fat_index],p_buff_FAT[fat_index+1]);
                }
                else if ((current_cluster % 2) != 0)
                {
                    next_cluster = READ_12_BITS_ODD(p_buff_FAT[fat_index],p_buff_FAT[fat_index+1]);
                }
            }
            else if (g_end_of_file == FAT_EOF_16)
            {
                fat_index = current_cluster * 2;
                first_sector = 1 + fat.fat_size * 2 + g_root_size + (current_cluster-2) * fat.sectors_per_cluster;
                next_cluster = READ_16_BITS(p_buff_FAT[fat_index],p_buff_FAT[fat_index+1]);
            }
            else if (g_end_of_file == FAT_EOF_32)
            {
                fat_index = current_cluster * 4;
                first_sector = 1 + fat.fat_size * 2 + (current_cluster-2) * fat.sectors_per_cluster;
                next_cluster = READ_32_BITS(p_buff_FAT[fat_index],p_buff_FAT[fat_index+1],p_buff_FAT[fat_index+2],p_buff_FAT[fat_index+3]);
            }
            p_buff = realloc(p_buff,sizeof(uint8_t)*fat.bytes_per_sector*fat.sectors_per_cluster*count);
            check_null(p_buff);
            total_bytes_read += kmc_read_multi_sector(first_sector,fat.sectors_per_cluster,p_buff + fat.bytes_per_sector*fat.sectors_per_cluster);
        }

        if(temp->attribute == 0x10)
        {
            read_entries(p_buff,total_bytes_read);
            retValue = FAT_SUB_DIR;
        }
        else if (temp->attribute == 0x00)
        {
            retValue = FAT_FILE;
        }
        *head_temp = entry_head;
        *buff_file = p_buff;
        free(p_buff_FAT);
        p_buff = NULL;
        p_buff_FAT = NULL;
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

static void check_null(void* ptr)
{
    if(ptr == NULL)
    {
        exit(1);
    }
}

bool fat_deinit(uint8_t* file_path)
{
    bool retValue = true;

    if(!kmc_close_file(file_path))
    {
        retValue = false;
    }
    return retValue;
}