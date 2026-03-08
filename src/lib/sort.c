#include "sort.h"

void sort_files(struct file_info* files, int n, int criteria, int reverse) {
    for (int i = 0; i < n - 1; i++) {
        for (int j = 0; j < n - i - 1; j++) {
            int swap = 0;
            switch(criteria) {
                case 1: // Par Nom
                    swap = (strcmp(files[j].name, files[j+1].name) > 0);
                    break;
                case 2: // Par Taille
                    swap = (files[j].size > files[j+1].size);
                    break;
                case 3: // Par Date/Heure (Date en MSB, Time en LSB)
                    if (files[j].date == files[j+1].date)
                        swap = (files[j].time > files[j+1].time);
                    else
                        swap = (files[j].date > files[j+1].date);
                    break;
                case 4: // Par Type (Dossiers d'abord, puis BIN, puis reste)
                    int type_j = (files[j].attr & 0x10) ? 0 : (files[j].is_bin ? 1 : 2);
                    int type_next = (files[j+1].attr & 0x10) ? 0 : (files[j+1].is_bin ? 1 : 2);
                    swap = (type_j > type_next);
                    break;
            }

            if (reverse) swap = !swap && (files[j].size != files[j+1].size); // Simplifié

            if (swap) {
                struct file_info tmp = files[j];
                files[j] = files[j+1];
                files[j+1] = tmp;
            }
        }
    }
}