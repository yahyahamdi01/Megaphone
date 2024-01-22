#ifndef USER_H
#define USER_H

#include <stdint.h>

#define USERNAME_LEN 10

typedef char username_t[USERNAME_LEN];
// Structure d'un utilisateur
typedef struct {
    username_t pseudo;
    uint16_t id;
} utilisateur;

typedef enum {
    USERNAME_OK,
    USERNAME_TOO_LONG,
    USERNAME_EMPTY,
} username_error;

char *username_to_string(username_t username, char *buf);
username_error string_to_username(const char *str, username_t buf);

#endif
