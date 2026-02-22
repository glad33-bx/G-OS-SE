#ifndef ATA_H
#define ATA_H

#include "ata.h"
#include "../kernel/io.h"
#include "../kernel/terminal.h"
#include "../lib/gos_types.h"

void ata_wait_bsy();
void ata_wait_drq();

void ata_identify();
void ata_read_sector(uint32_t lba, uint16_t* buffer);
void ata_write_sector(uint32_t lba, uint16_t* buffer);

#endif