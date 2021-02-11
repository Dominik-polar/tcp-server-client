#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/stat.h>
#include <string.h>
#include <stdlib.h>
#include <sys/sendfile.h>
#include <fcntl.h>
#include <stdio.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <errno.h>

#define _POSIX_C_SOURCE 200809L

#define PART_SIZE 512
#define MAX_CLIENTS 15

int pobieranie_do_pliku(int soc, int fd)
{
    int otrzymano, otrzymano_wsumie, licznik_do_testow_usunac_potem = 0;
    char part[PART_SIZE];

    while (1)
    {
        memset(part, 0, sizeof(PART_SIZE));
        if ((otrzymano = recv(soc, part, PART_SIZE, 0)) <= 0)
        {
            if (otrzymano < 0)
            {
                perror("recv in pobieranie_do_pliku");
            }
            break;
        }
        else
        {
            otrzymano_wsumie = otrzymano_wsumie + otrzymano;
            licznik_do_testow_usunac_potem++;

            if (write(fd, part, otrzymano) == -1)
            {
                perror("blad write() in pobieranie_do_pliku");
            }

            printf("part nr.%d odebrany - %d bajtow\n", licznik_do_testow_usunac_potem, otrzymano);
        }
    }
    return otrzymano_wsumie;
};



void pobierz(int gniazdo2)
{

    struct timeval tv;
    tv.tv_sec = 3;
    tv.tv_usec = 0;



    char plik_nazwa[256];
    memset(plik_nazwa, 0, sizeof(plik_nazwa));
    int size;
    struct stat sprawdzacz;

    /*tutaj zaczyna sie pobieranie pliku wyslanego od klienta*/

    recv(gniazdo2, plik_nazwa, 256, 0);  /*pobieramy info o nazwie pliku i wkladamy do plik_nazwa*/
    recv(gniazdo2, &size, sizeof(int), 0);  /*pobieramy info o size pliku i zapisujemy do zmienej size*/

    setsockopt(gniazdo2, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof tv);
    while (stat(plik_nazwa, &sprawdzacz) == 0) //plik juz istnieje na serwerze
    {
        if (strlen(plik_nazwa) < 256)
        {
            strcat(plik_nazwa, "1");
        }
        else
        {
            printf("Zbyt duza nazwa\n");
            return;
        }
    }



    FILE* plik_fd = fopen(plik_nazwa, "wb");
    if (plik_fd == NULL)
    {
        perror("fopen() blad przy tworzeniu pliku");
    }
    int licznik_bajtow_pobranych = 0;
    licznik_bajtow_pobranych = pobieranie_do_pliku(gniazdo2, fileno(plik_fd));
    printf("%d\n", licznik_bajtow_pobranych);

    fclose(plik_fd);

};

void wyslij(int gniazdo2)
{
    char error[20] = "error", plik_nazwa[256];
    int size, plikstatus;
    struct stat plik_info;

    recv(gniazdo2, plik_nazwa, 256, 0);  /*pobieramy info o nazwie pliku i wkladamy do plik_nazwa*/


    /*ZABRONIONE NAZWY PLIKOW DO POBRANIA*/

    if (strcmp(plik_nazwa, "serwer.c") != 0 && strcmp(plik_nazwa, "serwer") != 0 && strcmp(plik_nazwa, "a.out") != 0)
    {
        plikstatus = open(plik_nazwa, O_RDONLY);   /*otwarcie pliku*/
    }
    else
    {
        plikstatus = -1;
    }

    /*---------------------------------------*/



    if (plikstatus == -1)
    {
        printf("plik o takiej nazwie nie istnieje\n");
        send(gniazdo2, error, sizeof(error), 0);
    }
    else        /*zaczynamy przesylac*/
    {

        stat(plik_nazwa, &plik_info);
        size = plik_info.st_size;
        send(gniazdo2, plik_nazwa, 256, 0);
        send(gniazdo2, &size, sizeof(int), 0);






        int sprawdzacz;
        sprawdzacz = sendfile(gniazdo2, plikstatus, NULL, size);
        if (sprawdzacz != size)
        {
            printf("sendfile wyslal %d/%d bajtow\n", sprawdzacz, size);
            perror("blad sendfile");
        }


    }
};

void ls(int gniazdo2)
{
    int plikstatus, sprawdzacz, size;
    system("ls >nazwyplikow");
    plikstatus = open("nazwyplikow", O_RDONLY);
    struct stat plik_info;
    stat("nazwyplikow", &plik_info);
    size = plik_info.st_size;
    send(gniazdo2, "nazwyplikow", 256, 0);
    send(gniazdo2, &size, sizeof(int), 0);
    sprawdzacz = sendfile(gniazdo2, plikstatus, NULL, size);
    if (sprawdzacz != size)
    {
        printf("sendfile wyslal %d/%d bajtow\n", sprawdzacz, size);
        perror("blad sendfile");
    }
};

int wybor(int gniazdo2)
{
    char komenda[2] = {};
    if (recv(gniazdo2, komenda, sizeof(komenda), 0))
    {
        if (strcmp(komenda, "w") == 0)
        {
            pobierz(gniazdo2);
            return 0;
        }

        else if (strcmp(komenda, "p") == 0)
        {
            wyslij(gniazdo2);
            return 0;
        }
        else if (strcmp(komenda, "ls") == 0)
        {
            ls(gniazdo2);
            return 0;
        }

    }


    if (strcmp(komenda, "e") == 0)
    {
        return 1;
    }

    return 2;
}

int main(void) {
    unsigned int port;
    int gniazdoserwer, nowegniazdo, gniazdaklient[MAX_CLIENTS]; /*socket*/
    int max_klientow = 15, aktywnosc;
    int max_sd;
    struct sockaddr_in serwer, klient; /*sender*/
    socklen_t dl = sizeof(struct sockaddr_in);
    fd_set readfds;
    int i = 0;

    for (i = 0; i < MAX_CLIENTS; i++)
    {
        gniazdaklient[i] = 0;
    }

    if ((gniazdoserwer = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        perror("blad socket:");
        return 1;
    }
    printf("Na ktorym porcie mam sluchac?:");
    scanf("%u", &port);
    serwer.sin_family = AF_INET;
    serwer.sin_port = htons(port);
    serwer.sin_addr.s_addr = INADDR_ANY;
    if (bind(gniazdoserwer, (struct sockaddr*)&serwer, sizeof(serwer)) < 0) {
        printf("Bind nie powiodl sie (bind failed).\n");
        return 1;
    }
    if (listen(gniazdoserwer, 10) < 0) {
        printf("Listen nie powiodl sie (listen failed).\n");
        return 1;
    }

    printf("Czekam na polaczenie (I am waiting for connection)...\n");


    while (1)
    {
        FD_ZERO(&readfds);
        FD_SET(gniazdoserwer, &readfds);
        max_sd = gniazdoserwer;

        for (i = 0; i < max_klientow; i++)
        {
            if (gniazdaklient[i] > 0)
            {
                FD_SET(gniazdaklient[i], &readfds);
            }
            if (gniazdaklient[i] > max_sd)
            {
                max_sd = gniazdaklient[i];
            }
        }


        aktywnosc = select(max_sd + 1, &readfds, NULL, NULL, NULL);
        if ((aktywnosc < 0) && (errno != EINTR))
        {
            printf("blad select");
        }

        /*laczenie nowych klientow*/

        if ((FD_ISSET(gniazdoserwer, &readfds)))
        {
            if ((nowegniazdo = accept(gniazdoserwer,
                (struct sockaddr*)&klient,
                &dl)) < 0)
            {
                perror("accept error");
                return 1;
            }

            for (i = 0; i < max_klientow; i++)
            {
                if (gniazdaklient[i] == 0)
                {
                    gniazdaklient[i] = nowegniazdo;
                    printf("dodaje do tablicy jako %d\n", i);
                    break;
                }
            }
        }


        /*przesylanie miedzy klientem a serwerem*/

        for (i = 0; i < max_klientow; i++)
        {
            if (FD_ISSET(gniazdaklient[i], &readfds))
            {
                if (wybor(gniazdaklient[i]) == 1)
                {
                    close(gniazdaklient[i]);
                    gniazdaklient[i] = 0;
                }
            }
        }
    }

    close(gniazdoserwer);
    return 0;
}



