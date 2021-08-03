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
    uint8_t jump[3];                        /* 0x00-0x02 (0-2)          */
    uint8_t name[8];                        /* 0x03-0x0A (3-10)         */
    uint16_t bytes_per_sector;              /* 0x0B-0x0C (11-12)        */
    uint8_t sectors_per_cluster;            /* 0x0D (13)                */
    uint16_t size_of_reserved_area;         /* 0x0E-0x0F (14-15)        */
    uint8_t numbers_of_fats;                /* 0x10 (16)                */
    uint16_t max_root_entries;              /* 0x11-0x12 (17-18)        */
    uint16_t total_sectors;                 /* 0x13-0x14 (19-20)        */
    uint8_t media_type;                     /* 0x15 (21)                */
    uint16_t fat_size;                      /* 0x16-0x17 (22-23)        */
    uint16_t track_size;                    /* 0x18-0x19 (24-25)        */
    uint16_t number_of_heads;               /* 0x1A-0x1B (26-27)        */
    uint16_t number_of_hidden_sectors;      /* 0x1C-0x1D (28-29)        */
    uint8_t signature[2];                   /* 0x1FE-0x1FF (510-511)    */
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
static void fat_free_entries(fat_entry** head_temp);

/*******************************************************************************
* Variables
******************************************************************************/
static uint8_t boot_info[512];
static uint32_t boot_first_index = 0;
static uint32_t fat1_first_index = 0;
static uint32_t fat2_first_index = 0;
static uint32_t root_first_index = 0;           /* FAT12/FAT16 only                                */
static uint32_t root_first_cluster = 0;         /* 0 for FAT12/FAT16 - 2 for FAT32(but not always) */
static uint32_t data_first_index = 0;
static uint32_t root_size = 0;                  /* number of sectors in root (FAT12/FAT16 only)    */
static uint32_t end_of_file = 0;
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
    fat.bytes_per_sector = READ_16_BITS(boot_info[0x0B],boot_info[0x0C]);
    fat.sectors_per_cluster = boot_info[0x0D];
    fat.size_of_reserved_area = READ_16_BITS(boot_info[0x0E],boot_info[0x0F]);
    fat.numbers_of_fats = boot_info[0x10];
    fat.max_root_entries = READ_16_BITS(boot_info[0x11],boot_info[0x12]);
    fat.total_sectors = READ_16_BITS(boot_info[0x013],boot_info[0x14]);
    fat.fat_size = READ_16_BITS(boot_info[0x016],boot_info[0x17]);

    /* update sector size in HAL.c */
    kmc_update_sector_size(fat.bytes_per_sector);

    fat1_first_index = boot_first_index + 1;
    fat2_first_index = fat1_first_index + fat.fat_size;
    if(fat.max_root_entries != 0) /* FAT12/16 */
    {
        root_first_index = fat2_first_index + fat.fat_size;
        root_size = (32*fat.max_root_entries)/fat.bytes_per_sector;
        data_first_index = root_first_index + root_size;
        root_first_cluster = 0; /* important */
    }
    else /* FAT32 */
    {
        data_first_index = fat2_first_index + fat.fat_size;
        root_first_cluster = READ_32_BITS(boot_info[0x2C],boot_info[0x2D],boot_info[0x2E],boot_info[0x2F]); /* important */
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
        buff_root = (uint8_t*)malloc((sizeof(uint8_t)*root_size*fat.bytes_per_sector));
        if(buff_root == NULL)
        {
            printf("\nunable to allocate memory");
            exit(EXIT_FAILURE);
        }
        total_bytes_read = kmc_read_multi_sector(root_first_index,root_size,buff_root);
    }
    else if (end_of_file == fat_EOF_32)
    {
        buff_FAT = (uint8_t*)malloc(sizeof(uint8_t)*fat.bytes_per_sector*fat.sectors_per_cluster*fat.fat_size);
        if(buff_FAT == NULL)
        {
            printf("\nunable to allocate memory!");
            exit(EXIT_FAILURE);
        }
        kmc_read_multi_sector(fat1_first_index,fat.fat_size,buff_FAT);
        buff_root = (uint8_t*)malloc(sizeof(uint8_t)*fat.bytes_per_sector*fat.sectors_per_cluster*count);
        if(buff_root == NULL)
        {
            printf("\nunable to allocate memory!");
            exit(EXIT_FAILURE);
        }
        current_cluster = root_first_cluster;
        fat_index = current_cluster * 4;
        first_sector = 1 + fat.fat_size * 2 + (current_cluster-2) * fat.sectors_per_cluster;
        total_bytes_read = kmc_read_multi_sector(first_sector,fat.sectors_per_cluster,buff_root);
        next_cluster = READ_32_BITS(buff_FAT[fat_index],buff_FAT[fat_index+1],buff_FAT[fat_index+2],buff_FAT[fat_index+3]);
        while(next_cluster != end_of_file)
        {
            count += 1;
            current_cluster = next_cluster;
            fat_index = current_cluster * 4;
            first_sector = 1 + fat.fat_size * 2 + (current_cluster-2) * fat.sectors_per_cluster;
            buff_root = realloc(buff_root,sizeof(uint8_t)*fat.bytes_per_sector*fat.sectors_per_cluster*count);
            if(buff_root == NULL)
            {
                printf("\nunable to allocate memory!");
                exit(EXIT_FAILURE);
            }
            total_bytes_read += kmc_read_multi_sector(first_sector,fat.sectors_per_cluster,buff_root + fat.bytes_per_sector*fat.sectors_per_cluster);
            next_cluster = READ_32_BITS(buff_FAT[fat_index],buff_FAT[fat_index+1],buff_FAT[fat_index+2],buff_FAT[fat_index+3]);
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
    uint8_t k = 0;
    uint8_t LFN_entries = 0;
    fat_entry* temp = NULL;
    fat_entry* new_entry = NULL;
    fat_free_entries(&entry_head);
    while(i < bytes_count)
    {
        if(buff[i] == 0x00 || buff[i] == 0xE5) /* empty entry or deleted entry */
        {
            /* just check, don't do anything */
        }
        else
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
            strcpy(new_entry->LFN,""); // new_entry->LFN == temp->LFN => you are fucked. If you put inside, you are fucked too.
            if(buff[i + 0x0B] == 0x0F) /* long filename */
            {
                j = i;
                LFN_entries = buff[j] & 0x1F;
                i = i + LFN_entries * 32;
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

            if (buff[i + 0x0B] != 0x0F)
            {
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

    if(current_cluster == root_first_cluster)
    {
        fat_read_root();
        *headTemp = entry_head;
        retValue = 0;
    }
    else
    {
        buff_FAT = (uint8_t*)malloc(sizeof(uint8_t)*fat.bytes_per_sector*fat.sectors_per_cluster*fat.fat_size);
        if(buff_FAT == NULL)
        {
            printf("\nunable to allocate memory!");
            exit(EXIT_FAILURE);
        }
        kmc_read_multi_sector(fat1_first_index,fat.fat_size,buff_FAT);
        buff = (uint8_t*)malloc(sizeof(uint8_t)*fat.bytes_per_sector*fat.sectors_per_cluster*count);
        if(buff == NULL)
        {
            printf("\nunable to allocate memory!");
            exit(EXIT_FAILURE);
        }

        /* read next cluster according to FAT12 or FAT 16 or FAT 32 */
        if(end_of_file == fat_EOF_12)
        {
            fat_index = current_cluster * 1.5;
            first_sector = 1 + fat.fat_size * 2 + root_size + (current_cluster-2) * fat.sectors_per_cluster;
            if((current_cluster % 2) == 0)
            {
                next_cluster = READ_12_BITS_EVEN(buff_FAT[fat_index],buff_FAT[fat_index+1]);
            }
            else if ((current_cluster % 2) != 0)
            {
                next_cluster = READ_12_BITS_ODD(buff_FAT[fat_index],buff_FAT[fat_index+1]);
            }
        }
        else if (end_of_file == fat_EOF_16)
        {
            fat_index = current_cluster * 2;
            first_sector = 1 + fat.fat_size * 2 + root_size + (current_cluster-2) * fat.sectors_per_cluster;
            next_cluster = READ_16_BITS(buff_FAT[fat_index],buff_FAT[fat_index+1]);
        }
        else if (end_of_file == fat_EOF_32)
        {
            fat_index = current_cluster * 4;
            first_sector = 1 + fat.fat_size * 2 + (current_cluster-2) * fat.sectors_per_cluster;
            next_cluster = READ_32_BITS(buff_FAT[fat_index],buff_FAT[fat_index+1],buff_FAT[fat_index+2],buff_FAT[fat_index+3]);
        }
        total_bytes_read = kmc_read_multi_sector(first_sector,fat.sectors_per_cluster,buff);

        while(next_cluster != end_of_file)
        {
            count += 1;
            current_cluster = next_cluster;
            if(end_of_file == fat_EOF_12)
            {
                fat_index = current_cluster * 1.5;
                first_sector = 1 + fat.fat_size * 2 + root_size + (current_cluster-2) * fat.sectors_per_cluster;
                if((current_cluster % 2) == 0)
                {
                    next_cluster = READ_12_BITS_EVEN(buff_FAT[fat_index],buff_FAT[fat_index+1]);
                }
                else if ((current_cluster % 2) != 0)
                {
                    next_cluster = READ_12_BITS_ODD(buff_FAT[fat_index],buff_FAT[fat_index+1]);
                } 
            }
            else if (end_of_file == fat_EOF_16)
            {
                fat_index = current_cluster * 2;
                first_sector = 1 + fat.fat_size * 2 + root_size + (current_cluster-2) * fat.sectors_per_cluster;
                next_cluster = READ_16_BITS(buff_FAT[fat_index],buff_FAT[fat_index+1]);
            }
            else if (end_of_file == fat_EOF_32)
            {
                fat_index = current_cluster * 4;
                first_sector = 1 + fat.fat_size * 2 + (current_cluster-2) * fat.sectors_per_cluster;
                next_cluster = READ_32_BITS(buff_FAT[fat_index],buff_FAT[fat_index+1],buff_FAT[fat_index+2],buff_FAT[fat_index+3]);
            }
            buff = realloc(buff,sizeof(uint8_t)*fat.bytes_per_sector*fat.sectors_per_cluster*count);
            total_bytes_read += kmc_read_multi_sector(first_sector,fat.sectors_per_cluster,buff + fat.bytes_per_sector*fat.sectors_per_cluster);
        }

        if(temp->attribute == 0x10)
        {
            fat_read_entries(buff,total_bytes_read);
            retValue = 1;
        }
        else if (temp->attribute == 0x00)
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

static void fat_free_entries(fat_entry** head_temp)
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
