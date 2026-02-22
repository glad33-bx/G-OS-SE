#include "../fs/ata.h"
#include "../kernel/io.h"
#include "../kernel/terminal.h"
#include "../lib/gos_types.h"

// Registres du bus primaire
#define ATA_DATA        0x1F0
#define ATA_FEATURES    0x1F1
#define ATA_SECTOR_CNT  0x1F2
#define ATA_LBA_LOW     0x1F3
#define ATA_LBA_MID     0x1F4
#define ATA_LBA_HIGH    0x1F5
#define ATA_DRIVE_SEL   0x1F6
#define ATA_COMMAND     0x1F7
#define ATA_STATUS      0x1F7

// Commandes ATA
#define ATA_CMD_READ    0x20
#define ATA_CMD_WRITE   0x30
#define ATA_CMD_IDENTIFY 0xEC

// Bits du registre Status
#define ATA_SR_BSY     0x80    // Busy
#define ATA_SR_DRQ     0x08    // Data Request
#define ATA_SR_ERR     0x01    // Error

// Ajoute ce flag pour vérifier les erreurs
#define ATA_SR_DF      0x20    // Drive Fault

// délai de courtoisie (400ns)
static void ata_delay() {
    inb(ATA_STATUS);
    inb(ATA_STATUS);
    inb(ATA_STATUS);
    inb(ATA_STATUS);
}

void ata_wait_ready() {
    uint8_t status;
    do {
        status = inb(ATA_STATUS);
    } while ((status & ATA_SR_BSY) || !(status & ATA_SR_DRQ));
}

/**
 * Attend que le disque soit prêt (Polling)
 */
void ata_wait_bsy() {
    uint32_t timeout = 1000000;
    while ((inb(0x1F7) & 0x80) && timeout > 0) {
        timeout--;
    }
    if (timeout == 0) kprintf("ERREUR: Timeout BSY !\n");
}

void ata_wait_drq() {
    while (!(inb(ATA_STATUS) & ATA_SR_DRQ));
}

/**
 * Lit un secteur de 512 octets (LBA 28 bits)
 */
/*void ata_read_sector(uint32_t lba, uint16_t* buffer) {
    // Désactiver les interruptions au niveau du contrôleur ATA (Control Register)
    // 0x3F6 est le port de contrôle pour le bus primaire
    // On envoie 0x02 pour mettre le bit nIEN (No Interrupt)
    outb(0x3F6, 0x02);

    #if DEBUG
        kprintf("[ATA] Lecture LBA %d...\n", lba);
    #endif

    outb(ATA_DRIVE_SEL, 0xE0 | ((lba >> 24) & 0x0F));
    outb(ATA_SECTOR_CNT, 0x01);
    outb(ATA_LBA_LOW,  (uint8_t)lba);
    outb(ATA_LBA_MID,  (uint8_t)(lba >> 8));
    outb(ATA_LBA_HIGH, (uint8_t)(lba >> 16));
    
    #if DEBUG
        kprintf("[ATA] Commande READ envoyee...");
    #endif
    outb(ATA_COMMAND, ATA_CMD_READ);

    ata_wait_bsy();
    #if DEBUG
        kprintf(" BSY fini...");
    #endif

    ata_wait_drq();
    
    #if DEBUG
        kprintf(" DRQ ok. Transfert...");
    #endif
    for (int i = 0; i < 256; i++) {
        buffer[i] = inw(ATA_DATA);
    }

    inb(ATA_STATUS); 

    #if DEBUG
        kprintf(" OK!\n");
    #endif
}*/

/*
// KO
void ata_write_sector(uint32_t lba, uint16_t* buffer) {
    ata_wait_bsy();
    
    outb(0x1F6, (uint8_t)((lba >> 24) & 0x0F) | 0xE0);
    outb(0x1F2, 1); // Un seul secteur
    outb(0x1F3, (uint8_t)lba);
    outb(0x1F4, (uint8_t)(lba >> 8));
    outb(0x1F5, (uint8_t)(lba >> 16));
    outb(0x1F7, 0x30); // Commande WRITE SECTORS

    // On attend que le disque soit prêt à recevoir les données
    ata_wait_bsy();
    while (!(inb(0x1F7) & 0x08)); // On attend DRQ (Data Request)

    // On envoie les 256 mots (512 octets)
    for (int i = 0; i < 256; i++) {
        outw(0x1F0, buffer[i]);
    }

    // On force le disque à vider son cache vers le support physique
    outb(0x1F7, 0xE7); // Commande CACHE FLUSH
    ata_wait_bsy();
}*/

void ata_read_sector(uint32_t lba, uint16_t* buffer) {
    // 1. Désactiver les interruptions ATA (Essentiel pour ton setup)
    outb(0x3F6, 0x02);

    // 2. Sélection du drive et LBA
    // On attend que le disque ne soit pas occupé AVANT de lui parler
    while (inb(ATA_STATUS) & 0x80); 

    outb(ATA_DRIVE_SEL, 0xE0 | ((lba >> 24) & 0x0F));
    
    // Un petit délai ici laisse le temps au select de se propager
    inb(ATA_STATUS); inb(ATA_STATUS); inb(ATA_STATUS); inb(ATA_STATUS);

    outb(ATA_SECTOR_CNT, 0x01);
    outb(ATA_LBA_LOW,  (uint8_t)lba);
    outb(ATA_LBA_MID,  (uint8_t)(lba >> 8));
    outb(ATA_LBA_HIGH, (uint8_t)(lba >> 16));
    
    // 3. Envoyer la commande READ
    outb(ATA_COMMAND, ATA_CMD_READ);

    // 4. L'attente en deux étapes (la plus robuste)
    // Étape A : Attendre que Busy disparaisse
    while (inb(ATA_STATUS) & 0x80);
        
    // ... select drive ...
    outb(ATA_COMMAND, ATA_CMD_READ);
    while (inb(ATA_STATUS) & 0x80); // Attente BSY

    // Étape B : Attendre que Data Request (DRQ) apparaisse
    // On ajoute un timeout de sécurité pour éviter le reboot/freeze infini
    uint32_t timeout = 100000;
    while (!(inb(ATA_STATUS) & 0x08) && timeout > 0) {
        timeout--;
    }

    if (timeout == 0) {
        kprintf("[ATA] Erreur: Le disque ne repond pas (DRQ timeout)\n");
        return;
    }

    // 5. Transfert des données
    for (int i = 0; i < 256; i++) {
        buffer[i] = inw(ATA_DATA);
    }

    // 6. Petit délai final pour vider le registre de statut
    inb(ATA_STATUS); 
}
/*
void ata_write_sector(uint32_t lba, uint16_t* buffer) {
    outb(ATA_DRIVE_SEL, 0xE0 | ((lba >> 24) & 0x0F));
    ata_delay();

    outb(ATA_SECTOR_CNT, 1);
    outb(ATA_LBA_LOW, (uint8_t)lba);
    outb(ATA_LBA_MID, (uint8_t)(lba >> 8));
    outb(ATA_LBA_HIGH, (uint8_t)(lba >> 16));
    outb(ATA_COMMAND, ATA_CMD_WRITE);

    ata_wait_ready();

    for (int i = 0; i < 256; i++) {
        outw(ATA_DATA, buffer[i]);
        // Petit délai entre les écritures pour certains contrôleurs lents
        io_wait(); 
    }

    // Flush le cache pour garantir l'écriture physique sur disk.img
    outb(ATA_COMMAND, 0xE7); // CACHE FLUSH
    ata_wait_bsy();
}*/

void ata_write_sector(uint32_t lba, uint16_t* buffer) {
    outb(ATA_DRIVE_SEL, 0xE0 | ((lba >> 24) & 0x0F));
    ata_delay();

    outb(ATA_SECTOR_CNT, 1);
    outb(ATA_LBA_LOW, (uint8_t)lba);
    outb(ATA_LBA_MID, (uint8_t)(lba >> 8));
    outb(ATA_LBA_HIGH, (uint8_t)(lba >> 16));
    outb(ATA_COMMAND, ATA_CMD_WRITE);

    ata_wait_ready();

    for (int i = 0; i < 256; i++) {
        outw(ATA_DATA, buffer[i]);
        // Petit délai entre les écritures pour certains contrôleurs lents
        io_wait(); 
    }

    // Flush le cache pour garantir l'écriture physique sur disk.img
    outb(ATA_COMMAND, 0xE7); // CACHE FLUSH
    ata_wait_bsy();
}

/**
 * Identifie le disque présent
 */

void ata_identify() {
    outb(ATA_DRIVE_SEL, 0xA0);

    // On met les registres à 0 comme recommandé
    outb(ATA_SECTOR_CNT, 0);
    outb(ATA_LBA_LOW, 0);
    outb(ATA_LBA_MID, 0);
    outb(ATA_LBA_HIGH, 0);

    outb(ATA_COMMAND, ATA_CMD_IDENTIFY);

    // Un léger délai est nécessaire pour que le contrôleur mette à jour son statut
    ata_delay(); // <--- TRÈS IMPORTANT

    uint8_t status = inb(ATA_STATUS);
    if (status == 0) {
        kprintf("ATA: Aucun disque trouve.\n");
        return;
    }
    if (status == 0) return; // Pas de disque

    ata_wait_bsy();
    ata_wait_drq();

    uint16_t data[256];
    for (int i = 0; i < 256; i++) {
        data[i] = inw(ATA_DATA);
    }
    // On utilise une donnée pour supprimer le warning
    // Le mot 10-19 contient le numéro de série (en ASCII)
    kprintf("ATA: Disque detecte. Status: %x\n", data[0]); 
}






