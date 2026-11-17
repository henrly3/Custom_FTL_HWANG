#ifndef CONFIG_H
#define CONFIG_H



//--------------- NAND Flash Configuration ---------------//
#define PAGE_SIZE 4096
#define PAGES_PER_BLOCK 64
#define BLOCK_PER_PLANE 128
#define PLANE_PER_DIE 2
#define DIE_PER_PACKAGE 2
#define PACKAGE_PER_CHANNEL 2
#define CHANNELS_PER_SSD 2


#define Total_Pages (CHANNELS_PER_SSD * PACKAGE_PER_CHANNEL * DIE_PER_PACKAGE * PLANE_PER_DIE * BLOCK_PER_PLANE * PAGES_PER_BLOCK)
#define Total_Blocks (CHANNELS_PER_SSD * PACKAGE_PER_CHANNEL * DIE_PER_PACKAGE * PLANE_PER_DIE * BLOCK_PER_PLANE)

//--------------- SSD Configuration ---------------//
#define SSD_SIZE (Total_Pages * PAGE_SIZE)  // Total size of the SSD in bytes
//#define LOGICAL_PAGES (Total_Pages * 95 / 100)  // Total number of logical pages in the SSD

//--------------- DRAM Configuration ---------------//
#define DRAM_SIZE (4 * 1024 * 1024)  // 4MB


//--------------- FTL Configuration ---------------//
#define GC_THRESHOLD 0.02  // Trigger garbage collection when free space is below 20%

#endif