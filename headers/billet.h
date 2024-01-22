#ifndef BILLET_H
#define BILLET_H

#define MAX_FILE_SIZE 35127296
#define SIZE_MESS 200
#define NB_MAX_BILLETS_PAR_FIL 100
#define NB_MAX_FILS 100
#define SIZE_FILENAME 256

#include <stdint.h>

#include "users.h"

typedef enum {
    MESSAGE, 
    FICHIER
} type_billet_t;

typedef struct {
    type_billet_t type;
    uint16_t idClient;
    username_t pseudo;
    size_t len;
    union {
        struct {
            char contenu[SIZE_MESS + 1];
        } message;
        struct {
            char filename[SIZE_FILENAME + 1];
            char *data;
        } fichier;
    } billet;
} billet_t;

typedef struct {
    billet_t billets[NB_MAX_BILLETS_PAR_FIL];
    username_t pseudo;
    int nb_billet;
} fil_t;

typedef struct {
    fil_t list_fil[NB_MAX_FILS];
    int nb_fil;
} fils_t;

// Creation d'un nouveau fil ou le premier contenu est un message
int create_fil_message(fils_t *, uint16_t, uint8_t, const char *, const username_t);
// Creation d'un nouveau fil ou le premier contenu est un fichier
int create_fil_fichier(fils_t *, uint16_t, size_t, const char *, const char *, const username_t);
// Ajout d'un message dans un fil
int add_message(fils_t *, uint16_t, uint16_t, uint8_t, const char *, const username_t);
// Ajout d'un fichier dans un fil
int add_file(fils_t *, uint16_t, uint16_t, size_t, const char *, const char *, const username_t);

#endif
