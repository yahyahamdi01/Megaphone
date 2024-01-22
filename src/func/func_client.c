#include <arpa/inet.h>
#include <ctype.h>
#include <netdb.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/time.h>
#include <unistd.h>

#include "billet.h"
#include "error.h"
#include "func/func_client.h"
#include "message.h"
#include "request.h"

#define MAX_VALUE_11BITS_INTEGER ((1 << 11) - 1)
#define BUFLEN 4096
#define SIZE_FILENAME 256
#define SIZE_PAQUET 512
#define PORT_UDP_DW 9122
#define PORT_MD 1223

int error_request(const char *buf) {
    uint16_t header;
    uint8_t codereq;
    int type;

    memcpy(&header, buf, sizeof(header));
    codereq = ntohs(header) & 0x1f;
    if (codereq > 0) {
        return 0;
    }

    memcpy(&type, buf + sizeof(uint16_t), sizeof(type));

    switch (type) {
    case ERR_CODEREQ_UNKNOWN:
        printf("ERREUR : REQUETE INCONNUE \n");
        break;
    case ERR_NON_ZERO_ID_WITH_CODE_REQ_ONE:
        printf("ERREUR : ID NON NUL AVEC CODEREQ=1 \n");
        break;
    case ERR_ID_DOES_NOT_EXIST:
        printf("ERREUR : ID INEXISTANT DANS LA TABLE \n");
        break;
    case ERR_PSEUDO_ALREADY_USED:
        printf("ERREUR : PSEUDO DEJA UTILISE \n");
        break;
    case ERR_MAX_FILS_REACHED:
        printf("ERREUR : IMPOSSIBLE DE CREER UN NOUVEAU FIL \n");
        break;
    case ERR_MAX_USERS_REACHED:
        printf("ERREUR : IMPOSSIBLE DE CREER UN NOUVEAU UTILISATEUR \n");
        break;
    case ERR_MAX_BILLETS_REACHED:
        printf("ERREUR : IMPOSSIBLE DE CREER UN NOUVEAU BILLET \n");
        break;
    case ERR_NUMFIL:
        printf("ERREUR : NUMERO DE FIL INEXISTANT \n");
        break;
    default:
        fprintf(stderr, "ERREUR : ERREUR INCONNUE \n");
        exit(1);
    }

    return 1;
}

void demande_pseudo(username_t username) {
    bool ok = false;
    char buf[BUFLEN];
    while(!ok) {
        printf("Saisir votre pseudo: ");
        const char *r = fgets(buf, BUFLEN, stdin);
        if (r == NULL) {
            fprintf(stderr, "Erreur: EOF\n");
            exit(1);
        }

        username_error ru = string_to_username(buf, username);
        switch (ru) {
        case USERNAME_OK:
            ok = true;
            break;
        case USERNAME_TOO_LONG:
            printf("Le pseudo ne peut pas depasser 10 caractères\n");
            break;
        case USERNAME_EMPTY:
            break;
        }

    }

    printf("PSEUDO : %s\n", username_to_string(username, buf));
}

uint16_t create_header(uint8_t codereq_client) {
    uint16_t id = 0;
    uint16_t header_client = htons((id << 5) | (codereq_client & 0x1F));

    return header_client;
}

void header_username_buffer(char *buf, uint16_t header_client, username_t username) {
    memcpy(buf, &header_client, sizeof(header_client));
    memcpy(buf + sizeof(header_client), username, strlen(username));
    buf[sizeof(header_client) + strlen(username)] = '\0';
}

static int get_server_addr(const char *hostname, const char *port, int *sock, struct sockaddr_in6 *addr) {
    struct addrinfo hints, *r, *p;
    int ret;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET6;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_V4MAPPED;

    ret = getaddrinfo(hostname, port, &hints, &r);
    if (ret != 0 || r == NULL) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(ret));
        return -1;
    }

    p = r;
    while (p != NULL) {
        if ((*sock = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) > 0) {
            if (connect(*sock, p->ai_addr, sizeof(struct sockaddr_in6)) == 0)
                break;

            close(*sock);
        }

        p = p->ai_next;
    }

    if (p == NULL)
        return -2;

    if (addr != NULL) {
        // on stocke l'adresse de connexion
        memcpy(addr, p->ai_addr, sizeof(struct sockaddr_in6));
    }

    // on libère la mémoire allouée par getaddrinfo
    freeaddrinfo(r);

    return 0;
}

int connexion_server(const char *hostname, const char *port) {
    int sock;
    struct sockaddr_in6 server_addr;

    switch (get_server_addr(hostname, port, &sock, &server_addr)) {
    case 0:
        break;
    case -1:
        fprintf(stderr, "Erreur: hote non trouve.\n");
        exit(1);
    case -2:
        fprintf(stderr, "Erreur: echec de creation de la socket.\n");
        exit(1);
    }

    return sock;
}

int inscription_request(int sock) {
    uint16_t header_client, header_serv, id_serv;
    uint8_t codereq_serv;
    username_t username;
    char buf[SIZE_MESS];

    codereq_serv = REQ_INSCRIPTION;

    header_client = create_header(codereq_serv);

    demande_pseudo(username);

    header_username_buffer(buf, header_client, username);

    // ENVOI ENTETE + PSEUDO
    send_message(sock, buf, sizeof(header_client) + USERNAME_LEN);

    // RECEPTION ENTETE + ID
    recv_message(sock, buf, SIZE_MESS);

    if (error_request(buf) == 1) {
        close(sock);
        exit(5);
    }

    // DECODAGE ENTETE + ID
    memcpy(&header_serv, buf, sizeof(header_serv));
    id_serv = ntohs(header_serv) >> 5;
    codereq_serv = ntohs(header_serv) & 0x1F;

    printf("VOICI VOTRE ID : %d \n", id_serv);

    close(sock);

    return 0;
}

int post_billet_request(int sock) {
    uint16_t header, id, numfil, nb;
    uint8_t codereq_client, lendata;
    char data[SIZE_MESS];
    memset(data, 0, SIZE_MESS);

    fflush(stdout);
    printf("IDENTIFIANT ET NUM FIL (0 pour en créer un nouveau) : ");
    int r = scanf("%hu %hu", &id, &numfil);
    if (r == EOF) {
        perror("Erreur: ");
    } else if (r != 2) {
        fprintf(stderr, "Erreur : EOF");
    }

    if (id > MAX_VALUE_11BITS_INTEGER) {
        fprintf(stderr, "Erreur : Cet identifiant ne peux pas exister.");
        exit(1);
    }

    codereq_client = REQ_POST_BILLET;
    nb = 0;

    fflush(stdout);
    printf("ENTREZ VOTRE MESSAGE :\n");
    getchar();
    const char *r2 = fgets(data, SIZE_MESS, stdin);
    if (r2 == NULL) {
        fprintf(stderr, "Erreur : EOF\n");
        exit(1);
    }

    lendata = strlen(data) - 1;

    char buf[SIZE_MESS * 2];
    header = htons((id << 5) | (codereq_client & 0x1F));
    numfil = htons(numfil);
    nb = htons(nb);

    // CONSTRUCTION DE L'ENTETE
    memcpy(buf, &header, sizeof(header));
    memcpy(buf + sizeof(header), &numfil, sizeof(numfil));
    memcpy(buf + sizeof(header) + sizeof(numfil), &nb, sizeof(nb));
    memcpy(buf + sizeof(header) + sizeof(numfil) + sizeof(nb), &lendata,
           sizeof(lendata));
    memcpy(buf + sizeof(header) + sizeof(numfil) + sizeof(nb) + sizeof(lendata),
           data, lendata);

    size_t size_buf =
        sizeof(header) + sizeof(numfil) + sizeof(nb) + sizeof(lendata) + lendata;

    // ENVOI DE LA REQUETE
    send_message(sock, buf, size_buf);

    // RECEPTION DE LA REPONSE
    recv_message(sock, buf, SIZE_MESS * 2);

    if (error_request(buf) == 1) {
        close(sock);
        exit(5);
    }

    // DECODAGE DE LA REPONSE
    memcpy(&header, buf, sizeof(header));
    memcpy(&numfil, buf + sizeof(header), sizeof(numfil));
    memcpy(&nb, buf + sizeof(header) + sizeof(numfil), sizeof(nb));
    id = ntohs(header) >> 5;
    codereq_client = ntohs(header) & 0x1F;
    numfil = ntohs(numfil);

    // AFFICHAGE DE LA REPONSE
    printf("REPONSE : CODEREQ %hd, ID %hd, NUMFIL %hd\n", codereq_client, id,
           numfil);

    close(sock);

    return 0;
}

int get_billets_request(int sock) {
    uint16_t header, id, numfil, nb;
    uint8_t codereq, lendata;

    size_t sizebuf = sizeof(uint16_t) + sizeof(uint16_t) + sizeof(uint16_t);
    char buf[sizebuf];
    memset(buf, 0, sizebuf);

    fflush(stdout);
    printf("IDENTIFIANT, NUMERO DU FIL ET NB DE BILLETS : ");
    int iduser, f, n;
    int r = scanf("%d %d %d", &iduser, &f, &n);
    if (r != 3) {
        fprintf(stderr, "Erreur : Veuillez entrer 3 valeurs.\n");
        exit(1);
    }

    if (iduser > MAX_VALUE_11BITS_INTEGER) {
        fprintf(stderr, "Erreur : Cette identifiant ne peux pas exister.\n");
        exit(1);
    }

    if (f < 0 || n < 0) {
        fprintf(stderr, "Erreur : Les valeurs doivent être supérieures ou égales à zéro.\n");
        exit(1);
    }

    id = iduser;
    numfil = f;
    nb = n;

    // CONTRUCTION ENTETE
    codereq = REQ_GET_BILLET;
    header = htons((id << 5) | (codereq & 0x1F));
    numfil = htons(numfil);
    nb = htons(nb);

    // CONSTRUCTION DU BUFFER
    char *ptr = buf;
    memcpy(ptr, &header, sizeof(header));
    ptr += sizeof(header);
    memcpy(ptr, &numfil, sizeof(numfil));
    ptr += sizeof(numfil);
    memcpy(ptr, &nb, sizeof(nb));

    // ENVOI DE LA REQUETE
    send_message(sock, buf, sizebuf);

    // RECEPTION DE LA REPONSE
    // REPONSE PEUT ÊTRE UNE ERREUR
    recv_message(sock, buf, sizebuf);

    if (error_request(buf) == 1) {
        close(sock);
        exit(5);
    }

    // DECODAGE DE LA REPONSE
    ptr = buf;
    memcpy(&header, ptr, sizeof(header));
    ptr += sizeof(header);
    memcpy(&numfil, ptr, sizeof(numfil));
    ptr += sizeof(numfil);
    memcpy(&nb, ptr, sizeof(nb));

    codereq = ntohs(header) & 0x1F;
    id = ntohs(header) >> 5;
    numfil = ntohs(numfil);
    nb = ntohs(nb);

    // AFFICHAGE DE LA REPONSE
    printf("REPONSE : CODEREQ %hd, ID %hd, NUMFIL %hd, NB %hd\n",
           codereq, id, numfil, nb);

    // RECEPTION DES BILLETS
    size_t sizebillet = sizeof(uint16_t) // numfil
                        + USERNAME_LEN * 2 // origin + pseudo
                        + sizeof(uint8_t) // datalen
                        + (SIZE_MESS + 1); // data
    char billet[sizebillet];
    char data[SIZE_MESS + 1];
    username_t pseudo_fil;
    username_t pseudo_billet;

    memset(billet, 0, sizebillet);

    int nb_billets = nb;
    for (int i = 0; i < nb_billets; i++) {
        memset(pseudo_fil, 0, USERNAME_LEN);
        memset(pseudo_billet, 0, USERNAME_LEN);
        memset(data, 0, SIZE_MESS + 1);

        recv_message(sock, billet, sizebillet);

        // DECODAGE DU BILLET
        ptr = billet;
        memcpy(&numfil, ptr, sizeof(numfil));
        ptr += sizeof(numfil);
        memcpy(pseudo_fil, ptr, USERNAME_LEN);
        ptr += strlen(ptr) + 1;
        memcpy(pseudo_billet, ptr, USERNAME_LEN);
        ptr += strlen(ptr) + 1;
        memcpy(&lendata, ptr, sizeof(lendata));
        ptr += sizeof(lendata);
        memcpy(data, ptr, strlen(ptr) + 1);
        data[lendata] = '\0';

        numfil = ntohs(numfil);

        // AFFICHAGE DU BILLET
        char buf_pseudo_fil[USERNAME_LEN + 1]; username_to_string(pseudo_fil, buf_pseudo_fil);
        char buf_pseudo_billet[USERNAME_LEN + 1]; username_to_string(pseudo_billet, buf_pseudo_billet);
        printf("BILLET %d : NUMFIL %hd, ORIGINE %s, PSEUDO %s, DATA %s\n",
                i + 1, numfil, buf_pseudo_fil, buf_pseudo_billet, data);
    }

    close(sock);

    return 0;
}

int subscribe_request(int sock){
    int iduser, f;
    uint16_t header, id, numfil, nb,addr_Multicast;
    uint8_t codereq_client, lendata;
    char data[SIZE_MESS];
    

    fflush(stdout);
    printf("Entrez l'ID et le numero de fil : ");
    scanf("%d %d",&iduser,&f);

    if (iduser > MAX_VALUE_11BITS_INTEGER) {
        perror("Erreur : Cette identifiant ne peux pas exister.");
        exit(1);
    }

    if (f < 0) {
        perror("Erreur : Ce numéro de fil ne peux pas exister.");
        exit(1);
    }

    id = iduser;
    numfil = f;

    codereq_client = REQ_SUBSCRIBE;
    nb = 0;

    fflush(stdout);
    lendata = 0;

    char buf[SIZE_MESS*4];
    header = htons((id << 5) | (codereq_client & 0x1F));
    numfil = htons(numfil);
    nb = htons(nb);

    // CONSTRUCTION DE L'ENTETE
    memcpy(buf, &header, sizeof(header));
    memcpy(buf+sizeof(header), &numfil, sizeof(numfil));
    memcpy(buf+sizeof(header)+sizeof(numfil), &nb, sizeof(nb));
    memcpy(buf+sizeof(header)+sizeof(numfil)+sizeof(nb), &lendata, sizeof(lendata));
    memcpy(buf+sizeof(header)+sizeof(numfil)+sizeof(nb)+sizeof(lendata), data, lendata);
    size_t sizebuf = sizeof(header)+sizeof(numfil)+sizeof(nb)+sizeof(lendata)+lendata;
    // ENVOI DE LA REQUETE
    send_message(sock, buf, sizebuf);
    // RECEPTION DE LA REPONSE
    recv_message(sock, buf, sizebuf);

    if (error_request(buf) == 1) {
        close(sock);
        exit(5);
    }

    memcpy(&header, buf, sizeof(header));
    memcpy(&numfil, buf+sizeof(header), sizeof(numfil));
    memcpy(&nb, buf+sizeof(header)+sizeof(numfil), sizeof(nb));
    memcpy(&addr_Multicast, buf+sizeof(header)+sizeof(numfil)+sizeof(nb), sizeof(addr_Multicast));
    id = ntohs(header) >> 5;
    codereq_client = ntohs(header) & 0x1F;
    numfil = ntohs(numfil);
    addr_Multicast = ntohs(addr_Multicast);
    // AFFICHAGE DE LA REPONSE
    printf("REPONSE : CODEREQ %hd, ID %hd, NUMFIL %hd,ADDRMLD %hd\n", codereq_client, id, numfil,addr_Multicast);

    char addr_Multicast_str[INET6_ADDRSTRLEN];
    snprintf(addr_Multicast_str, INET6_ADDRSTRLEN, "%u", addr_Multicast);
    // abonnement au groupe multicast
    struct sockaddr_in6 grsock;
    memset(&grsock, 0, sizeof(grsock));
    grsock.sin6_family = AF_INET6;
    grsock.sin6_addr = in6addr_any;
    grsock.sin6_port = htons(PORT_MD);

    int sock_udp = socket(AF_INET6, SOCK_DGRAM, 0);
    if(sock_udp < 0) 
    {
        perror("erreur socket");
        exit(1);
    }
    // pour l'envoi de messages(notifications) en multicast 
    if(bind(sock_udp, (struct sockaddr*)&grsock, sizeof(grsock))) 
    {
        perror("erreur bind");
        exit(1);
    } 
    struct ipv6_mreq mreq;
    mreq.ipv6mr_interface = 0;
    inet_pton(AF_INET6, addr_Multicast_str, &mreq.ipv6mr_multiaddr.s6_addr);
    if(setsockopt(sock_udp, IPPROTO_IPV6, IPV6_JOIN_GROUP, &mreq, sizeof(mreq)) < 0) 
    {
        perror("erreur abonnement groupe ");
    
    }

    close(sock);
    close(sock_udp);

    return 0;
}

int add_file_request(int sock) {
    uint16_t header, id, numfil, nb, numbloc;
    uint8_t codereq, lendata;
    int iduser, f;
    char data[SIZE_FILENAME];
    char filename[SIZE_FILENAME];

    fflush(stdout);
    printf("IDENTIFIANT, NUMERO DU FIL, NOM DU FICHIER : ");

    int r = scanf("%d %d %256s", &iduser, &f, filename);
    if (r != 3) {
        fprintf(stderr, "Erreur : Veuillez entrer 3 valeurs.\n");
        exit(1);
    }
    if (iduser > MAX_VALUE_11BITS_INTEGER) {
        fprintf(stderr, "Erreur : Cette identifiant ne peux pas exister.\n");
        exit(1);
    }
    if (f < 0) {
        fprintf(stderr, "Erreur : Le numéro du fil doit être supérieur ou égal à zéro.\n");
        exit(1);
    }

    // OUVERTURE DU FICHIER
    FILE *file = fopen(filename, "r");
    if (file == NULL) {
        fprintf(stderr, "Erreur : Le fichier n'existe pas.\n");
        exit(1);
    }
    // TAILLE DU FICHIER
    fseek(file, 0, SEEK_END);
    size_t filesize = ftell(file);
    fseek(file, 0, SEEK_SET);

    if (filesize > MAX_FILE_SIZE) {
        fprintf(stderr, "Erreur : Le fichier est trop volumineux.\n");
        exit(1);
    }

    size_t filename_len = strlen(filename);

    // ENTETE
    codereq = REQ_ADD_FILE;
    id = iduser;
    numfil = f;

    header = htons((id << 5) | (codereq & 0x1F));
    numfil = htons(numfil);
    nb = htons(0);
    lendata = filename_len;
    memcpy(data, filename, filename_len);
    data[filename_len] = '\0';

    size_t sizebuf = sizeof(header) + sizeof(numfil) + sizeof(nb) + sizeof(lendata) + lendata;
    char buf[sizebuf];
    memset(buf, 0, sizebuf);

    // CONSTRUCTION DU BUFFER
    char *ptr = buf;
    memcpy(ptr, &header, sizeof(header));
    ptr += sizeof(header);
    memcpy(ptr, &numfil, sizeof(numfil));
    ptr += sizeof(numfil);
    memcpy(ptr, &nb, sizeof(nb));
    ptr += sizeof(nb);
    memcpy(ptr, &lendata, sizeof(lendata));
    ptr += sizeof(lendata);
    memcpy(ptr, data, lendata);

    // ENVOI DE LA REQUETE
    send_message(sock, buf, sizebuf);

    // RECEPTION DU PORT UDP
    recv_message(sock, buf, sizebuf);

    ptr = buf;
    ptr += sizeof(header) + sizeof(numfil);
    memcpy(&nb, ptr, sizeof(nb));

    nb = ntohs(nb);

    // DECONNEXION SERVER TCP
    close(sock);

    // SOCKET CLIENT UDP
    int sock_udp = socket(AF_INET6, SOCK_DGRAM, 0);
    if (sock_udp < 0) {
        perror("Impossible de créer le socket UDP");
        exit(1);
    }

    // ADRESSE DESTINATION
    struct sockaddr_in6 addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin6_family = AF_INET6;
    addr.sin6_port = htons(nb);
    addr.sin6_addr = in6addr_any;

    // CALCULE DU NOMBRE DE PAQUETS
    int nb_paquets = 0;

    nb_paquets = filesize / SIZE_PAQUET + 1;

    // ENVOI DES PAQUETS
    size_t sizebuffer = sizeof(uint16_t)+sizeof(uint16_t)+SIZE_PAQUET;
    char buffer_udp[sizebuffer];
    memset(buffer_udp, 0, sizebuffer);

    char paquet[SIZE_PAQUET];
    memset(paquet, 0, SIZE_PAQUET);

    for (int i = 0; i < nb_paquets; i++) {
        memset(paquet, 0, SIZE_PAQUET);
        int r = fread(paquet, 1, SIZE_PAQUET, file);
        if (r < 1) {
            fprintf(stderr, "Erreur: EOF\n");
            return -1;
        }

        ptr = buffer_udp;
        memcpy(ptr, &header, sizeof(header));
        ptr += sizeof(header);
        numbloc = htons(i);
        memcpy(ptr, &numbloc, sizeof(numbloc));
        ptr += sizeof(numbloc);
        memcpy(ptr, paquet, SIZE_PAQUET);

        sendto(sock_udp, buffer_udp, sizebuffer, 0, (struct sockaddr *) &addr, sizeof(addr));
    }

    // FERMETURE DU SOCKET UDP
    close(sock_udp);

    // FERMETURE DU FICHIER
    fclose(file);

    return 0;
}

int dw_file_request(int sock) {
    uint16_t header, id, numfil, nb, numbloc;
    uint8_t codereq, lendata;
    int iduser, f;
    char filename[SIZE_FILENAME];
    char data[SIZE_FILENAME];

    fflush(stdout);
    printf("IDENTIFIANT, NUMERO DU FIL, NOM DU FICHIER : ");

    int r = scanf("%d %d %256s", &iduser, &f, filename);
    if (r != 3) {
        fprintf(stderr, "Erreur : Veuillez entrer 3 valeurs.\n");
        exit(1);
    }
    if (iduser > MAX_VALUE_11BITS_INTEGER) {
        fprintf(stderr, "Erreur : Cette identifiant ne peux pas exister.\n");
        exit(1);
    }
    if (f < 1) {
        fprintf(stderr, "Erreur : Le numéro du fil doit être supérieur ou égal à 1.\n");
        exit(1);
    }

    // UDP
    int sock_udp = socket(AF_INET6, SOCK_DGRAM, 0);
    if (sock_udp < 0) {
        perror("Erreur : Impossible de créer le socket UDP");
        exit(1);
    }

    // ADRESSE DESTINATION
    struct sockaddr_in6 addr;
    memset(&addr, 0, sizeof(addr));
    socklen_t addr_len = sizeof(addr);
    addr.sin6_family = AF_INET6;
    addr.sin6_port = htons(PORT_UDP_DW);
    addr.sin6_addr = in6addr_any;

    // BIND
    if (bind(sock_udp, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
        perror("Impossible de bind le socket UDP");
        exit(1);
    }

    // ENTETE
    codereq = REQ_DW_FILE;
    id = iduser;
    numfil = f;
    header = htons((id << 5) | (codereq & 0x1F));
    numfil = htons(numfil);
    nb = addr.sin6_port;
    lendata = strlen(filename);
    memcpy(data, filename, lendata);
    data[lendata] = '\0';

    size_t sizebuf = sizeof(header) + sizeof(numfil) + sizeof(nb) + sizeof(lendata) + lendata;
    char buf[sizebuf];
    memset(buf, 0, sizebuf);

    // CONSTRUCTION DU BUFFER
    char *ptr = buf;
    memcpy(ptr, &header, sizeof(header));
    ptr += sizeof(header);
    memcpy(ptr, &numfil, sizeof(numfil));
    ptr += sizeof(numfil);
    memcpy(ptr, &nb, sizeof(nb));
    ptr += sizeof(nb);
    memcpy(ptr, &lendata, sizeof(lendata));
    ptr += sizeof(lendata);
    memcpy(ptr, data, lendata);

    // ENVOI DE LA REQUETE
    send_message(sock, buf, sizebuf);

    //RECEPTION ACCEPTATION OU REFUS DU TRANSFERT
    int timeout = 5;
    int timeoutU = 0;
    fd_set readset;
    FD_ZERO(&readset);
    FD_SET(sock, &readset);

    struct timeval tv;
    tv.tv_sec = timeout;
    tv.tv_usec = timeoutU;

    int ready = select(sock + 1, &readset, NULL, NULL, &tv);
    if (ready == -1) {
        perror("select");
        exit(1);
    } else if (ready == 0) {
        printf("REFUS DE TRANSFERT\n");
        return 0;
    }

    recv_message(sock, buf, sizebuf);

    // LE CLIENT SE DECONNECTE DU SERVEUR
    close(sock);

    // RECEPTION DES PAQUETS UDP
    size_t size_all = MAX_FILE_SIZE;
    char *all_paquets = malloc(size_all);

    char paquet[SIZE_PAQUET];
    memset(paquet, 0, SIZE_PAQUET);

    size_t size = sizeof(uint16_t)*2 + SIZE_PAQUET;
    char buffer_udp[size];

    FD_ZERO(&readset);
    FD_SET(sock_udp, &readset);

    while (1) { 
        memset(buffer_udp, 0, size);

        ready = select(sock_udp + 1, &readset, NULL, NULL, &tv);

        if (ready == -1) {
            perror("select");
            exit(1);
        } else if (ready == 0) {
            printf("TIMEOUT UDP\n");
            exit(1);
        }

        recvfrom(sock_udp, buffer_udp, size, 0, (struct sockaddr *) &addr, &addr_len);
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

    // ON CREE LE FICHIER
    FILE *file = fopen(data, "w");
    if (file == NULL) {
        perror("Impossible de créer le fichier");
        exit(1);
    }

    // ON ECRIT DANS LE FICHIER
    fwrite(all_paquets, sizeof(char), filesize, file);

    // ON FERME LE FICHIER
    fclose(file);

    // ON FERME LE SOCKET UDP
    close(sock_udp);

    return 0;
}
