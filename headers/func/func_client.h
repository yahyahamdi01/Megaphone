#ifndef FUNC_CLIENT_H
#define FUNC_CLIENT_H

#include <stdint.h>

#include "users.h"

// Requete d'erreur
int error_request(const char *);
// Suppression des caracteres invisibles
void remove_special_chars(username_t);
// Remplissage du pseudo par des #
void completion_pseudo(char *);
// Demande du pseudo
void demande_pseudo(username_t);
// Creation de l'entete du serveur
uint16_t create_header(uint8_t);
// Remplissage entete buffer
void header_username_buffer(char *, uint16_t, username_t);
// Connexion au serveur
int connexion_server(const char *, const char *);
// Requete d'inscription
int inscription_request(int);
// Requete poster un billet
int post_billet_request(int);
// Requete obtention des billets
int get_billets_request(int);
// Requete ajout d'un fichier
int add_file_request(int);
// Requete telechargement d'un fichier
int dw_file_request(int);
// Requete d'abonnement
int subscribe_request(int);

#endif
