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
static void read_file(uint8_t* buff,uint32_t byte_count);
static uint16_t modified_date(uint8_t* date,uint8_t option);
static uint16_t modified_time(uint8_t* time,uint8_t option);

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
        printf("select (or input a value out of range to exit): ");
        scanf("%u",&option);
        clear();
        for(i=0;i<option;i++)
        {
            temp = temp->next;
        }
        strcpy(name,temp->SFN);
        strcpy(extension,temp->extension);

        k = fat_read(option,&entry_head,&buff,&byte_count);
        if(k == 0)
        {
            printf("folder: Root\n\n");
            read_dir(entry_head);
            // free(buff);
            // buff = NULL;
        }
        else if (k == 1)
        {
            printf("folder: %s\n\n",name);
            read_dir(entry_head);
            free(buff);
            buff = NULL;
        }
        else if (k == 2)
        {
            printf("file: %8s.%3s\n\n",name,extension);
            read_file(buff,byte_count);
            free(buff);
            buff = NULL;
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

static void read_file(uint8_t* buff,uint32_t byte_count)
{
    uint16_t i = 0;
    for(i = 0;i < byte_count;i++)
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
    uint8_t index = 0;
    uint8_t day = 0;
    uint8_t month = 0;
    uint16_t year = 0;
    uint8_t hour = 0;
    uint8_t minutes = 0;
    uint32_t size = 0;
    fat_entry* temp = headTemp;

    printf("No      Name                  date & time                 size\n");
    while(temp != NULL)
    {
        day = modified_date(temp->modified_date,'D');
        month = modified_date(temp->modified_date,'M');
        year = modified_date(temp->modified_date,'Y');
        hour = (uint8_t)modified_time(temp->modified_time,'h');
        minutes = (uint8_t)modified_time(temp->modified_time,'m');
        size = READ_32_BITS(temp->size[0],temp->size[1],temp->size[2],temp->size[3]);
        if(minutes < 10)
        {
            printf("%hhu   -   %8s.%3s        %hhu/%hhu/%hu %hhu:0%hhu               %u", \
            index,temp->SFN,temp->extension,day,month,year,hour,minutes,size);
            printf("\n");
        }
        else
        {
            printf("%hhu   -   %8s.%3s        %hhu/%hhu/%hu %hhu:%hhu                %u", \
            index,temp->SFN,temp->extension,day,month,year,hour,minutes,size);
            printf("\n");
        }
        index+=1;
        temp = temp->next;
    }
}

static uint16_t modified_time(uint8_t* time,uint8_t option)
{
    uint8_t i = 0;
    uint16_t ret_value = 0;
    uint16_t tempTime = READ_16_BITS(time[0],time[1]);

    if(option == 'm')
    {
        for(i = 5;i <= 10;i++)
        {
            ret_value |= 1<<i;
        }
        ret_value = (ret_value & tempTime) >> 5;
    }
    else if (option == 'h')
    {          
        for(i = 11;i <= 15;i++)
        {
            ret_value |= 1<<i;
        }
        ret_value = (ret_value & tempTime) >> 11;
    }
    return ret_value;
}

static uint16_t modified_date(uint8_t* date,uint8_t option)
{
    uint8_t i = 0;
    uint16_t ret_value = 0;
    uint16_t tempDate = READ_16_BITS(date[0],date[1]);

    if(option == 'D')
    {
        for(i = 0;i <= 4;i++)
        {
            ret_value |= 1<<i;
        }
        ret_value = ret_value & tempDate;
    }
    else if (option == 'M')
    {          
        for(i = 5;i <= 8;i++)
        {
            ret_value |= 1<<i;
        }
        ret_value = (ret_value & tempDate) >> 5;
    }
    else if (option == 'Y')
    {          
        for(i = 9;i <= 15;i++)
        {
            ret_value |= 1<<i;
        }
        ret_value = (ret_value & tempDate) >> 9;
        ret_value = ret_value + 1980;
    }
    return ret_value;
}
