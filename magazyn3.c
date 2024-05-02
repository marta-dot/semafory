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
#include <errno.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <signal.h>

struct message {
    long mtype;
    char mtext[100];
};

static struct sembuf buf_sem;

void podnies_sem(int semid, int semnum) {
    buf_sem.sem_num = semnum;
    buf_sem.sem_op = 1;
    buf_sem.sem_flg = 0;
    semop(semid, &buf_sem, 1);
}

void opusc_sem(int semid, int semnum) {
    buf_sem.sem_num = semnum;
    buf_sem.sem_op = -1;
    buf_sem.sem_flg = 0;
    semop(semid, &buf_sem, 1);
}

int timeout = 0;

void handle_alarm(int signal) {
    timeout = 1;
}


int main(int argc, const char *argv[]) {
    printf("Magazyn 3!\n");

    int klucz, result;
    struct message msg;
    int id_zamowienia, liczba_A, liczba_B, liczba_C;

    //pobierz argumenty z pliku podanego jaki 2 argv

    int fd = open(argv[1], O_RDONLY);
    if (fd == -1) {
        printf("Nie można otworzyć pliku %s\n", argv[2]);
        return 1;
    }

    char buf[256];
    int lbajt;
    while ((lbajt = read(fd, buf, sizeof(buf) - 1)) > 0) {
        buf[lbajt] = '\0'; // Dodaj znak końca ciągu
        //printf("Pobrano z pliku %s\n", buf);
    }
    close(fd);

    int magazyn_A, magazyn_B, magazyn_C;
    int cena_A, cena_B, cena_C, gold = 0;

    if (sscanf(buf, "A* %d %d B* %d %d C* %d %d", &magazyn_A, &cena_A, &magazyn_B, &cena_B, &magazyn_C, &cena_C) == 6) {
        printf("Pobrano ilości i ceny z buf.\n");
    } else {
        printf("Nie udało się odczytać ilości i ceny z buf.\n");
    }

    sprintf(buf, "%d %d %d %d", magazyn_A, magazyn_B, magazyn_C, gold);

    // pamięć współdzielona dla komunikacji kurier-magazyn

    key_t key3 = ftok("magazyn3_konf.txt", 85);
    int shmid3 = shmget(key3, 256, 0666 | IPC_CREAT);
    char *data3 = shmat(shmid3, (void *) 0, 0);
    strncpy(data3, buf, 256);

    //semafory
    int semid = semget(IPC_PRIVATE, 1, IPC_CREAT | 0666);
    semctl(semid, 0, SETVAL, 1);

    //signal
    signal(SIGALRM, handle_alarm);


    //kolejka komunikatów - klucz
    int msg_key = atoi(argv[2]); //1234
    klucz = msgget(msg_key, IPC_CREAT | 0666);

    if (fork() == 0) { //POTOMNY
        while (1) {
            alarm(30);
            result = msgrcv(klucz, &msg, sizeof(msg.mtext), 1, 0);
            alarm(0);

            if (timeout) {
                printf("Kurier 1, Nie ma zleceń do przyjęcia.\n");
                exit(0);
            } else if (result == -1) {
                if (errno == ENOMSG) { // błąd jeżeli kolejka jest pusta
                    sleep(1); // Czekaj na nowe wiadomości
                    continue;
                } else {
                    break; // Zakończ pętlę, jeżeli wystąpił inny błąd
                }
            }

            sscanf(msg.mtext, "%d %d %d %d", &id_zamowienia, &liczba_A, &liczba_B,
                   &liczba_C); //odczytaj zamówienie z kolejki
            opusc_sem(semid, 0); //semafor
            sscanf(data3, "%d %d %d %d", &magazyn_A, &magazyn_B, &magazyn_C,
                   &gold); //odczytaj ilości z pamięci współdzielonej

            if (liczba_A <= magazyn_A && liczba_B <= magazyn_B && liczba_C <= magazyn_C) {
                printf("Kurier 1, Odebrał zamówienie nr %d: %d %d %d\n", id_zamowienia, liczba_A, liczba_B, liczba_C);
                magazyn_A -= liczba_A;
                magazyn_B -= liczba_B;
                magazyn_C -= liczba_C;
                gold += liczba_A * cena_A + liczba_B * cena_B + liczba_C * cena_C;
                printf("Pobrano zamówienie %d. Cena %d\n", id_zamowienia,
                       liczba_A * cena_A + liczba_B * cena_B + liczba_C * cena_C);
            } else {
                printf("Kurier 1, Odebrał zamówienie nr %d: %d %d %d\n", id_zamowienia, liczba_A, liczba_B, liczba_C);
                printf("Magazyn 3, Nie ma wystarczającej ilości towaru.\n");
                podnies_sem(semid, 0); //semafor
                exit(0);
            }

            sprintf(buf, "%d %d %d %d", magazyn_A, magazyn_B, magazyn_C, gold); //zapisz ilości do bufora
            strncpy(data3, buf, 256); //zapisz ilości do pamięci współdzielonej
            podnies_sem(semid, 0); //semafor

        }

    } else {//MACIERZYSTY
        if (fork() == 0) { // POTOMNY
            while (1) {
                alarm(30);
                result = msgrcv(klucz, &msg, sizeof(msg.mtext), 1, 0);
                alarm(0);

                if (timeout) {
                    printf("Kurier 2, Nie ma zleceń do przyjęcia.\n");
                    exit(0);
                } else if (result == -1) {
                    if (errno == ENOMSG) { // błąd jeżeli kolejka jest pusta
                        sleep(1); // Czekaj na nowe wiadomości
                        continue;
                    } else {
                        break; // Zakończ pętlę, jeżeli wystąpił inny błąd
                    }
                }

                sscanf(msg.mtext, "%d %d %d %d", &id_zamowienia, &liczba_A, &liczba_B,
                       &liczba_C); //odczytaj zamówienie z kolejki
                opusc_sem(semid, 0); //semafor
                sscanf(data3, "%d %d %d %d", &magazyn_A, &magazyn_B, &magazyn_C,
                       &gold); //odczytaj ilości z pamięci współdzielonej

                if (liczba_A <= magazyn_A && liczba_B <= magazyn_B && liczba_C <= magazyn_C) {
                    printf("Kurier 2, Odebrał zamówienie nr %d: %d %d %d\n", id_zamowienia, liczba_A, liczba_B,
                           liczba_C);
                    magazyn_A -= liczba_A;
                    magazyn_B -= liczba_B;
                    magazyn_C -= liczba_C;
                    gold += liczba_A * cena_A + liczba_B * cena_B + liczba_C * cena_C;
                    printf("Pobrano zamówienie %d. Cena %d\n", id_zamowienia,
                           liczba_A * cena_A + liczba_B * cena_B + liczba_C * cena_C);
                } else {
                    printf("Kurier 2, Odebrał zamówienie nr %d: %d %d %d\n", id_zamowienia, liczba_A, liczba_B,
                           liczba_C);
                    printf("Magazyn 3, Nie ma wystarczającej ilości towaru.\n");
                    podnies_sem(semid, 0); //semafor
                    exit(0);
                }

                sprintf(buf, "%d %d %d %d", magazyn_A, magazyn_B, magazyn_C, gold); //zapisz ilości do bufora
                strncpy(data3, buf, 256); //zapisz ilości do pamięci współdzielonej
                podnies_sem(semid, 0); //semafor

            }

        } else {//MACIERZYSTY
            if (fork() == 0) { // POTOMNY
                while (1) {
                    alarm(30);
                    result = msgrcv(klucz, &msg, sizeof(msg.mtext), 1, 0);
                    alarm(0);

                    if (timeout) {
                        printf("Kurier 3, Nie ma zleceń do przyjęcia\n");
                        exit(0);
                    } else if (result == -1) {
                        if (errno == ENOMSG) { // błąd jeżeli kolejka jest pusta
                            sleep(1); // Czekaj na nowe wiadomości
                            continue;
                        } else {
                            break; // Zakończ pętlę, jeżeli wystąpił inny błąd
                        }
                    }

                    sscanf(msg.mtext, "%d %d %d %d", &id_zamowienia, &liczba_A, &liczba_B,
                           &liczba_C); //odczytaj zamówienie z kolejki
                    opusc_sem(semid, 0); //semafor
                    sscanf(data3, "%d %d %d %d", &magazyn_A, &magazyn_B, &magazyn_C,
                           &gold); //odczytaj ilości z pamięci współdzielonej

                    if (liczba_A <= magazyn_A && liczba_B <= magazyn_B && liczba_C <= magazyn_C) {
                        printf("Kurier 3, Odebrał zamówienie nr %d: %d %d %d\n", id_zamowienia, liczba_A, liczba_B,
                               liczba_C);
                        magazyn_A -= liczba_A;
                        magazyn_B -= liczba_B;
                        magazyn_C -= liczba_C;
                        gold += liczba_A * cena_A + liczba_B * cena_B + liczba_C * cena_C;
                        printf("Pobrano zamówienie %d. Cena %d\n", id_zamowienia,
                               liczba_A * cena_A + liczba_B * cena_B + liczba_C * cena_C);
                    } else {
                        printf("Kurier 3, Odebrał zamówienie nr %d: %d %d %d\n", id_zamowienia, liczba_A, liczba_B,
                               liczba_C);
                        printf("Magazyn 3, Nie ma wystarczającej ilości towaru.\n");
                        podnies_sem(semid, 0); //semafor
                        exit(0);

                    }

                    sprintf(buf, "%d %d %d %d", magazyn_A, magazyn_B, magazyn_C, gold); //zapisz ilości do bufora
                    strncpy(data3, buf, 256); //zapisz ilości do pamięci współdzielonej
                    podnies_sem(semid, 0); //semafor

                }

            } else {
                for (int i = 0; i < 3; i++) {
                    wait(NULL);
                }

                sscanf(data3, "%d %d %d %d", &magazyn_A, &magazyn_B, &magazyn_C,
                       &gold); //odczytaj ilości z pamięci współdzielonej

                printf("Stan magazynu 3 : A:%d B:%d C:%d\n", magazyn_A, magazyn_B, magazyn_C);
                printf("Zarobiony GLD: %d.\n", gold);


                msg.mtype = 2; //zwrotna
                sprintf(msg.mtext, "%d", gold);
                msgsnd(klucz, &msg, sizeof(msg.mtext), 0);

            }

        }

    }

    return 0;
}
