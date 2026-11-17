#ifndef NAND_FLASH_H
#define NAND_FLASH_H

#include <stdint.h>
#include <stdbool.h>
#include "config.h"
#include "FTL.h"

typedef enum {
    NAND_SUCCESS,
    NAND_FAILURE,
    NAND_INVALID_ADDRESS,
    NAND_OUT_OF_SPACE
} NAND_Status;


void NAND_init(void);

NAND_Status NAND_read(PhysicalAddress *addr, uint8_t *data);
NAND_Status NAND_write(PhysicalAddress *addr, const uint8_t *data);
NAND_Status NAND_erase(PhysicalAddress *addr);

#endif