#include <iostream>
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

#include <fstream>

#define MAX_CLIENTS 15
#define PART_SIZE 512
using namespace std;

class Klient
{
public:
    char abcd[512];
    struct sockaddr_in adr;
    unsigned int port;
    int gniazdo;


    int plikstatus, rozmiar;
    struct stat plik_info;
    char komenda[2], plik_nazwa[256];



    void polacz()
    {
        printf("\x1B[32mPodaj adres IP serwera: \033[0m");
        scanf("%s", abcd);
        printf("\x1B[32mPodaj numer portu serwera: \033[0m");
        scanf("%u", &port);
        gniazdo = socket(AF_INET, SOCK_STREAM, 0);
        adr.sin_family = AF_INET;
        adr.sin_port = htons(port);
        adr.sin_addr.s_addr = inet_addr(abcd);
        if (connect(gniazdo, (struct sockaddr*)&adr, sizeof(adr)) < 0)
        {
            printf("\x1B[31mNawiazanie polaczenia nie powiodlo sie...\033[0m\n");
            exit(EXIT_FAILURE);
        }
        printf("Polaczenie nawiazane.\n ");
    }

    void wybierz()
    {
        memset(komenda, 0, 2);
        memset(plik_nazwa, 0, 256);

        puts("Podaj: \x1B[33mp - pobieranie\033[0m | \x1B[36mw - wyslanie\033[0m |  \x1B[35mls - lista plikow na serwerze\033[0m  |  \x1B[31me - zamkniecie programu\033[0m");
        scanf("%s", komenda);
        if (strcmp(komenda, "w") == 0)
        {
            wyslij();
        }
        else if (strcmp(komenda, "p") == 0)
        {
            pobierz();
        }
        else if (strcmp(komenda, "ls") == 0)
        {
            odswiez();
        }
        else if (strcmp(komenda, "e") == 0)
        {
            zamknij();
        }
        else
        {
            puts("zla komenda\n");
        }

    }

private:


    int pobieranie_do_pliku(int soc, int fd)
    {
        int otrzymano, otrzymano_wsumie;
        char part[PART_SIZE];


        struct timeval tv;
        tv.tv_sec = 1;
        tv.tv_usec = 0;
        setsockopt(gniazdo, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof tv);


        while (1)
        {
            memset(part, 0, sizeof(PART_SIZE));
            if ((otrzymano = recv(soc, part, PART_SIZE, 0)) <= 0)
            {
                printf("\033c");
                perror("recv in pobieranie_do_pliku");
                break;
            }
            else
            {
                otrzymano_wsumie = otrzymano_wsumie + otrzymano;


                if (write(fd, part, otrzymano) == -1)
                {
                    perror("blad write() in pobieranie_do_pliku");
                }


            }
        }
        return otrzymano_wsumie;
    }

    void zamknij()
    {
        send(gniazdo, komenda, sizeof(komenda), 0);
        puts("\x1B[31mZamkniecie programu\033[0m");
        if (close(gniazdo) == -1)
        {
            perror("Blad close(gniazdo)");
        }
        exit(EXIT_SUCCESS);
    }

    void wyslij()
    {

        /*---------------------------------------------------------------*/
        /*wysylanie pliku----------------------------------------------*/


        printf("\033[0m | \x1B[36mPodaj nazwe pliku do wyslania: \033[0m");
        fflush(stdout); fgetc(stdin);
        if (fgets(plik_nazwa, sizeof(plik_nazwa), stdin) == NULL)
        {
            printf("Blad fgets()");
            return;
        }
        plik_nazwa[strcspn(plik_nazwa, "\n")] = '\0';
        /*printf("%s\n", plik_nazwa);*/

        printf("\033c");


        plikstatus = open(plik_nazwa, O_RDONLY);   /*otwarcie pliku*/
        if (plikstatus == -1)
        {
            printf("\033c");
            printf("plik o takiej nazwie nie istnieje\n");
            return;
        }
        else        /*zaczynamy przesylac*/
        {
            send(gniazdo, komenda, sizeof(komenda), 0);
            stat(plik_nazwa, &plik_info);
            rozmiar = plik_info.st_size;
            send(gniazdo, plik_nazwa, sizeof(plik_nazwa), 0);
            send(gniazdo, &rozmiar, sizeof(int), 0);

            /*stary kod*/

            int sprawdzacz;
            sprawdzacz = sendfile(gniazdo, plikstatus, NULL, rozmiar);
            if (sprawdzacz != rozmiar)
            {
                printf("sendfile wyslal %d/%d bajtow\n", sprawdzacz, rozmiar);
            }
            else
            {
                printf("\x1B[36mPlik o nazwie: %s zostal wyslany\033[0m\n", plik_nazwa);
            }
        }
    }

    void pobierz()
    {
        /*---------------------------------------------------------------*/
        /*pobieranie pliku----------------------------------------------*/


        printf("\x1B[33mPodaj nazwe pliku do pobrania: \033[0m");
        fflush(stdout); fgetc(stdin);
        if (fgets(plik_nazwa, 256, stdin) == NULL)
        {
            printf("Blad fgets()");
            return;
        }
        plik_nazwa[strcspn(plik_nazwa, "\n")] = '\0';

        send(gniazdo, komenda, sizeof(komenda), 0);
        send(gniazdo, plik_nazwa, sizeof(plik_nazwa), 0); /*wysylam nazwe pliku na serwer, ktory chce pobrac*/


        recv(gniazdo, plik_nazwa, 256, 0); /*pobieramy info o nazwie pliku ew error i wkladamy do plik_nazwa*/

        if (strcmp(plik_nazwa, "error") == 0)
        {
            puts("Blad pobierania, upewnij sie ze chcesz pobrac prawidlowy plik");
            return;
        }

        recv(gniazdo, &rozmiar, sizeof(int), 0);  /*pobieramy info o size pliku i zapisujemy do zmienej size*/

        FILE* plik_fd = fopen(plik_nazwa, "wb");
        if (plik_fd == NULL)
        {
            perror("fopen() blad przy tworzeniu pliku");
            return;
        }
        int licznik_bajtow_pobranych = 0;
        licznik_bajtow_pobranych = pobieranie_do_pliku(gniazdo, fileno(plik_fd));
        printf("\x1B[33mPlik o nazwie: %s zostal pobrany\033[0m\n", plik_nazwa);
    }
    void odswiez()
    {


        send(gniazdo, komenda, sizeof(komenda), 0);
        recv(gniazdo, plik_nazwa, 256, 0); /*pobieramy info o nazwie pliku ew error i wkladamy do plik_nazwa*/


        printf("\033c");


        if (strcmp(plik_nazwa, "error") == 0)
        {
            puts("Blad pobierania, upewnij sie czy pobierasz prawidlowy plik");
            return;
        }

        recv(gniazdo, &rozmiar, sizeof(int), 0);  /*pobieramy info o size pliku i zapisujemy do zmienej size*/

        FILE* plik_fd = fopen(plik_nazwa, "wb");
        if (plik_fd == NULL)
        {
            perror("fopen() blad przy tworzeniu pliku");
            return;
        }
        int licznik_bajtow_pobranych = 0;
        licznik_bajtow_pobranych = pobieranie_do_pliku(gniazdo, fileno(plik_fd));

        close(fileno(plik_fd));
        printf("\x1B[35mLista plikow na serwerze:\033[0m  \n");
        wyswietl_plik(plik_nazwa);
    }


    void wyswietl_plik(char* sciezka)
    {
        string tekst;
        ifstream Plik(sciezka);
        while (getline(Plik, tekst))
        {
            cout << tekst << endl;
        }
        Plik.close();
    }
};


int main()
{
    Klient klient;
    klient.polacz();
    while (1)
    {
        klient.wybierz();
    }
    return 0;

};