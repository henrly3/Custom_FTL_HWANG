#include "Workload_HOT_COLD.h"
#include "FTL.h"

void workload_hot_cold_80_20(int repeat_count) {
    int total_logical_pages = get_Total_Logical_Pages();
    int hot_pages = (int)(total_logical_pages * 0.8);
    int cold_pages = total_logical_pages - hot_pages;

    for (int r = 0; r < repeat_count; r++) {
        // Write to hot pages
        for (int i = 0; i < hot_pages; i++) {
            uint32_t current_LPA = i;
            FTL_STATUS status;
            PhysicalAddress addr;
            status = FTL_WRITE(current_LPA, &addr);
            if (status != FTL_SUCCESS) {
                printf("Error writing to hot page %d\n", current_LPA);
            }
        }

        // Write to cold pages
        for (int i = hot_pages; i < total_logical_pages; i++) {
            uint32_t current_LPA = i;
            FTL_STATUS status;
            PhysicalAddress addr;
            status = FTL_WRITE(current_LPA, &addr);
            if (status != FTL_SUCCESS) {
                printf("Error writing to cold page %d\n", current_LPA);
            }
        }
    }
}