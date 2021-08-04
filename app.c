/*******************************************************************************
* Includes
******************************************************************************/
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include "fat.h"
#define MAX_LENGTH 50

/*******************************************************************************
* Code
******************************************************************************/
static void clear();
static void read_dir(fat_entry* headTemp);
static void read_file(uint8_t* buff,uint32_t size);

void menu(void)
{
    uint8_t file_path[MAX_LENGTH];
    bool condition = true;
    uint8_t* buff = NULL;
    fat_entry* entry_head = NULL;
    fat_entry* temp = NULL;
    uint32_t byte_count = 0;
    uint32_t option = 0;
    uint32_t i = 0;
    uint8_t k = 0;
    uint8_t name[9];
    uint8_t extension[4];
    uint32_t size = 0;

    printf("nhap ten file (\"floppy.img\"): ");
    scanf("%49s",file_path);
    if(fat_init(&file_path[0],&entry_head))
    {
        read_dir(entry_head);
    }
    else
    {
        printf("failed to open!\n");
        return;
    }
    while(condition)
    {
        temp = entry_head;
        printf("select: ");
        scanf("%u",&option);
        clear();
        for(i=0;i<option;i++)
        {
            temp = temp->next;
        }
        strcpy(name,temp->SFN);
        strcpy(extension,temp->extension);
        size = READ_32_BITS(temp->size[0],temp->size[1],temp->size[2],temp->size[3]);

        k = fat_read(option,&entry_head,&buff);
        if(k == FAT_ROOT)
        {
            printf("folder: Root\n\n");
            read_dir(entry_head);
            free(buff);
            buff = NULL;
        }
        else if (k == FAT_SUB_DIR)
        {
            printf("folder: %s\n\n",name);
            read_dir(entry_head);
            free(buff);
            buff = NULL;
        }
        else if (k == FAT_FILE)
        {
            printf("file: %8s.%3s\n\n",name,extension);
            read_file(buff,size);
            free(buff);
            free(entry_head);
            buff = NULL;
            entry_head = NULL;
            condition = false;
        }
        
    }
}

static void clear(){
    #if defined(__linux__) || defined(__unix__) || defined(__APPLE__)
        system("clear");
    #endif

    #if defined(_WIN32) || defined(_WIN64)
        system("cls");
    #endif
}

static void read_file(uint8_t* buff,uint32_t size)
{
    uint16_t i = 0;
    for(i = 0;i < size;i++)
    {
        if( (i >= 16) && (i % 16 == 0))
        {
            printf("\n");
        }
        printf("%3hhu ",buff[i]);
    }
    printf("\n");
}
static void read_dir(fat_entry* headTemp)
{
    uint16_t index = 0;
    uint16_t day = 0;
    uint16_t month = 0;
    uint16_t year = 0;
    uint16_t hour = 0;
    uint16_t minutes = 0;
    uint32_t size = 0;
    fat_entry* temp = headTemp;

    printf("No      Name                  date & time                 size\n");
    while(temp != NULL)
    {
        day = DATE_DAY(READ_16_BITS(temp->modified_date[0],temp->modified_date[1]));
        month = DATE_MONTH(READ_16_BITS(temp->modified_date[0],temp->modified_date[1]));
        year = DATE_YEAR(READ_16_BITS(temp->modified_date[0],temp->modified_date[1]));
        hour = TIME_HOUR(READ_16_BITS(temp->modified_time[0],temp->modified_time[1]));
        minutes = TIME_MINUTE(READ_16_BITS(temp->modified_time[0],temp->modified_time[1]));
        size = READ_32_BITS(temp->size[0],temp->size[1],temp->size[2],temp->size[3]);
        if(minutes < 10 || hour < 10)
        {
            if(minutes < 10 && hour < 10)
            {
                printf("%hhu   -   %8s.%3s        %hu/%hu/%hu 0%hu:0%hu               %u", \
                index,temp->SFN,temp->extension,day,month,year,hour,minutes,size);
            }
            else if (hour < 10)
            {
                printf("%hhu   -   %8s.%3s        %hu/%hu/%hu 0%hu:%hu               %u", \
                index,temp->SFN,temp->extension,day,month,year,hour,minutes,size);
            }
            else if (minutes < 10)
            {
                printf("%hhu   -   %8s.%3s        %hu/%hu/%hu %hu:0%hu               %u", \
                index,temp->SFN,temp->extension,day,month,year,hour,minutes,size);
            }
            printf("\n");
        }
        else
        {
            printf("%hhu   -   %8s.%3s        %hu/%hu/%hu %hu:%hu                %u", \
            index,temp->SFN,temp->extension,day,month,year,hour,minutes,size);
            printf("\n");
        }
        index+=1;
        temp = temp->next;
    }
}