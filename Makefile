# ==========================================
# Chemins et Fichiers
# ==========================================
SRC_DIR   = src
UTILS_DIR = utils
OBJ_DIR   = o
BIN_DIR   = bin

DISK_IMG  = disk.img
KERNEL_BIN = $(BIN_DIR)/kernel.bin
BOOTLOADER_BIN = boot/bootloader.bin

KERNEL_SYS = $(BIN_DIR)/kernel.sys

# Liste des applications actives
APPS_BINS = $(BIN_DIR)/TEST.BIN $(BIN_DIR)/HELLO.BIN

# ==========================================
# Outils et Flags
# ==========================================
CC = gcc
LD = ld
AS = nasm

CFLAGS  = -ffreestanding -m32 -g -Wall -Wextra -Isrc/kernel -Isrc/fs -Isrc/shell -Isrc/lib
ASFLAGS = -f elf32
LDFLAGS = -T linker.ld -m elf_i386

APP_CFLAGS  = -ffreestanding -m32 -fno-pic -fno-pie -fno-stack-protector -Isrc/lib
APP_LDFLAGS = -T app.ld -m elf_i386

# ==========================================
# Listes des objets Noyau
# ==========================================
# On exclut spécifiquement le bootloader de la liste des objets ELF du noyau
SRCS_C   = $(shell find $(SRC_DIR) -name '*.c' ! -path '$(SRC_DIR)/user/*' ! -name 'ulib.c')
SRCS_ASM = $(shell find $(SRC_DIR) -name '*.asm' ! -path '$(SRC_DIR)/user/*' ! -name 'crt0.asm' ! -name 'bootloader.asm')
OBJS     = $(SRCS_C:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o) \
           $(SRCS_ASM:$(SRC_DIR)/%.asm=$(OBJ_DIR)/%.o)

ULIB_OBJS = $(OBJ_DIR)/lib/crt0.o $(OBJ_DIR)/lib/ulib.o

# ==========================================
# Règles Principales
# ==========================================

.PHONY: all setup clean run go disk apps_user

all: setup $(KERNEL_BIN) apps_user disk

setup:
	@mkdir -p $(OBJ_DIR) $(BIN_DIR) boot

# --- Compilation du Noyau ---
$(KERNEL_BIN): $(OBJS)
	$(eval CLEAN_OBJS = $(sort $(OBJS)))
	$(LD) $(LDFLAGS) -o $@ $(OBJ_DIR)/kernel/boot.o $(filter-out $(OBJ_DIR)/kernel/boot.o, $(CLEAN_OBJS))

# --- Bibliothèque User ---
$(OBJ_DIR)/lib/crt0.o: $(SRC_DIR)/lib/crt0.asm
	@mkdir -p $(dir $@)
	$(AS) -f elf32 $< -o $@

$(OBJ_DIR)/lib/ulib.o: $(SRC_DIR)/lib/ulib.c
	@mkdir -p $(dir $@)
	$(CC) $(APP_CFLAGS) -c $< -o $@

# --- Applications (Décommentées pour fonctionner) ---
apps_user: $(APPS_BINS)

$(BIN_DIR)/HELLO.BIN: $(UTILS_DIR)/hello.asm
	@mkdir -p $(BIN_DIR)
	$(AS) -f bin $< -o $@

$(BIN_DIR)/TEST.BIN: $(UTILS_DIR)/test.c $(ULIB_OBJS)
	@mkdir -p $(OBJ_DIR)/utils $(BIN_DIR)
	$(CC) $(APP_CFLAGS) -c $(UTILS_DIR)/test.c -o $(OBJ_DIR)/utils/test.o
	$(LD) $(APP_LDFLAGS) $(ULIB_OBJS) $(OBJ_DIR)/utils/test.o -o $@

$(BIN_DIR)/TEST2.BIN: $(UTILS_DIR)/test2.c $(ULIB_OBJS)
	@mkdir -p $(OBJ_DIR)/utils $(BIN_DIR)
	$(CC) $(APP_CFLAGS) -c $(UTILS_DIR)/test2.c -o $(OBJ_DIR)/utils/test2.o
	$(LD) $(APP_LDFLAGS) $(ULIB_OBJS) $(OBJ_DIR)/utils/test2.o -o $@


# ==========================================
# CONSTRUCTION DU DISQUE
# ==========================================


# LA RÈGLE CRUCIALE POUR LE BOOTLOADER (Binaire plat)
$(BOOTLOADER_BIN): src/boot/bootloader.asm
	@mkdir -p boot
	$(AS) -f bin $< -o $@

disk: $(DISK_IMG)

$(KERNEL_SYS): $(KERNEL_BIN)
	objcopy -O binary $< $@

# Modifie la règle disk pour copier KERNEL.SYS
# Nouveau paramétrage : 128 Mo
DISK_SIZE_MB = 128

$(DISK_IMG): $(KERNEL_SYS) $(APPS_BINS) boot/bootloader.bin
	@echo "[DISK] Creation de l'image FAT16 ($(DISK_SIZE_MB) Mo)..."
# 1. Création de l'image vide
	dd if=/dev/zero of=$(DISK_IMG) bs=1M count=$(DISK_SIZE_MB)
	
# 2. Injection du bootloader au secteur 0
	dd if=boot/bootloader.bin of=$(DISK_IMG) conv=notrunc
	
# 3. Formatage FAT16 "Hard Drive Style"
# -F : FAT16
# -h 64 -t 32 -s 8 : Géométrie (secteurs/pistes/têtes)
# -R 32 : Secteurs réservés (ton Root LBA ne sera plus 19 !)
# -M 512 : Préserve ton bootloader (secteur 0)
# 	mformat -i $(DISK_IMG) -F -h 64 -t 32 -s 8 -R 32 -M 512 ::
# Ajoute -c 1 (1 secteur par cluster) et force les entrées root à 512
# 	mformat -i $(DISK_IMG) -F -h 64 -t 32 -s 8 -R 32 -r 512 -M 512 ::
# 	mformat -i $(DISK_IMG) -F -N 16 -h 64 -t 32 -s 8 -R 32 -r 512 -M 512 ::
# -c 4 : Force 4 secteurs par cluster (2KB), ce qui permet au FAT16 de couvrir 128MB
# -v GOS : Label pour remplir les champs
	mformat -i $(DISK_IMG) -F -c 4 -r 512 -h 64 -t 32 -s 8 -R 32 -M 512 ::

	# 4. Copie des fichiers
# 	mcopy -i $(DISK_IMG) $(KERNEL_SYS) ::/KERNEL.BIN
# 	@for app in $(APPS_BINS); do \
# 		mcopy -i $(DISK_IMG) $$app ::/`basename $$app`; \
# 	done

	mcopy -i disk.img bin_save/TEST.BIN ::/TEST.BIN
	mcopy -i disk.img bin_save/TEST2.BIN ::/TEST2.BIN
	mcopy -i disk.img config.cnf ::/CONFIG.CNF
	mcopy -i disk.img NOTES.txt ::/NOTES.TXT

	@echo "[DISK] Image $(DISK_IMG) prête au format FAT16."

# $(DISK_IMG): $(KERNEL_SYS) $(APPS_BINS) boot/bootloader.bin
# 	@echo "[DISK] Creation de l'image FAT12..."
# 	dd if=/dev/zero of=$(DISK_IMG) bs=1024 count=1440
# 	dd if=boot/bootloader.bin of=$(DISK_IMG) conv=notrunc
# 	mformat -i $(DISK_IMG) -f 1440 -M 512 ::
# 	mcopy -i $(DISK_IMG) $(KERNEL_SYS) ::/KERNEL.BIN
# 	@for app in $(APPS_BINS); do mcopy -i $(DISK_IMG) $$app ::/`basename $$app`; done

# $(DISK_IMG): $(KERNEL_BIN) $(APPS_BINS) $(BOOTLOADER_BIN)
# 	@echo "[DISK] Creation de l'image bootable FAT12..."
# 	# 1. Créer une image de 1.44 Mo
# 	dd if=/dev/zero of=$(DISK_IMG) bs=1024 count=1440
# 	# 2. Injecter le bootloader au secteur 0
# 	dd if=$(BOOTLOADER_BIN) of=$(DISK_IMG) conv=notrunc
# 	# 3. Formater en FAT12 en préservant le bootloader (-M 512)
# 	mformat -i $(DISK_IMG) -f 1440 -M 512 ::
# 	# 4. Copier les fichiers
# 	mcopy -i $(DISK_IMG) $(KERNEL_BIN) ::/KERNEL.BIN
# 	@for app in $(APPS_BINS); do \
# 		mcopy -i $(DISK_IMG) $$app ::/`basename $$app`; \
# 	done
# 	@echo "[DISK] Image $(DISK_IMG) prête."

# ==========================================
# Lancement et Nettoyage
# ==========================================

# Lancement classique (Multiboot)
run: all
	qemu-system-i386 -kernel $(KERNEL_BIN) \
	                 -drive format=raw,file=$(DISK_IMG),index=0,if=ide \
	                 -m 256M \
	                 -audiodev pa,id=audio0 -machine pcspk-audiodev=audio0

# Lancement Réel (Boot sur la disquette créée)
go: disk
	qemu-system-i386 -drive format=raw,file=$(DISK_IMG),if=ide -m 256M

clean:
	rm -rf $(OBJ_DIR) $(BIN_DIR) boot/bootloader.bin $(DISK_IMG)

# Règles génériques
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.asm
	@mkdir -p $(dir $@)
	$(AS) $(ASFLAGS) $< -o $@