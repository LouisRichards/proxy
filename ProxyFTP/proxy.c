#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include "./simpleSocketAPI.h"

#define SERVADDR "127.0.0.1" // Définition de l'adresse IP d'écoute
#define SERVPORT "0"         // Définition du port d'écoute, si 0 port choisi dynamiquement
#define LISTENLEN 1          // Taille de la file des demandes de connexion
#define MAXBUFFERLEN 1024    // Taille du tampon pour les échanges de données
#define MAXHOSTLEN 64        // Taille d'un nom de machine
#define MAXPORTLEN 64        // Taille d'un numéro de port

void fils();

int main()
{
    int ecode;                      // Code retour des fonctions
    char serverAddr[MAXHOSTLEN];    // Adresse du serveur
    char serverPort[MAXPORTLEN];    // Port du server
    int descSockRDV;                // Descripteur de socket de rendez-vous
    int descSockCOM;                // Descripteur de socket de communication
    struct addrinfo hints;          // Contrôle la fonction getaddrinfo
    struct addrinfo *res;           // Contient le résultat de la fonction getaddrinfo
    struct sockaddr_storage myinfo; // Informations sur la connexion de RDV
    struct sockaddr_storage from;   // Informations sur le client connecté
    socklen_t len;                  // Variable utilisée pour stocker les
                                    // longueurs des structures de socket
    char buffer[MAXBUFFERLEN];      // Tampon de communication entre le client et le serveur

    // Initialisation de la socket de RDV IPv4/TCP
    descSockRDV = socket(AF_INET, SOCK_STREAM, 0);
    if (descSockRDV == -1)
    {
        perror("Erreur création socket RDV\n");
        exit(2);
    }
    // Publication de la socket au niveau du système
    // Assignation d'une adresse IP et un numéro de port
    // Mise à zéro de hints
    memset(&hints, 0, sizeof(hints));
    // Initialisation de hints
    hints.ai_flags = AI_PASSIVE;     // mode serveur, nous allons utiliser la fonction bind
    hints.ai_socktype = SOCK_STREAM; // TCP
    hints.ai_family = AF_INET;       // seules les adresses IPv4 seront présentées par
                                     // la fonction getaddrinfo

    // Récupération des informations du serveur
    ecode = getaddrinfo(SERVADDR, SERVPORT, &hints, &res);
    if (ecode)
    {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(ecode));
        exit(1);
    }
    // Publication de la socket
    ecode = bind(descSockRDV, res->ai_addr, res->ai_addrlen);
    if (ecode == -1)
    {
        perror("Erreur liaison de la socket de RDV");
        exit(3);
    }
    // Nous n'avons plus besoin de cette liste chainée addrinfo
    freeaddrinfo(res);

    // Récuppération du nom de la machine et du numéro de port pour affichage à l'écran
    len = sizeof(struct sockaddr_storage);
    ecode = getsockname(descSockRDV, (struct sockaddr *)&myinfo, &len);
    if (ecode == -1)
    {
        perror("SERVEUR: getsockname");
        exit(4);
    }
    ecode = getnameinfo((struct sockaddr *)&myinfo, sizeof(myinfo), serverAddr, MAXHOSTLEN,
                        serverPort, MAXPORTLEN, NI_NUMERICHOST | NI_NUMERICSERV);
    if (ecode != 0)
    {
        fprintf(stderr, "error in getnameinfo: %s\n", gai_strerror(ecode));
        exit(4);
    }
    printf("L'adresse d'ecoute est: %s\n", serverAddr);
    printf("Le port d'ecoute est: %s\n", serverPort);

    // Definition de la taille du tampon contenant les demandes de connexion
    ecode = listen(descSockRDV, LISTENLEN);
    if (ecode == -1)
    {
        perror("Erreur initialisation buffer d'écoute");
        exit(5);
    }

    len = sizeof(struct sockaddr_storage);
    // Attente connexion du client
    // Lorsque demande de connexion, creation d'une socket de communication avec le client

    while (1)
    {
        descSockCOM = accept(descSockRDV, (struct sockaddr *)&from, &len);
        if (descSockCOM == -1)
        {
            perror("Erreur accept\n");
            exit(6);
        }
        // Echange de données avec le client connecté
        int err;
        pid_t pid;
        int rapport, numSig, status;

        // Creation d'un processus fils afin de gerer les echanges client-serveur

        pid = fork();
        switch (pid)
        {
        case -1:
            perror("Impossible de creer le fils 1");
            exit(1);

        case 0:
            fils(descSockCOM);
            exit(0);
        }
    }

    // Apres avoir creer le fils fermer la connection "descSockRDV"
    close(descSockRDV);
}

void fils(int descSockCOM)
{
    char buffer[MAXBUFFERLEN];
    int ecode;

    // Message de bienvenue du proxy au client (220)
    strcpy(buffer, "220 Bienvenue au proxy\n");
    write(descSockCOM, buffer, strlen(buffer));

    // lecture du login@server du client(descSockCOM)
    ecode = read(descSockCOM, buffer, MAXBUFFERLEN - 1);
    if (ecode == -1)
    {
        perror("probleme de lecture");
        exit(1);
    }
    buffer[ecode] = '\0';
    printf("Recu du client ftp: %s\n", buffer);

    // USER login@server decoupage du message pour separer le login et le nom du serveur

    char login[50], serverName[50];
    sscanf(buffer, "%50[^@]@%50s", login, serverName); // decoupage du message buffer recu
    strncat(login, "\n", 49);

    // affichage du login et serveur
    printf("Login %s et ServerName %s\n", login, serverName);

    // connection au serveur
    int sockServerCTRL;
    ecode = connect2Server(serverName, "21", &sockServerCTRL);

    // recoit Bienvenue (220) du serveur
    ecode = read(sockServerCTRL, buffer, MAXBUFFERLEN - 1);
    if (ecode == -1)
    {
        perror("probleme de lecture");
        exit(1);
    }
    buffer[ecode] = '\0';
    printf("Recu du serveur: %s\n", buffer);

    // envoie du login au serveur
    write(sockServerCTRL, login, strlen(login));

    // ecoute de la reponse du serveur (331)
    ecode = read(sockServerCTRL, buffer, MAXBUFFERLEN - 1);
    if (ecode == -1)
    {
        perror("probleme de lecture");
        exit(1);
    }
    buffer[ecode] = '\0';
    printf("Recu du serveur: %s\n", buffer);

    // demande au client son mdp (331)
    write(descSockCOM, buffer, strlen(buffer));

    // ecoute la reponse du client (PASS)
    ecode = read(descSockCOM, buffer, MAXBUFFERLEN - 1);
    if (ecode == -1)
    {
        perror("probleme de lecture");
        exit(1);
    }
    buffer[ecode] = '\0';
    printf("Recu du client ftp: %s\n", buffer);

    // envoie le mdp au serveur (PASS)
    write(sockServerCTRL, buffer, strlen(buffer));

    // ecoute la reponse du serveur (230)
    ecode = read(sockServerCTRL, buffer, MAXBUFFERLEN - 1);
    if (ecode == -1)
    {
        perror("probleme de lecture");
        exit(1);
    }
    buffer[ecode] = '\0';
    printf("Recu du serveur: %s\n", buffer);

    // envoie la confirmation au client (230)
    write(descSockCOM, buffer, strlen(buffer));

    // ecoute la reponse du client (SYST)
    ecode = read(descSockCOM, buffer, MAXBUFFERLEN - 1);
    if (ecode == -1)
    {
        perror("probleme de lecture");
        exit(1);
    }
    buffer[ecode] = '\0';
    printf("Recu du client ftp: %s\n", buffer);

    // envoie au serveur (SYST)
    write(sockServerCTRL, buffer, strlen(buffer));

    // ecoute la reponse du serveur (215)
    ecode = read(sockServerCTRL, buffer, MAXBUFFERLEN - 1);
    if (ecode == -1)
    {
        perror("probleme de lecture");
        exit(1);
    }
    buffer[ecode] = '\0';
    printf("Recu du serveur: %s\n", buffer);

    // envoie au client (215)
    write(descSockCOM, buffer, strlen(buffer));

    // ecoute la reponse du client
    ecode = read(descSockCOM, buffer, MAXBUFFERLEN - 1);
    if (ecode == -1)
    {
        perror("probleme de lecture");
        exit(1);
    }
    buffer[ecode] = '\0';
    printf("Recu du client ftp: %s\n", buffer);

    // decoupage du PORT
    int n1, n2, n3, n4, n5, n6;
    sscanf(buffer, "PORT %d,%d,%d,%d,%d,%d", &n1, &n2, &n3, &n4, &n5, &n6); // decoupage du message buffer recu

    // calcul de pasv
    char ipClient[50], portClient[10];
    sprintf(ipClient, "%d.%d.%d.%d", n1, n2, n3, n4);
    sprintf(portClient, "%d", n5 * 256 + n6);

    // connection avec le client
    int actif;
    ecode = connect2Server(ipClient, portClient, &actif);

    // envoie de PASV au serveur
    char command[50] = "PASV\n";
    write(sockServerCTRL, command, strlen(command));

    // ecoute du serveur
    ecode = read(sockServerCTRL, buffer, MAXBUFFERLEN - 1);
    if (ecode == -1)
    {
        perror("probleme de lecture");
        exit(1);
    }
    buffer[ecode] = '\0';
    printf("Recu du serveur: %s\n", buffer);

    // decoupage de l'ip et du port
    sscanf(buffer, "227 Entering Passive Mode (%d,%d,%d,%d,%d,%d)", &n1, &n2, &n3, &n4, &n5, &n6);

    char ipServeur[50], portServeur[10];
    sprintf(ipServeur, "%d.%d.%d.%d", n1, n2, n3, n4);
    sprintf(portServeur, "%d", n5 * 256 + n6);

    // connection au serveur
    int passif;
    ecode = connect2Server(ipServeur, portServeur, &passif);

    // envoie de 200 au client
    char message[50] = "200 OK\n";
    write(descSockCOM, message, strlen(message));

    // ecoute de LIST au client
    ecode = read(descSockCOM, buffer, MAXBUFFERLEN - 1);
    if (ecode == -1)
    {
        perror("probleme de lecture");
        exit(1);
    }
    buffer[ecode] = '\0';
    printf("Recu du client ftp: %s\n", buffer);

    // envoie au serveur (LIST)
    write(sockServerCTRL, buffer, strlen(buffer));

    // ecoute du serveur
    ecode = read(sockServerCTRL, buffer, MAXBUFFERLEN - 1);
    if (ecode == -1)
    {
        perror("probleme de lecture");
        exit(1);
    }
    buffer[ecode] = '\0';
    printf("Recu du serveur: %s\n", buffer);

    // envoie au client (150)
    write(descSockCOM, buffer, strlen(buffer));

    printf("Recu du serveur: \n");
    do // boucle afin de lire l'entierete du ls
    {
        // lecture de donnees du serveur
        ecode = read(passif, buffer, MAXBUFFERLEN - 1);
        if (ecode == -1)
        {
            perror("probleme de lecture");
            exit(1);
        }
        buffer[ecode] = '\0';
        printf("%s", buffer);

        // envoie des donnees au client
        write(actif, buffer, strlen(buffer));
    } while (read(passif, buffer, MAXBUFFERLEN - 1) != 0);

    close(actif);
    close(passif);

    // ecoute pour confirmation du transfert (226)
    ecode = read(sockServerCTRL, buffer, MAXBUFFERLEN - 1);
    if (ecode == -1)
    {
        perror("probleme de lecture");
        exit(1);
    }
    buffer[ecode] = '\0';
    printf("Recu du serveur: %s\n", buffer);

    // envoie de la confirmation du transfert au client (226)
    write(descSockCOM, buffer, strlen(buffer));

    // Fermeture de la connexion
    close(sockServerCTRL);
    close(descSockCOM);
}
