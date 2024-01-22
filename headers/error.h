#ifndef ERROR_H_
#define ERROR_H_

typedef enum {
    // CODEREQ inconnu
    ERR_CODEREQ_UNKNOWN,
    // ID non nul avec CODEREQ=1
    ERR_NON_ZERO_ID_WITH_CODE_REQ_ONE,
    // ID inexistant dans la table
    ERR_ID_DOES_NOT_EXIST,
    // PSEUDO déjà utilisé
    ERR_PSEUDO_ALREADY_USED,
    // Impossible d'ajouter un fil car la table est pleine
    ERR_MAX_FILS_REACHED,
    // Impossible d'ajouter un utilisateur car la table est pleine
    ERR_MAX_USERS_REACHED,
    // Impossible d'ajouter un billet car la table est pleine
    ERR_MAX_BILLETS_REACHED,
    // Numéro de fil inexistant
    ERR_NUMFIL,
    // Type pour affichage billets
    ERR_NON_TYPE,
    // Le fichier n'existe pas
    ERR_FILE
} error_t;

#endif
