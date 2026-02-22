[BITS 16]
[ORG 0x7C00]

jmp short start
nop
times 87 db 0          ; BPB étendu FAT32 (plus large que FAT16)

start:
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00

    ; 1. Extraire les paramètres du BPB FAT32
    mov ax, [0x7C00 + 14]       ; Reserved Sectors (ex: 32)
    mov [fat_lba], ax
    
    mov eax, [0x7C00 + 36]      ; SPF (Sectors Per FAT 32) - Ton 1015
    movzx edx, byte [0x7C00 + 16]; Number of FATs (2)
    mul edx                     ; EAX = SPF * 2
    add eax, [fat_lba]          ; EAX = Début de la Data Area (Cluster 2)
    mov [data_lba], eax

    mov eax, [0x7C00 + 44]      ; Root Cluster (Souvent 2)
    mov [curr_cluster], eax

    ; 2. Charger le Root Directory (Cluster 2) à l'adresse 0x0500:0000
    mov bx, 0x0500
    mov es, bx
    xor bx, bx
    call load_cluster_to_es

    ; 3. Chercher "KERNEL  BIN" dans le Root Directory
    mov cx, 16                  ; 16 entrées par secteur (test simple sur 1 sect)
    xor di, di
find_kernel:
    push cx
    mov si, kernel_name
    mov cx, 11
    push di
    repe cmpsb
    pop di
    pop cx
    je found
    add di, 32
    loop find_kernel
    jmp error

found:
    ; Récupérer le cluster de KERNEL.BIN (High word à 20, Low word à 26)
    mov ax, [es:di + 20]        ; Cluster High
    shl eax, 16
    mov ax, [es:di + 26]        ; Cluster Low
    mov [curr_cluster], eax

    ; 4. Charger le Kernel à 0x1000:0000
    mov ax, 0x1000
    mov es, ax
    xor bx, bx

load_loop:
    call load_cluster_to_es
    
    ; Avancer le segment ES pour le prochain cluster
    mov ax, es
    movzx dx, byte [0x7C00 + 13] ; Sectors per cluster
    shl dx, 5                    ; DX * 512 / 16 (conversion en segment)
    add ax, dx
    mov es, ax

    ; Trouver le prochain cluster dans la FAT32
    call get_next_cluster
    mov [curr_cluster], eax
    cmp eax, 0x0FFFFFF8          ; Fin de chaîne FAT32
    jb load_loop

    ; 5. Lancement du noyau
    jmp 0x1000:0000

; --- Routines ---

load_cluster_to_es:
    ; LBA = data_lba + (cluster - 2) * sectors_per_cluster
    mov eax, [curr_cluster]
    sub eax, 2
    movzx edx, byte [0x7C00 + 13]
    mul edx
    add eax, [data_lba]
    
    movzx cx, byte [0x7C00 + 13] ; Nombre de secteurs à lire
    xor bx, bx                   ; Offset ES:0000
.read_loop:
    push cx
    call read_sector_lba
    add eax, 1                   ; Secteur suivant
    add bx, 512                  ; Buffer suivant (Si BX déborde, il faut incrémenter ES)
    pop cx
    loop .read_loop
    ret

read_sector_lba:                 ; Lit 1 secteur EAX vers ES:BX
    pushad
    ; Conversion LBA (EAX) en CHS (simplifié pour BIOS)
    xor edx, edx
    movzx ecx, word [0x7C00 + 24] ; Sectors per track
    div ecx
    inc dl                        ; DL = Sector (1-based)
    mov cl, dl
    
    xor edx, edx
    movzx ecx, word [0x7C00 + 26] ; Heads
    div ecx
    mov dh, dl                    ; DH = Head
    mov ch, al                    ; CH = Cylinder (low 8 bits)
    
    mov dl, [0x7C00 + 40]         ; Drive number
    mov ax, 0x0201                ; Read 1 sector
    int 0x13
    jc error
    popad
    ret

get_next_cluster:
    ; Entrée : [curr_cluster], Sortie : EAX = prochain cluster
    ; Calcul du secteur FAT = fat_lba + (cluster * 4 / 512)
    mov eax, [curr_cluster]
    shl eax, 2                    ; Cluster * 4 octets
    xor edx, edx
    mov ecx, 512
    div ecx                       ; EAX = Secteur relatif, EDX = Offset
    add eax, [fat_lba]
    
    push dx
    mov bx, 0x0700                ; Buffer temporaire pour la FAT
    mov es, bx
    xor bx, bx
    call read_sector_lba
    pop dx
    
    mov si, dx
    mov eax, [es:si]
    and eax, 0x0FFFFFFF           ; Masque FAT32 (28 bits)
    ret

error:
    mov si, err_msg
    mov ah, 0x0E
.loop:
    lodsb
    or al, al
    jz .halt
    int 0x10
    jmp .loop
.halt:
    jmp $

kernel_name db "KERNEL  BIN"
err_msg     db "Boot Error", 0

fat_lba      dd 0
data_lba     dd 0
curr_cluster dd 0

times 510-($-$$) db 0
dw 0xAA55