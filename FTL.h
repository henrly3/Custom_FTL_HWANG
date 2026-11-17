#ifndef FTL_H
#define FTL_H

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include "config.h"


typedef struct {
    uint32_t channel;
    uint32_t package;
    uint32_t die;
    uint32_t plane;
    uint32_t block;
    uint32_t page;  // 내부에 logical page 번호를 포함할 수 있음
} PhysicalAddress;

typedef enum 
{
    FTL_SUCCESS,
    FTL_FAILURE,
    FTL_INVALID_ADDRESS,
    FTL_OUT_OF_SPACE, 
    FTL_GC_FAILED
} FTL_STATUS;


//FTL mappings
void FTL_init(uint32_t over_provisioning_percentage);


//Garbage Collection
bool Garbage_Collection_GREEDY(void);
bool Garbage_Collection_FIFO(void);
void Garbage_Collection_WEAR_LEVELING(void);
bool Garbage_Collection_CHECK(void);
uint32_t find_victim_block(void);
uint32_t find_victim_block_FIFO(void);

//Used block management_FIFO
void USED_BLOCK_FIFO_INIT(void);
void USED_BLOCK_FIFO_IN(uint32_t Block_Number);
uint32_t USED_BLOCK_FIFO_OUT(void);

//WAF 
void reset_WAF(void);
double calculate_WAF(void);

//Command Execution
PhysicalAddress NAND_Decode(uint32_t physical_page_number);
FTL_STATUS FTL_READ(uint32_t LPA, PhysicalAddress *addr);
FTL_STATUS FTL_WRITE(uint32_t LPA, PhysicalAddress *addr);

//Get # of Logical_Pages
uint32_t get_Total_Logical_Pages(void);



#endif