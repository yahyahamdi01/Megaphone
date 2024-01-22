#include <arpa/inet.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/select.h>
#include <sys/time.h>
#include <unistd.h>

#include "billet.h"
#include "error.h"
#include "func/func_serveur.h"
#include "message.h"
#include "request.h"
#include "users.h"

#define SIZE_MESS 200
#define MAX_USERS 2047
#define ID_BITS 11
#define SIZE_PAQUET 512
#define SIZE_FILENAME 256
#define PORT_MD 1223

static uint16_t generate_user_id(const username_t username) {
    // HASH
    uint16_t id = 0;
    unsigned long hash = 5381;
    int c;
    while ((c = *username++)) {
        hash = ((hash << 5) + hash) + c;
    }

    for (int i = 0; i < ID_BITS; i++) {
        int bit = (hash >> i) & 1;
        id |= bit << i;
    }

    return id;
}

static void create_new_user(const username_t username, uint16_t user_id,
                            utilisateur liste[], int nb_utilisateurs) {
    utilisateur user;
    memcpy(user.pseudo, username, USERNAME_LEN);
    user.id = user_id;

    liste[nb_utilisateurs] = user;
    nb_utilisateurs++;
}

static int is_pseudo_used(const username_t username, const utilisateur liste[],
                          int nb_utilisateurs) {
    for (int i = 0; i < nb_utilisateurs; i++) {
        if (strcmp(liste[i].pseudo, username) == 0) {
            return 1;
        }
    }
    return 0;
}

int inscription_request(int sock_client, char *buf, utilisateur liste[], int nb_utilisateurs) {
    uint16_t header;
    uint16_t id;
    codereq_t codereq;
    username_t username;

    memcpy(&header, buf, sizeof(uint16_t));
    codereq = ntohs(header) & 0x1F;
    id = ntohs(header) >> 5;
    memcpy(username, buf + sizeof(uint16_t), USERNAME_LEN);

    // TEST SI LE NOMBRE MAX D'UTILISATEURS EST ATTEINT
    if (nb_utilisateurs >= MAX_USERS) {
        error_request(sock_client, codereq, id, ERR_MAX_USERS_REACHED);
        return 1;
    }

    // TEST SI LE PSEUDONYME EST DEJA UTILISE
    if (is_pseudo_used(username, liste, nb_utilisateurs)) {
        error_request(sock_client, codereq, id, ERR_PSEUDO_ALREADY_USED);
        return 1;
    }

    // AFFICHAGE DU MESSAGE DU CLIENT
    char buf2[USERNAME_LEN + 1];
    printf("CODEREQ %d, ID %hd, PSEUDONYME %s\n", codereq, id, username_to_string(username, buf2));

    id = generate_user_id(username);

    create_new_user(username, id, liste, nb_utilisateurs);

    codereq = REQ_INSCRIPTION;

    header = htons((id << 5) | (codereq & 0x1F));

    memset(buf, 0, SIZE_MESS);
    memcpy(buf, &header, sizeof(header));

    send_message(sock_client, buf, SIZE_MESS);

    return 0;
}

int post_billet_request(int sock_client, char *buf, fils_t *fils, username_t username) {
    // TRADUCTION DU MESSAGE DU CLIENT
    uint16_t header, id, numfil, nb;
    uint8_t codereq, lendata;
    char data[SIZE_MESS + 1];
    memset(data, 0, SIZE_MESS);

    // RECUPERATION DE L'ENTETE
    memcpy(&header, buf, sizeof(uint16_t));
    memcpy(&numfil, buf + sizeof(uint16_t), sizeof(uint16_t));
    memcpy(&nb, buf + sizeof(uint16_t) * 2, sizeof(uint16_t));
    memcpy(&lendata, buf + sizeof(uint16_t) * 3, sizeof(uint8_t));
    memcpy(data, buf + sizeof(uint16_t) * 3 + sizeof(uint8_t), lendata);

    codereq = ntohs(header) & 0x1F;
    id = ntohs(header) >> 5;
    numfil = ntohs(numfil);
    nb = ntohs(nb);

    // AFFICHAGE DU MESSAGE DU CLIENT
    printf("CODEREQ %hd, ID %hd, NUMFIL %hd, NB %hd, LENDATA %hd, DATA %s\n",
           codereq, id, numfil, nb, lendata, data);

    // SI LE NUMERO DE FIL EST 0, CREER UN NOUVEAU FIL
    if (numfil == 0) {
        int newfil = create_fil_message(fils, id, lendata, data, username);

        if (newfil == -1) {
            error_request(sock_client, codereq, id, ERR_MAX_FILS_REACHED);
            return 1;
        }
    } else {
        // SINON, AJOUTER LE BILLET AU FIL

        // TEST SI LE NUMERO DE FIL EST VALIDE
        if (numfil > fils->nb_fil) {
            error_request(sock_client, codereq, id, ERR_NUMFIL);
            return 1;
        }

        if (add_message(fils, numfil, id, lendata, data, username) == -1) {
            error_request(sock_client, codereq, id, ERR_MAX_BILLETS_REACHED);
            return 1;
        }
    }

    // REPONSE AU CLIENT
    codereq = REQ_POST_BILLET;
    header = htons((id << 5) | (codereq & 0x1F));
    numfil = htons(numfil);
    nb = htons(0);

    memset(buf, 0, SIZE_MESS * 2);
    memcpy(buf, &header, sizeof(uint16_t));
    memcpy(buf + sizeof(uint16_t), &numfil, sizeof(uint16_t));
    memcpy(buf + sizeof(uint16_t) * 2, &nb, sizeof(uint16_t));

    send_message(sock_client, buf, sizeof(uint16_t) * 3);

    return 0;
}

void error_request(int sock_client, codereq_t codereq_client, uint16_t id,
                   error_t err) {
    uint16_t header_serv;
    char buf[SIZE_MESS];

    switch (err) {
    case ERR_CODEREQ_UNKNOWN:
        printf("CODEREQ INCONNU : <%d>, ID %d\n", codereq_client, id);
        break;
    case ERR_NON_ZERO_ID_WITH_CODE_REQ_ONE:
        printf("ID NON NUL AVEC CODEREQ 1, ID %d\n", id);
        break;
    case ERR_ID_DOES_NOT_EXIST:
        printf("ID INCONNU : <%d>, ID %d\n", codereq_client, id);
        break;
    case ERR_PSEUDO_ALREADY_USED:
        printf("PSEUDONYME DEJA UTILISE : <%d>, ID %d\n", codereq_client, id);
        break;
    case ERR_MAX_FILS_REACHED:
        printf("IMPOSSIBLE DE CREER UN NOUVEAU FIL : <%d>, ID %d\n", codereq_client,
               id);
        break;
    case ERR_MAX_BILLETS_REACHED:
        printf("IMPOSSIBLE DE CREER UN NOUVEAU BILLET : <%d>, ID %d\n",
               codereq_client, id);
        break;
    case ERR_MAX_USERS_REACHED:
        printf("IMPOSSIBLE DE CREER UN NOUVEAU UTILISATEUR : <%d>, ID %d\n",
               codereq_client, id);
        break;
    case ERR_NUMFIL:
        printf("NUMERO DE FIL INCONNU : <%d>, ID %d\n", codereq_client, id);
        break;
    default:
        fprintf(stderr, "code erreur inconnu\n");
        exit(1);
    }

    codereq_client = 0;
    header_serv = htons((id << 5) | (codereq_client & 0x1F));

    memset(buf, 0, SIZE_MESS);
    memcpy(buf, &header_serv, sizeof(uint16_t));
    memcpy(buf + sizeof(uint16_t), &err, sizeof(int));

    send_message(sock_client, buf, SIZE_MESS);
}

static int get_nb_billets(fils_t *fils, uint16_t numfil, uint16_t nb,
                          billet_type_t type) {
    int nb_billets = 0;
    // TOUS LES BILLETS DANS UN FIL
    if (type == TYPE_ALL_BILLETS_OF_ONE_FIL) {
        nb_billets = fils->list_fil[numfil - 1].nb_billet;
    }
        // N BILLETS DANS CHAQUE FILS
    else if (type == TYPE_NBILLETS_OF_ALL_FIL) {
        for (int i = 0; i < fils->nb_fil; i++) {
            nb_billets +=
                (fils->list_fil[i].nb_billet > nb) ? nb : fils->list_fil[i].nb_billet;
        }
    }
        // TOUT LES BILLETS DE CHAQUE FILS
    else if (type == TYPE_ALL_BILLETS_OF_ALL_FILS) {
        for (int i = 0; i < fils->nb_fil; i++) {
            nb_billets += fils->list_fil[i].nb_billet;
        }
    } else if (type == TYPE_NORMAL) {
        nb_billets = (fils->list_fil[numfil - 1].nb_billet > nb)
        ? nb
        : fils->list_fil[numfil - 1].nb_billet;
    }

    return nb_billets;
}

static void send_billet(int sock, fils_t *fils, uint16_t numfil,
                       int pos_billet) {
    int type = fils->list_fil[numfil].billets[pos_billet].type;

    uint8_t lendata;
    char pseudo_fil[USERNAME_LEN];
    char pseudo_billet[USERNAME_LEN];
    char *data;
    if (type == MESSAGE)
        data = malloc(SIZE_MESS + 1);
    else if (type == FICHIER)
        data = fils->list_fil[numfil].billets[pos_billet].billet.fichier.filename;

    memset(pseudo_fil, 0, USERNAME_LEN);
    memset(pseudo_billet, 0, USERNAME_LEN);

    size_t sizebillet = sizeof(uint16_t) + USERNAME_LEN * 2 + sizeof(uint8_t) + (SIZE_MESS + 1);
    char *billet = malloc(sizebillet);
    if (billet == NULL) {
        perror("malloc");
        exit(1);
    }

    memcpy(pseudo_fil, fils->list_fil[numfil].pseudo, USERNAME_LEN);
    memcpy(pseudo_billet, fils->list_fil[numfil].billets[pos_billet].pseudo, USERNAME_LEN);
    if (type == MESSAGE) {
        memcpy(data, fils->list_fil[numfil].billets[pos_billet]
                .billet.message.contenu, SIZE_MESS+1);
        lendata = fils->list_fil[numfil].billets[pos_billet].len;
    }
    else if (type == FICHIER) {
        lendata = strlen(data);
    }

    numfil += 1;
    pos_billet += 1;

    char buf_pseudo_fil[USERNAME_LEN + 1]; username_to_string(pseudo_fil, buf_pseudo_fil);
    char buf_pseudo_billet[USERNAME_LEN + 1]; username_to_string(pseudo_billet, buf_pseudo_billet);

    if (type == MESSAGE) {
        printf("MESSAGE : BILLET %d DU FIL %d : PSEUDO FIL %s, PSEUDO BILLET %s, LEN DATA %d, DATA %s\n",
            pos_billet, numfil, buf_pseudo_fil, buf_pseudo_billet, lendata, data);
    }
    else if (type == FICHIER) {
        printf("FICHIER : BILLET %d DU FIL %d : PSEUDO FIL %s, PSEUDO BILLET %s, LEN DATA %d, DATA %s\n",
            pos_billet, numfil, buf_pseudo_fil, buf_pseudo_billet, lendata, data);
    }
    printf("ENVOI DU BILLET %d DU FIL %d\n", pos_billet, numfil);

    numfil = htons(numfil);

    char *ptr = billet;
    memcpy(ptr, &numfil, sizeof(uint16_t));
    ptr += sizeof(uint16_t);
    memcpy(ptr, pseudo_fil, strlen(pseudo_fil) + 1);
    ptr += strlen(pseudo_fil) + 1;
    memcpy(ptr, pseudo_billet, strlen(pseudo_billet) + 1);
    ptr += strlen(pseudo_billet) + 1;
    memcpy(ptr, &lendata, sizeof(uint8_t));
    ptr += sizeof(uint8_t);
    memcpy(ptr, data, strlen(data) + 1);

    send_message(sock, billet, sizebillet);
}

void send_type_all_billets_of_one_fil(int sock, fils_t *fils,
                                      uint16_t numfil) {
    int nombre_billets = fils->list_fil[numfil - 1].nb_billet;
    for (int i = nombre_billets - 1; i >= 0; i--) {
        send_billet(sock, fils, numfil - 1, i);
    }
}

static void send_type_nbillets_of_all_fil(int sock, fils_t *fils, int n) {
    int nombre_fils = fils->nb_fil;
    for (int i = 0; i < nombre_fils; i++) {
        int nombre_billets_temp = fils->list_fil[i].nb_billet;

        for (int j = nombre_billets_temp - 1;
             j >= nombre_billets_temp - n && j >= 0; j--) {
            send_billet(sock, fils, i, j);
        }
    }
}

static void send_type_all_billets_of_all_fil(int sock, fils_t *fils) {
    int nombre_fils = fils->nb_fil;
    for (int i = nombre_fils - 1; i >= 0; i--) {
        int nombre_billets = fils->list_fil[i].nb_billet;
        for (int j = nombre_billets - 1; j >= 0; j--) {
            send_billet(sock, fils, i, j);
        }
    }
}

static void send_type_normal(int sock, fils_t *fils, uint16_t numfil,
                             int n) {
    int nombre_billets = fils->list_fil[numfil - 1].nb_billet;

    for (int j = nombre_billets - 1; j >= nombre_billets - n && j >= 0; j--) {
        send_billet(sock, fils, numfil - 1, j);
    }
}

static int send_billets(int sock, fils_t *fils, uint16_t numfil, int n,
                        billet_type_t type) {
    if (type == TYPE_ALL_BILLETS_OF_ONE_FIL) {
        send_type_all_billets_of_one_fil(sock, fils, numfil);
    } else if (type == TYPE_NBILLETS_OF_ALL_FIL) {
        send_type_nbillets_of_all_fil(sock, fils, numfil);
    } else if (type == TYPE_ALL_BILLETS_OF_ALL_FILS) {
        send_type_all_billets_of_all_fil(sock, fils);
    } else if (type == TYPE_NORMAL) {
        send_type_normal(sock, fils, numfil, n);
    }

    return 0;
}

static billet_type_t find_case_type(uint16_t numfil, uint16_t nb,
                                    int sock_client, codereq_t codereq,
                                    uint16_t id) {
    billet_type_t type;
    if (numfil == 0 && nb == 0) {
        type = TYPE_ALL_BILLETS_OF_ALL_FILS;
    } else if (numfil == 0) {
        type = TYPE_NBILLETS_OF_ALL_FIL;
    } else if (nb == 0) {
        type = TYPE_ALL_BILLETS_OF_ONE_FIL;
    } else if (nb != 0 && numfil != 0) {
        type = TYPE_NORMAL;
    } else {
        error_request(sock_client, codereq, id, ERR_NON_TYPE);
        return -1;
    }

    return type;
}

static int send_num_billets_to_client(int sock_client, uint16_t numfil,
                                      int nb_billets, uint16_t id) {
    uint16_t codereq, header, nb;

    codereq = REQ_GET_BILLET;
    header = htons((id << 5) | (codereq & 0x1F));
    numfil = htons(numfil);
    nb = htons(nb_billets);

    size_t sizebuf = sizeof(uint16_t) * 3;
    char buffer[sizebuf];
    memset(buffer, 0, sizebuf);

    memcpy(buffer, &header, sizeof(uint16_t));
    memcpy(buffer + sizeof(uint16_t), &numfil, sizeof(uint16_t));
    memcpy(buffer + sizeof(uint16_t) * 2, &nb, sizeof(uint16_t));

    send_message(sock_client, buffer, sizebuf);

    return 0;
}

int get_billets_request(int sock_client, const char *buf, fils_t *fils) {
    uint16_t header, codereq, id, numfil, nb;
    int n;

    const char *ptr = buf;
    memcpy(&header, ptr, sizeof(uint16_t));
    ptr += sizeof(uint16_t);
    memcpy(&numfil, ptr, sizeof(uint16_t));
    ptr += sizeof(uint16_t);
    memcpy(&nb, ptr, sizeof(uint16_t));

    codereq = ntohs(header) & 0x1F;
    id = ntohs(header) >> 5;
    numfil = ntohs(numfil);
    nb = ntohs(nb);

    n = nb;

    printf("CODEREQ %hd, ID %hd, NUMFIL %hd, NB %hd\n", codereq, id, numfil, nb);

    // TEST SI LE NUMERO DE FIL EST VALIDE
    if (numfil > fils->nb_fil) {
        error_request(sock_client, codereq, id, ERR_NUMFIL);
        return 1;
    }

    billet_type_t type = find_case_type(numfil, nb, sock_client, codereq, id);
    if (type == TYPE_ERROR) {
        return 1;
    }

    int nb_billets = get_nb_billets(fils, numfil, nb, type);

    // ENVOIE LE NOMBRE DE BILLETS QUE VA RECEVOIR LE CLIENT
    send_num_billets_to_client(sock_client, numfil, nb_billets, id);

    // ON ENVOIE LES BILLETS AU CLIENT
    send_billets(sock_client, fils, numfil, n, type);

    return 0;
}

int subscribe_request(int sock_client, char *buf) {
    // TRADUCTION DU MESSAGE DU CLIENT
    uint16_t header, id, numfil, nb,addr_MD;
    uint8_t codereq, lendata;
    char data[SIZE_MESS+1];
    memset(data, 0, SIZE_MESS);

    // RECUPERATION DE L'ENTETE
    memcpy(&header, buf, sizeof(uint16_t));
    memcpy(&numfil, buf+sizeof(uint16_t), sizeof(uint16_t));
    memcpy(&nb, buf+sizeof(uint16_t)*2, sizeof(uint16_t));
    memcpy(&lendata, buf+sizeof(uint16_t)*3, sizeof(uint8_t));
    memcpy(data, buf+sizeof(uint16_t)*3+sizeof(uint8_t), lendata);

    codereq = ntohs(header) & 0x1F;
    id = ntohs(header) >> 5;
    numfil = ntohs(numfil);
    nb = ntohs(nb);

    // AFFICHAGE DU MESSAGE DU CLIENT
    printf("CODEREQ %hd, ID %hd, NUMFIL %hd, NB %hd, LENDATA %hd, DATA %s\n", codereq, id, numfil, nb, lendata, data);

    int sock_udp = socket(AF_INET6, SOCK_DGRAM, 0);
    if(sock_udp < 0){
        perror("creation socket");
        exit(1);
    }
    char addr_fil_str[INET6_ADDRSTRLEN];
    strcpy(addr_fil_str, "ff02::");
    snprintf(addr_fil_str, INET6_ADDRSTRLEN, "%u",numfil);
    //initialiser l'addresse multicast 
    struct sockaddr_in6 grp;
    memset(&grp, 0, sizeof(grp));
    grp.sin6_family = AF_INET6;
    grp.sin6_port = htons(PORT_MD);
    inet_pton(AF_INET6, addr_fil_str, &grp.sin6_addr);
    int ifindex = 0; //interface par defaut
    if(setsockopt(sock_udp, IPPROTO_IPV6, IPV6_MULTICAST_IF, &ifindex, sizeof(ifindex)))
        {
            perror("erreur initialisation de l’interface locale");
        }


// REPONSE AU CLIENT
    codereq = REQ_POST_BILLET;
    header = htons((id << 5) | (codereq & 0x1F));
    numfil = htons(numfil);
    nb = htons(0);
    

    memset(buf, 0, SIZE_MESS*2);
    memcpy(buf, &header, sizeof(uint16_t));
    memcpy(buf+sizeof(uint16_t), &numfil, sizeof(uint16_t));
    memcpy(buf+sizeof(uint16_t)*2, &nb, sizeof(uint16_t));
    memcpy(buf+sizeof(uint16_t)*3, &addr_MD, sizeof(uint16_t));
    send_message(sock_client, buf, sizeof(uint16_t)*4);

    return 0;
}


int add_file_request(int sock_client, char *buf, fils_t *fils, int sock_udp, 
                int port_udp, struct sockaddr_in6 addr_udp, username_t username) {
    uint16_t header, id, numfil, nb, numbloc;
    uint8_t codereq, lendata;
    char data[SIZE_FILENAME];
    socklen_t addr_udp_len = sizeof(struct sockaddr_in6);

    char *ptr = buf;
    memcpy(&header, ptr, sizeof(uint16_t));
    ptr += sizeof(uint16_t);
    memcpy(&numfil, ptr, sizeof(uint16_t));
    ptr += sizeof(uint16_t);
    memcpy(&nb, ptr, sizeof(uint16_t));
    ptr += sizeof(uint16_t);
    memcpy(&lendata, ptr, sizeof(uint8_t));
    ptr += sizeof(uint8_t);
    memcpy(&data, ptr, lendata);

    codereq = ntohs(header) & 0x1F;
    id = ntohs(header) >> 5;
    numfil = ntohs(numfil);
    nb = ntohs(nb);

    printf("CODEREQ %hd, ID %hd, NUMFIL %hd, NB %hd, LENDATA %hd, DATA %s\n", codereq, id, numfil, nb, lendata, data);

    // TEST SI LE NUMERO DE FIL EST VALIDE
    if (numfil > fils->nb_fil) {
        error_request(sock_client, codereq, id, ERR_NUMFIL);
        return 1;
    }

    // ENVOIE PORT UDP AU CLIENT
    nb = htons(port_udp);

    size_t sizebuf = sizeof(uint16_t) * 3 + sizeof(uint8_t) + lendata;
    char buffer[sizebuf];
    memset(buffer, 0, sizebuf);

    ptr = buffer;
    memcpy(ptr, &header, sizeof(uint16_t));
    ptr += sizeof(uint16_t);
    memcpy(ptr, &numfil, sizeof(uint16_t));
    ptr += sizeof(uint16_t);
    memcpy(ptr, &nb, sizeof(uint16_t));
    ptr += sizeof(uint16_t);

    send_message(sock_client, buffer, sizebuf);

    close(sock_client);

    // RECEPTION DES PAQUETS UDP
    size_t size_all = MAX_FILE_SIZE;
    char *all_paquets = malloc(size_all);

    char paquet[SIZE_PAQUET];
    memset(paquet, 0, SIZE_PAQUET);

    size_t size = sizeof(uint16_t)*2 + SIZE_PAQUET;
    char buffer_udp[size];

    int timeout = 5;
    int timeoutU = 0;
    fd_set readset;
    FD_ZERO(&readset);
    FD_SET(sock_udp, &readset);

    struct timeval tv;
    tv.tv_sec = timeout;
    tv.tv_usec = timeoutU;

    while (1) { 
        memset(buffer_udp, 0, size);

        int ready = select(sock_udp + 1, &readset, NULL, NULL, &tv);

        if (ready == -1) {
            perror("select");
            exit(1);
        } else if (ready == 0) {
            printf("TIMEOUT UDP\n");
            break;
        }

        recvfrom(sock_udp, buffer_udp, size, 0, (struct sockaddr *) &addr_udp, &addr_udp_len);
        ptr = buffer_udp;
        memcpy(&header, ptr, sizeof(uint16_t));
        ptr += sizeof(uint16_t);
        memcpy(&numbloc, ptr, sizeof(uint16_t));
        ptr += sizeof(uint16_t);
        memcpy(&paquet, ptr, SIZE_PAQUET);

        strcat(all_paquets, paquet);

        if (strlen(paquet) < SIZE_PAQUET) {
            break;
        }
    }

    // ON AFFICHE LE CONTENU DU FICHIER
    size_t filesize = strlen(all_paquets);
    printf("FICHIER RECU : NOM %s TAILLE %d\n", data, (int) filesize);

    // ON AJOUTE LE FICHIER DANS LE FIL
    if (numfil == 0) {
        create_fil_fichier(fils, id, filesize, data, all_paquets, username);
    }
    else {
        add_file(fils, numfil, id, filesize, data, all_paquets, username);
    }

    free(all_paquets);

    return 0;
}

static int find_file_fil(fils_t *fils, uint16_t numfil, char *data) {
    for (int i = 0; i < fils->list_fil[numfil].nb_billet; i++) {
        billet_t *billet = &fils->list_fil[numfil].billets[i];
        if (billet->type == FICHIER) {
            if (strcmp(billet->billet.fichier.filename, data) == 0) {
                return i;
            }
        }
    }

    return -1;
}

int dw_file_request(int sock_client, char *buf, fils_t *fils) {
    uint16_t header, id, numfil, nb, numbloc;
    uint8_t codereq, lendata;
    char data[SIZE_FILENAME];

    char *ptr = buf;
    memcpy(&header, ptr, sizeof(uint16_t));
    ptr += sizeof(uint16_t);
    memcpy(&numfil, ptr, sizeof(uint16_t));
    ptr += sizeof(uint16_t);
    memcpy(&nb, ptr, sizeof(uint16_t));
    ptr += sizeof(uint16_t);
    memcpy(&lendata, ptr, sizeof(uint8_t));
    ptr += sizeof(uint8_t);
    memcpy(&data, ptr, lendata);

    codereq = ntohs(header) & 0x1F;
    id = ntohs(header) >> 5;
    numfil = ntohs(numfil);
    nb = ntohs(nb);

    printf("CODEREQ %hd, ID %hd, NUMFIL %hd, NB %hd, LENDATA %hd, DATA %s\n", codereq, id, numfil, nb, lendata, data);

    // TEST SI LE NUMERO DE FIL EST VALIDE
    if (numfil > fils->nb_fil) {
        error_request(sock_client, codereq, id, ERR_NUMFIL);
        return 1;
    }

    // ON TESTE SI LE FICHIER EXISTE DANS LE FIL DONNE
    int find = find_file_fil(fils, numfil-1, data);
    if (find == -1) return 1;

    // SI LE FICHIER EXISTE ON ENVOIE AU CLIENT L'AUTORISATION DE TELECHARGEMENT
    size_t sizebuf = sizeof(uint16_t) * 3 + sizeof(uint8_t) + lendata;
    send_message(sock_client, buf, sizebuf);

    // LE CLIENT ET LE SERVEUR SE DECONNECTE EN TCP ET ON PASSE EN UDP
    close(sock_client);

    // ON RECUPERE LE PORT UDP DU CLIENT
    int sock_udp = socket(AF_INET6, SOCK_DGRAM, 0);
    if (sock_udp == -1) {
        perror("Impossible de créer la socket UDP");
        exit(1);
    }

    struct sockaddr_in6 addr_udp;
    socklen_t addr_udp_len = sizeof(addr_udp);
    addr_udp.sin6_family = AF_INET6;
    addr_udp.sin6_port = htons(nb);
    addr_udp.sin6_addr = in6addr_any;

    // CALCULE DU NOMBRE DE PAQUETS
    size_t filesize = fils->list_fil[numfil-1].billets[find].len;
    char *file = fils->list_fil[numfil-1].billets[find].billet.fichier.data;

    int nb_paquets = 0;
    nb_paquets = filesize / SIZE_PAQUET + 1;

    // ENVOI DES PAQUETS
    size_t size = sizeof(header)+sizeof(numbloc)+SIZE_PAQUET;
    char buffer[size];
    memset(buffer, 0, size);

    char paquet[SIZE_PAQUET];
    memset(paquet, 0, SIZE_PAQUET);

    for (int i = 0; i < nb_paquets; i++) {
        memset(paquet, 0, SIZE_PAQUET);
        memcpy(paquet, file + i * SIZE_PAQUET, SIZE_PAQUET);

        ptr = buffer;
        memcpy(ptr, &header, sizeof(header));
        ptr += sizeof(header);
        numbloc = htons(i);
        memcpy(ptr, &numbloc, sizeof(numbloc));
        ptr += sizeof(numbloc);
        memcpy(ptr, paquet, SIZE_PAQUET);

        sendto(sock_udp, buffer, size, 0, (struct sockaddr *) &addr_udp, addr_udp_len);
    }

    close(sock_udp);
 
    return 0;
}
