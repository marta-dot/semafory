#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/msg.h>
#include <sys/ipc.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>

struct message {
    long mtype;
    char mtext[100];
};

int main(int argc, const char * argv[]) {
    int klucz, msg_key;
    struct message msg;

    //kolejka komunikatów - klucz
    msg_key = atoi(argv[1]); //1234
    klucz = msgget(msg_key, IPC_CREAT | 0666);
    msg.mtype = 1;

    printf("Hello, World!\n");

    srand(time(NULL));
    int liczba_zamowien = atoi(argv[2]);    //atoi nie wywala błędów jak podamy stringa po prostu daje 0 dla
    int max_liczba_A = atoi(argv[3]);
    int max_liczba_B = atoi(argv[4]);
    int max_liczba_C = atoi(argv[5]);

    for(int i = 0; i < liczba_zamowien; i++) {
        usleep(500000);                 // 500000 mikrosekund = 0.5 sekundy
        int id_zamowienia = i+1;                //liczymy zamówienia od 1 !!!
        int liczba_A = rand() % max_liczba_A;
        int liczba_B = rand() % max_liczba_B;
        int liczba_C = rand() % max_liczba_C;
        printf("Zlecono zlecenie %d na: A:%d B:%d C:%d\n", id_zamowienia, liczba_A, liczba_B, liczba_C);

        char combined[300];
        sprintf(combined, "%d %d %d %d", id_zamowienia, liczba_A, liczba_B, liczba_C);

        strcpy(msg.mtext, combined);

        msgsnd(klucz, &msg, sizeof(msg.mtext), 0);

    }

    int gold = 0, suma = 0;

    for (int i = 0; i <3; ++i) {
        msgrcv(klucz, &msg, sizeof(msg.mtext), 2, 0);
        sscanf(msg.mtext, "%d", &gold);
        suma += gold;
//        printf("Odebrano %d złota\n", gold);
    }

    printf("Suma wydana na zlecenia: %d GLD\n", suma);

    msgctl(klucz, IPC_RMID, NULL);


    return 0;
}
