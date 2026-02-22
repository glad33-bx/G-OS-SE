#include "idt.h"
#include "syscalls.h"
/*
IDT (Interrupt Descriptor Table)
ous avons écrit le code du gestionnaire de clavier, mais pour l'instant, 
le processeur ne sait pas qu'il doit l'utiliser. 
C'est comme si nous avions installé un téléphone mais que nous n'avions pas branché la ligne.

Pour que le clavier réagisse, il nous manque l'IDT (Interrupt Descriptor Table). 
Sans elle, quand tu appuies sur une touche, le matériel envoie un signal (IRQ), 
mais le processeur ne sait pas où aller et ignore l'événement (ou plante).
*/

/*
L'IDT est le tableau de bord des événements.

sti (Set Interrupt Flag) est la commande assembleur qui dit au processeur : 
"Ok, j'accepte d'être interrompu maintenant".

Quand tu tapes une touche, le PIC (contrôleur d'interruption) envoie le signal 0x21, 
le processeur regarde dans l'IDT, saute vers irq1_wrapper, qui appelle ton code C.
*/

// Définition de la table et du pointeur pour le processeur
struct idt_entry idt[256];
struct idt_ptr idtp;

// On déclare les wrappers assembleur définis dans interrupt.asm
extern void irq1_wrapper();
extern void irq0_wrapper();
extern void syscall_wrapper();

/**
 * Configure une entrée (gate) dans l'IDT
 */
void idt_set_gate(uint8_t num, uint32_t base, uint16_t sel, uint8_t flags) {
    idt[num].base_low = (base & 0xFFFF);
    idt[num].base_high = (base >> 16) & 0xFFFF;
    idt[num].sel = sel;// 0x08 est le sélecteur de segment de code standard (GDT)
    idt[num].always0 = 0;
    /* Flags: 0x8E (10001110 en binaire)
       P (1) : Présent
       DPL (00) : Ring 0 (Privilège kernel)
       S (0) : Système
       Type (1110) : 32-bit Interrupt Gate
    */
    idt[num].flags = flags;
}

/**
 * Reprogramme le PIC pour décaler les IRQ
 */
/*
Pour que le clavier fonctionne réellement, il faut configurer le PIC (Programmable Interrupt Controller). 
Par défaut, le PIC envoie les interruptions du clavier sur un canal qui entre en conflit avec les exceptions du processeur.

Si tu tapes au clavier et que QEMU redémarre (ou ne fait rien), 
il faudra ajouter une petite fonction PIC_remap() dans ton fichier idt.c 
pour déplacer les interruptions matérielles vers la plage 0x20 - 0x2F.
-----------------------
Sans le remap du PIC, si tu appuies sur une touche, le processeur croit recevoir 
une exception matérielle interne (une erreur grave) au lieu d'une touche clavier. 
C'est parce que par défaut, les IRQ (matériel) et les Exceptions (CPU) utilisent les mêmes numéros d'interruption.
*/

void PIC_remap() {
    // Initialisation (ICW1) en mode cascade
    outb(PIC1_COMMAND, 0x11);                   
    outb(PIC2_COMMAND, 0x11);

    // Vecteurs de base (ICW2) - On décale les IRQ à 0x20 (32)
    // On décale les IRQ : le Maître commence à 0x20, l'Esclave à 0x28
    outb(PIC1_DATA, 0x20); // Maître : 0x20
    outb(PIC2_DATA, 0x28); // Esclave : 0x28

    // Chaînage Maître/Esclave (ICW3)
    // Configuration de la liaison entre les deux PICs
    outb(PIC1_DATA, 0x04);
    outb(PIC2_DATA, 0x02);

    // Mode 8086 (ICW4)
    outb(PIC1_DATA, 0x01);
    outb(PIC2_DATA, 0x01);

    // Masquage : On commence par tout bloquer (0xFF)
    //outb(PIC1_DATA, 0xFF);
    //outb(PIC2_DATA, 0xFF);

    outb(PIC1_DATA, 0x0);  // Démasquer tout
    outb(PIC2_DATA, 0x0);
}

/**
 * Initialise l'IDT complète
 * Il faut appeler PIC_remap() avant d'activer les interruptions avec sti.
 */
void init_idt() {
    // 1. Préparer le pointeur de l'IDT
    idtp.limit = (sizeof(struct idt_entry) * 256) - 1;
    idtp.base = (unsigned int)&idt;

    // 2. Nettoyer l'IDT (remplir de zéros par sécurité)
    // Initialisation totale : La boucle for garantit qu'aucune interruption sauvage 
    // ne fera sauter le processeur sur une adresse mémoire corrompue.
    for(int i = 0; i < 256; i++) {
        idt_set_gate(i, 0, 0, 0);
        // On force les entrées vides à "non-présentes"
        idt[i].flags = 0x00; 
    }

    // 3. Reprogrammer le PIC
    //Masquage sélectif : On bloque tout au début du PIC_remap 
    // et on n'ouvre que le clavier (0xFD) à la toute fin.
    PIC_remap();

    // 4. Enregistrer notre gestionnaire de clavier (IRQ 1 -> 0x21)
    // On va dire au processeur que l'interruption 0x21 (clavier) doit appeler notre irq1_wrapper en assembleur.
    // le masque du PIC pour autoriser IRQ 0 et IRQ 1 : outb(0x21, 0xFC); (FC = 11111100 en binaire, active les bits 0 et 1).
    // 2. Pour le Timer (IRQ 0 -> Interruption 0x20)
    idt_set_gate(0x20, (uint32_t)irq0_wrapper, 0x08, 0x8E);
    // 3. Pour le Clavier (IRQ 1 -> Interruption 0x21)
    idt_set_gate(0x21, (uint32_t)irq1_wrapper, 0x08, 0x8E);
    /*
    Pourquoi ces valeurs ?
    Argument	Valeur	Signification
    num	0x21	Le numéro de l'entrée dans la table (33 pour le clavier).
    base	(uint32_t)addr	L'adresse mémoire de ta fonction de traitement (le wrapper ASM).
    sel	0x08	Le "Code Segment". Dans un noyau 32 bits standard, c'est l'offset 8 dans la GDT.
    flags	0x8E	10001110 en binaire : Présent (1), Privilège Ring 0 (00), Type Interruption 32-bit (01110).
    */

    // Note : Le flag 0x8E (ou 0xEE si tu veux que les futurs programmes en User Mode puissent l'appeler) définit une porte d'interruption 32 bits.
    //idt_set_gate(0x80, (uint32_t)syscall_handler, 0x08, 0x8E);

    idt_set_gate(0x80, (uint32_t)syscall_wrapper, 0x08, 0xEE);

    // 5. Charger l'IDT dans le registre CPU (lidt)
    // Note : On utilise l'adresse de idtp
    __asm__ __volatile__("lidt %0" : : "m" (idtp));

    // 6. Démasquer uniquement le clavier (IRQ 1)
    // 0xFD = 11111101 (le bit 1 à 0 active l'IRQ 1)
    //outb(0x21, 0xFD);

    //autoriser IRQ 0 et IRQ 1 : 
    outb(0x21, 0xFC);
    
    // 7. Activer les interruptions matérielles au niveau CPU
    __asm__ __volatile__("sti");
}

void idt_load() {
    __asm__ __volatile__("lidt (%0)" : : "r" (&idtp));
}