#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../headers/billet.h"

static billet_t create_billet_message(uint16_t id, uint8_t lendata, const char *data, const username_t username) {
    billet_t new_billet;
    new_billet.type = MESSAGE;
    new_billet.idClient = id;
    new_billet.len = lendata;
    memcpy(new_billet.pseudo, username, USERNAME_LEN);
    memcpy(new_billet.billet.message.contenu, data, lendata);
    new_billet.billet.message.contenu[lendata] = '\0';

    return new_billet;
}

static billet_t create_billet_fichier(uint16_t id, size_t filesize, const char *filename, 
                const char *data, const username_t username) {
    billet_t new_billet;
    new_billet.type = FICHIER;
    new_billet.idClient = id;
    new_billet.len = filesize;
    memcpy(new_billet.pseudo, username, USERNAME_LEN);
    memcpy(new_billet.billet.fichier.filename, filename, strlen(filename));
    new_billet.billet.fichier.filename[strlen(filename)] = '\0';
    new_billet.billet.fichier.data = malloc(filesize + 1);
    memcpy(new_billet.billet.fichier.data, data, filesize);
    new_billet.billet.fichier.data[filesize] = '\0';

    return new_billet;
}

int add_message(fils_t *fils, uint16_t numfil, uint16_t id, uint8_t lendata,
               const char *data, const username_t username) {
    int last_billet = fils->list_fil[numfil - 1].nb_billet;
    if (last_billet >= 100) {
        return -1;
    }

    billet_t new_billet = create_billet_message(id, lendata, data, username);

    memcpy(&fils->list_fil[numfil - 1].billets[last_billet], &new_billet,
           sizeof(billet_t));
    fils->list_fil[numfil - 1].nb_billet++;

    return 0;
}

int add_file(fils_t *fils, uint16_t numfil, uint16_t id, size_t filesize, const char *filename, 
                const char *data, const username_t username) {
    int last_billet = fils->list_fil[numfil - 1].nb_billet;
    if (last_billet >= 100) {
        return -1;
    }

    billet_t new_billet = create_billet_fichier(id, filesize, filename, data, username);

    memcpy(&fils->list_fil[numfil - 1].billets[last_billet], &new_billet,
           sizeof(billet_t));
    fils->list_fil[numfil - 1].nb_billet++;

    return 0;
}

int create_fil_message(fils_t *fils, uint16_t id, uint8_t lendata, const char *data, const username_t username) {
    if (fils->nb_fil >= 100)
        return -1;

    billet_t new_billet = create_billet_message(id, lendata, data, username);

    fil_t new_fil;
    memcpy(&new_fil.billets[0], &new_billet, sizeof(billet_t));
    new_fil.nb_billet = 1;
    memcpy(new_fil.pseudo, username, USERNAME_LEN);
    memcpy(&fils->list_fil[fils->nb_fil], &new_fil, sizeof(fil_t));
    fils->nb_fil++;

    return fils->nb_fil;
}

int create_fil_fichier(fils_t *fils, uint16_t id, size_t filesize, const char *filename, 
                const char *data, const username_t username) {
    if (fils->nb_fil >= 100)
        return -1;

    billet_t new_billet = create_billet_fichier(id, filesize, filename, data, username);

    fil_t new_fil;
    memcpy(&new_fil.billets[0], &new_billet, sizeof(billet_t));
    new_fil.nb_billet = 1;
    memcpy(new_fil.pseudo, username, USERNAME_LEN);
    memcpy(&fils->list_fil[fils->nb_fil], &new_fil, sizeof(fil_t));
    fils->nb_fil++;

    return fils->nb_fil;
}
