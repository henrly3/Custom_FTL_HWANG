#include "FTL.h"

#define INVALID_ADDR 0xFFFFFFFF

typedef struct {
    int32_t block;
    uint32_t page;
} WritePointer;


typedef struct {
    uint32_t valid_page_cnt;
    uint32_t erase_cnt;
    bool is_free;
} BlockInfo;

static BlockInfo blocks[Total_Blocks];

//USED Block management
static uint32_t *used_block_FIFO = NULL;
static uint32_t used_block_head = 0;
static uint32_t used_block_tail = 0;


static uint32_t L2PMAP[Total_Pages];
static uint32_t P2LMAP[Total_Pages];  

static WritePointer WritePointer_HOST;
static WritePointer WritePointer_GC;



static uint32_t free_block_cnt;
static uint32_t Write_Count_HOST;
static uint32_t Write_Count_REAL;

static uint32_t Total_Logical_Pages;

void FTL_init(uint32_t over_provisioning_percentage)
{
    Total_Logical_Pages = Total_Pages * (100 - over_provisioning_percentage) / 100;
    for(uint32_t i=0;i<Total_Logical_Pages;i++)
    {
        L2PMAP[i] = INVALID_ADDR;
    }

    for(uint32_t i=0;i<Total_Pages;i++)
    {
        P2LMAP[i] = INVALID_ADDR;
    }

    //All blocks are free at the beginning 
    for(uint32_t i=0;i<Total_Blocks;i++)
    {
        blocks[i].valid_page_cnt = 0;
        blocks[i].erase_cnt = 0;
        blocks[i].is_free = true;
    }
    
    //Write Pointer initialization
    WritePointer_HOST.block = -1;
    WritePointer_GC.block = -1;

    WritePointer_HOST.page = 0;
    WritePointer_GC.page = 0;

    free_block_cnt = Total_Blocks;

    Write_Count_HOST = 0;
    Write_Count_REAL = 0;

    //USED Block_FIFO initilaization
    USED_BLOCK_FIFO_INIT();

    printf("FTL initialized successfully.\n");
    printf("Total Pages: %u\n", Total_Pages);
    printf("Total Blocks: %u\n", Total_Blocks);
    printf("SSD Size: %u bytes\n", SSD_SIZE);
}



PhysicalAddress NAND_Decode(uint32_t physical_page_number)
{
    PhysicalAddress addr;
    addr.page = physical_page_number % PAGES_PER_BLOCK;
    physical_page_number /= PAGES_PER_BLOCK;

    addr.block = physical_page_number % BLOCK_PER_PLANE;
    physical_page_number /= BLOCK_PER_PLANE;

    addr.plane = physical_page_number % PLANE_PER_DIE;
    physical_page_number /= PLANE_PER_DIE;

    addr.die = physical_page_number % DIE_PER_PACKAGE;
    physical_page_number /= DIE_PER_PACKAGE;

    addr.package = physical_page_number % PACKAGE_PER_CHANNEL;
    physical_page_number /= PACKAGE_PER_CHANNEL;

    addr.channel = physical_page_number;  // Remaining value is the channel

    return addr;
}


uint32_t find_victim_block(void)
{
    uint32_t victim_block = 0;
    uint32_t min_valid_pages = PAGES_PER_BLOCK;
    bool is_find = false;

    for(int i=0;i<Total_Blocks;i++)
    {
        if(blocks[i].valid_page_cnt < min_valid_pages && !blocks[i].is_free && i!= WritePointer_HOST.block && i!=WritePointer_GC.block)
        {
            min_valid_pages = blocks[i].valid_page_cnt;
            victim_block = i;
            is_find = true;
        }

    }
    if(!is_find)
    {
        return INVALID_ADDR;  // Indicate that no victim block was found
    }
    return victim_block;
}



FTL_STATUS FTL_READ(uint32_t LPA, PhysicalAddress *addr)
{
    if (LPA >= Total_Logical_Pages) {
        return FTL_INVALID_ADDRESS;
    }

    uint32_t physical_page_number = L2PMAP[LPA];
    if (physical_page_number == INVALID_ADDR) {
        return FTL_FAILURE;  // No valid mapping for this LPA
    }

    *addr = NAND_Decode(physical_page_number);
    return FTL_SUCCESS;
}


FTL_STATUS FTL_WRITE(uint32_t LPA, PhysicalAddress *addr)
{

    if (LPA >= Total_Logical_Pages) {
        return FTL_INVALID_ADDRESS;
    }

    // 1. Move to the next block
    if(WritePointer_HOST.block <0 || WritePointer_HOST.page >= PAGES_PER_BLOCK )
    {
        while (Garbage_Collection_CHECK() == true) {
            bool success = Garbage_Collection_GREEDY();   // GC 한 번 하고 결과 저장
            if (success == false) {   // 실패했으면
            break;                // 그만두기
            }
        }
    
        WritePointer_HOST.block = -1;
            //Find a free block
            for(int i=0;i<Total_Blocks;i++)
            {
                if(blocks[i].is_free)
                {
                    WritePointer_HOST.block = i;
                    WritePointer_HOST.page = 0;
                    blocks[i].is_free = false;
                    free_block_cnt--;
                    break;
                }
            }
    }
    if(WritePointer_HOST.block < 0) {
        return FTL_OUT_OF_SPACE;  // No free blocks available
    }

    //2. Write the data to NAND (simulated here)
    uint32_t old_ppa = L2PMAP[LPA];
    if(old_ppa != INVALID_ADDR) { // already written before, need to invalidate the old page
        blocks[old_ppa / PAGES_PER_BLOCK].valid_page_cnt--;
        P2LMAP[old_ppa] = INVALID_ADDR;  // Invalidate the old physical page
    } 
    uint32_t new_ppa = WritePointer_HOST.block * PAGES_PER_BLOCK + WritePointer_HOST.page;
    L2PMAP[LPA] = new_ppa;
    P2LMAP[new_ppa] = LPA;  // Update the reverse mapping
    blocks[WritePointer_HOST.block].valid_page_cnt++;
    WritePointer_HOST.page++;
    *addr = NAND_Decode(new_ppa);

    //if target block's page is full -->  get a block is a used block FIFO QUEUE
    if (WritePointer_HOST.page == PAGES_PER_BLOCK) {      // 이 쓰기로 블록이 가득 참
    USED_BLOCK_FIFO_IN((uint32_t)WritePointer_HOST.block);
    }

    Write_Count_HOST++;
    Write_Count_REAL++;
    //printf the number of free blocks after the write operation
   // printf(", Free Blocks: %u\n", free_block_cnt);
    return FTL_SUCCESS;
}

bool Garbage_Collection_CHECK(void)
{
    float free_space_ratio = (float)free_block_cnt / Total_Blocks;
    return free_space_ratio < GC_THRESHOLD;
}




bool Garbage_Collection_GREEDY(void)
{
    
    //1. Find the victim block
    uint32_t VICTIM_BLOCK = find_victim_block();
    if(VICTIM_BLOCK == INVALID_ADDR) {
        printf(" | Garbage Collection failed: No victim block found. |  ");
        return false;  // No victim block found
    }

    //3. Move the valid pages from the victim block to the new block
    for(int i=0;i<PAGES_PER_BLOCK;i++)
    {
        // move valid pages to the new block
        uint32_t old_ppn = VICTIM_BLOCK * PAGES_PER_BLOCK + i;
        if(P2LMAP[old_ppn] != INVALID_ADDR) // if victim block has valid page
        {
            // 1. Check if the write pointer for GC is valid
            if(WritePointer_GC.block < 0 || WritePointer_GC.page >= PAGES_PER_BLOCK)
            {
                //Find a free block for GC
                WritePointer_GC.block = -1;
                for(int j=0;j<Total_Blocks;j++)
                {
                    if(blocks[j].is_free)
                    {
                        WritePointer_GC.block = j;
                        WritePointer_GC.page = 0;
                        blocks[j].is_free = false;
                        free_block_cnt--;
                        break;
                    }
                }
                if(WritePointer_GC.block < 0) {
                    printf(" | Garbage Collection failed: No free block available for GC.\n");
                    return false;  // No free blocks available for GC
                }
            }

            // 2. Move the valid page to the new block
            uint32_t LPA = P2LMAP[old_ppn];
            uint32_t new_ppn = WritePointer_GC.block * PAGES_PER_BLOCK + WritePointer_GC.page;
            L2PMAP[LPA] = new_ppn;
            P2LMAP[new_ppn] = LPA;  // Update the reverse mapping
            blocks[WritePointer_GC.block].valid_page_cnt++;
            blocks[VICTIM_BLOCK].valid_page_cnt--;
            WritePointer_GC.page++;
            Write_Count_REAL++;
        }
        P2LMAP[old_ppn] = INVALID_ADDR;  // Invalidate the old physical pag
    }

    //4. Erase the victim block
    blocks[VICTIM_BLOCK].valid_page_cnt = 0;
    blocks[VICTIM_BLOCK].erase_cnt++;
    blocks[VICTIM_BLOCK].is_free = true;
    free_block_cnt++;

    //5. print the result of garbage collection
    printf(" | Garbage Collection completed. Victim Block: %u, New Block: %d | \n", VICTIM_BLOCK, WritePointer_GC.block);
    return true;
}


double calculate_WAF(void)
{
    printf(" | Write Count (Host): %u, Write Count (Real): %u | ", Write_Count_HOST, Write_Count_REAL);
    if (Write_Count_HOST == 0) {
        return 0.0;  // Avoid division by zero
    }
    return (double)Write_Count_REAL / Write_Count_HOST;  
}

void reset_WAF(void)
{
    Write_Count_HOST = 0;
    Write_Count_REAL = 0;
}


uint32_t get_Total_Logical_Pages(void)
{
    return Total_Logical_Pages;
}


void USED_BLOCK_FIFO_INIT(void)
{
    free(used_block_FIFO);   //ERASE
    used_block_FIFO = (uint32_t *)malloc(sizeof(uint32_t)*Total_Blocks);
    used_block_head = 0;
    used_block_tail = 0;
    for(uint32_t i=0; i<Total_Blocks;i++) 
    {
        used_block_FIFO[i] = INVALID_ADDR;
    }
}

void USED_BLOCK_FIFO_IN(uint32_t Block_Number)
{
    used_block_FIFO[used_block_tail] = Block_Number;
    used_block_tail = (used_block_tail+1)%Total_Blocks;
}

uint32_t USED_BLOCK_FIFO_OUT(void)
{
    uint32_t result = used_block_FIFO[used_block_head];
    used_block_head = (used_block_head+1)%Total_Blocks;
    return result;
}


bool Garbage_Collection_FIFO()
{
    
    //1. Find the victim block
    uint32_t VICTIM_BLOCK = USED_BLOCK_FIFO_OUT();

    //3. Move the valid pages from the victim block to the new block
    for(int i=0;i<PAGES_PER_BLOCK;i++)
    {
        // move valid pages to the new block
        uint32_t old_ppn = VICTIM_BLOCK * PAGES_PER_BLOCK + i;
        if(P2LMAP[old_ppn] != INVALID_ADDR) // if victim block has valid page
        {
            // 1. Check if the write pointer for GC is valid
            if(WritePointer_GC.block < 0 || WritePointer_GC.page >= PAGES_PER_BLOCK)
            {
                //Find a free block for GC
                WritePointer_GC.block = -1;
                for(int j=0;j<Total_Blocks;j++)
                {
                    if(blocks[j].is_free)
                    {
                        WritePointer_GC.block = j;
                        WritePointer_GC.page = 0;
                        blocks[j].is_free = false;
                        free_block_cnt--;
                        break;
                    }
                }
                if(WritePointer_GC.block < 0) {
                    printf(" | Garbage Collection failed: No free block available for GC.\n");
                    return false;  // No free blocks available for GC
                }
            }

            // 2. Move the valid page to the new block
            uint32_t LPA = P2LMAP[old_ppn];
            uint32_t new_ppn = WritePointer_GC.block * PAGES_PER_BLOCK + WritePointer_GC.page;
            L2PMAP[LPA] = new_ppn;
            P2LMAP[new_ppn] = LPA;  // Update the reverse mapping
            blocks[WritePointer_GC.block].valid_page_cnt++;
            WritePointer_GC.page++;
            Write_Count_REAL++;

            if(WritePointer_GC.page >= PAGES_PER_BLOCK) USED_BLOCK_FIFO_IN(WritePointer_GC.block);
        }
        P2LMAP[old_ppn] = INVALID_ADDR;  // Invalidate the old physical pag
    }

    //4. Erase the victim block
    blocks[VICTIM_BLOCK].valid_page_cnt = 0;
    blocks[VICTIM_BLOCK].erase_cnt++;
    blocks[VICTIM_BLOCK].is_free = true;
    free_block_cnt++;

    //5. print the result of garbage collection
    printf(" | Garbage Collection completed. Victim Block: %u, New Block: %d | \n", VICTIM_BLOCK, WritePointer_GC.block);
    return true;
}