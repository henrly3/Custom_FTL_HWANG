#include <stdio.h>
#include <stdlib.h>
#include "FTL.h"
#include "config.h"

int main(int argc, char **argv)
{
  for(int i=0;i<argc;i++)
  {
    printf("argv[%d]: %s\n", i, argv[i]);
  }


  printf("Choose Which Test to Run:\n");
  printf("1. Sequential Workload Test\n");
  printf("2. Random Workload Test\n");
  int choice = 0;
  while(choice != 1 && choice != 2) {
      printf("Enter 1 or 2: ");
      scanf("%d", &choice);
      if(choice != 1 && choice != 2) {
          printf("Invalid choice. Please enter 1 or 2.\n");
      }
  }

  printf("\n--------------------------------------\n");
  printf("Choose the number of OverProvisioning (OP) to use (Must be between 0 and 25):\n");
  int op_choice = -1;
  while(op_choice < 0 || op_choice > 25) {
      printf("Enter a number between 0 and 25: ");
      scanf("%d", &op_choice);
      if(op_choice < 0 || op_choice > 25) {
          printf("Invalid choice. Please enter a number between 0 and 25:\n");
      }
  }

  printf("-----------------------------------------------\n");
  printf("Choose how many times to repeat the test (Must be between 1 and 10):\n");
  int repeat_count = -1;
  while(repeat_count < 1 || repeat_count > 10) {
      scanf("%d", &repeat_count);
      if(repeat_count < 1 || repeat_count > 10) {
          printf("Invalid choice. Please enter a number between 1 and 10:\n");
      }
    }

  FTL_init(op_choice); // Initialize FTL with the specified over-provisioning percentage

  int number_of_total_writes = get_Total_Logical_Pages() * repeat_count;

  // 1. Sequential Write Test
  for( uint32_t i=0;i<get_Total_Logical_Pages();i++) // sequential write test
  {
    uint32_t current_LPA = i; // overwrite test
    FTL_STATUS status;
    PhysicalAddress addr;
    status = FTL_WRITE(current_LPA, &addr);
    if (status == FTL_SUCCESS) {
    //    printf(" LPA : %u - Physical Address: Channel %u, Package %u, Die %u, Plane %u, Block %u, Page %u,  ",
          //     current_LPA, addr.channel, addr.package, addr.die, addr.plane, addr.block, addr.page);
    } else {
        printf(" - FTL_WRITE failed with status: %d\n", status);  
      }
  }
  printf("\n--------------------------------------\nSequential Write Test Completed.\n--------------------------------------\n");
  reset_WAF(); // Reset WAF counters after sequential write test


  // 2. Overwrite Test
  for(int i=0;i<number_of_total_writes;i++) // overwrite test
  {
    uint32_t current_LPA_Sequential = i%(get_Total_Logical_Pages()); // overwrite test
    uint32_t current_LPA_Random = rand() % get_Total_Logical_Pages(); // overwrite test
    uint32_t current_LPA = (choice == 1) ? current_LPA_Sequential : current_LPA_Random; // Choose LPA based on user choice
    FTL_STATUS status;
    PhysicalAddress addr;
    status = FTL_WRITE(current_LPA, &addr);
    if (status == FTL_SUCCESS) {
     //   printf(" LPA : %u - Physical Address: Channel %u, Package %u, Die %u, Plane %u, Block %u, Page %u",
     //          current_LPA, addr.channel, addr.package, addr.die, addr.plane, addr.block, addr.page);
    } else {
        printf(" - FTL_WRITE failed with status: %d\n", status);
        return 0;  
      }
  }




  printf("\n--------------------------------------\nOverwrite Test Completed.\n--------------------------------------\n");


  //3. Read Whether GC is triggered 
  for(int i=0;i<number_of_total_writes;i++) // overwrite test
  {
    uint32_t current_LPA = i%(get_Total_Logical_Pages()); // overwrite test
    FTL_STATUS status;
    PhysicalAddress addr;
    status = FTL_READ(current_LPA, &addr);
    if (status == FTL_SUCCESS) {
     //   printf(" LPA : %u - Physical Address: Channel %u, Package %u, Die %u, Plane %u, Block %u, Page %u\n",
          //     current_LPA, addr.channel, addr.package, addr.die, addr.plane, addr.block, addr.page);
    } else {
        printf(" - FTL_READ failed with status: %d\n", status);  
      }
  }
  printf("\n--------------------------------------\nRead Test Completed.\n--------------------------------------\n");
  printf("WAF: %.2f\n", calculate_WAF());
  return 0;

}
