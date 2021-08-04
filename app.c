/*******************************************************************************
* Includes
******************************************************************************/
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include "fat.h"

/*******************************************************************************
* Definitions
******************************************************************************/
#define MAX_LENGTH 50

/*******************************************************************************
* Prototypes
******************************************************************************/

/** @brief This function will clear console screen.
 */
static void clear();


/** @brief This function will print to the screen a list of entries.
 * @param head_temp - start of the linked list.
 */
static void read_dir(fat_entry* head_temp);


/** @brief This function will print to the screen content of a file.
 * @param buff - an array that stores data.
 * @param size - number of bytes in the file.
 */
static void read_file(uint8_t* buff,uint32_t size);

/*******************************************************************************
* Code
******************************************************************************/
void menu(void)
{
    uint8_t file_path[MAX_LENGTH];
    bool condition = true;
    uint8_t* buff = NULL;
    fat_entry* entry_head = NULL;
    fat_entry* temp = NULL;
    uint8_t boot_info[512];
    uint32_t byte_count = 0;
    uint32_t option = 0;
    uint32_t i = 0;
    uint8_t k = 0;
    uint8_t name[9];
    uint8_t extension[4];
    uint32_t size = 0;

    printf("nhap ten file (\"floppy.img\"): ");
    scanf("%49s",file_path);
    if(fat_init(&file_path[0],&entry_head,&boot_info[0]))
    {
        printf("file opened successfully.\n");
        read_dir(entry_head);
    }
    else
    {
        printf("failed to open file!\n");
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
            printf("Root directory\n\n");
            read_dir(entry_head);
        }
        else if (k == FAT_SUB_DIR)
        {
            printf("folder: %s\n\n",name);
            read_dir(entry_head);
        }
        else if (k == FAT_FILE)
        {
            printf("file: %8s.%3s\n\n",name,extension);
            read_file(buff,size);
            condition = false;
        }
        free(buff);
        buff = NULL;

        if(condition == false)
        {
            if(fat_deinit(&file_path[0]))
            {
                free(entry_head);
                entry_head = NULL;
                printf("file closed successfully.");
            }
            else
            {
                printf("failed to close file");
            }
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
static void read_dir(fat_entry* head_temp)
{
    uint16_t index = 0;
    uint16_t day = 0;
    uint16_t month = 0;
    uint16_t year = 0;
    uint16_t hour = 0;
    uint16_t minutes = 0;
    uint32_t size = 0;
    uint16_t modified_time = 0;
    uint16_t modified_date = 0;
    fat_entry* temp = head_temp;

    printf("No      Name                  date & time                 size\n");
    while(temp != NULL)
    {
        modified_time = READ_16_BITS(temp->modified_time[0],temp->modified_time[1]);
        modified_date = READ_16_BITS(temp->modified_date[0],temp->modified_date[1]);
        day = DATE_DAY(modified_date);
        month = DATE_MONTH(modified_date);
        year = DATE_YEAR(modified_date);
        hour = TIME_HOUR(modified_time);
        minutes = TIME_MINUTE(modified_time);
        size = READ_32_BITS(temp->size[0],temp->size[1],temp->size[2],temp->size[3]);
        if(minutes < 10 || hour < 10)
        {
            if(minutes < 10 && hour < 10)
            {
                printf("%hhu   -   %8s.%3s        %hu/%hu/%hu 0%hu:0%hu               %u",\
                index,temp->SFN,temp->extension,day,month,year,hour,minutes,size);
            }
            else if (hour < 10)
            {
                printf("%hhu   -   %8s.%3s        %hu/%hu/%hu 0%hu:%hu               %u",\
                index,temp->SFN,temp->extension,day,month,year,hour,minutes,size);
            }
            else if (minutes < 10)
            {
                printf("%hhu   -   %8s.%3s        %hu/%hu/%hu %hu:0%hu               %u",\
                index,temp->SFN,temp->extension,day,month,year,hour,minutes,size);
            }
            printf("\n");
        }
        else
        {
            printf("%hhu   -   %8s.%3s        %hu/%hu/%hu %hu:%hu                %u",\
            index,temp->SFN,temp->extension,day,month,year,hour,minutes,size);
            printf("\n");
        }
        index+=1;
        temp = temp->next;
    }
}