#include <ctype.h>
#include <string.h>

#include "users.h"

char *username_to_string(username_t username, char *buf) {
    memcpy(buf, username, USERNAME_LEN);
    buf[USERNAME_LEN] = '\0';

    for (int i = 0; i < USERNAME_LEN; i++) {
        if (buf[i] == '#') {
            buf[i] = '\0';
        }
    }

    return buf;
}

username_error string_to_username(const char *str, username_t username) {
    memset(username, 0, USERNAME_LEN);

    size_t i = 0, j = 0;
    while (j < 10 && str[i]) {
        if (isprint(str[i]) && !isspace(str[i])) {
            username[j] = str[i];
            j++;
        }
        i++;
    }

    for (; str[i]; i++) {
        if (isprint(str[i]) && !isspace(str[i])) return USERNAME_TOO_LONG;
    }
    if (j == 0) return USERNAME_EMPTY;

    for (size_t i = j; i < 10; i++) {
        username[i] = '#';
    }

    return USERNAME_OK;
}
