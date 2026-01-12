#ifndef MAINH
#define MAINH
#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>

#include "util.h"
#define TRUE 1
#define FALSE 0

#define ROOT 0

/* Parametry gry */
#define P_GUNS 2       
#define X_CYCLES 10      

extern int rank;
extern int size;
extern int ackCount;
extern int lamport_clock;
extern int cycle;

/* 
 * Macierz ról
 */
extern int *roles_matrix; 

/* Struktura żądania do sekcji krytycznej*/
typedef struct {
    int ts;  // Zegar Lamporta
    int src; // ID procesu
} request_t;

extern request_t *request_queue;
extern int request_queue_size;

/* Tablice stanu globalnego */
extern int *current_cycle_status; // W którym cyklu jest dany proces?
extern int *player_scores;        // Aktualne wyniki graczy
extern int *received_ack;         // Tablica potwierdzeń
extern int score;

/* 
 * Flaga synchronizująca zakończenie interakcji w parze.
 */
extern int interaction_finished;

/* Muteksy i Zmienne Warunkowe */
extern pthread_mutex_t clock_mutex; /* Chroni dostęp do zegara logicznego */
extern pthread_mutex_t ack_mutex;   /* Chroni licznik ackCount i tablicę received_ack */
extern pthread_mutex_t queue_mutex; /* Chroni lokalną kolejkę żądań */
extern pthread_mutex_t cycle_mutex; /* Chroni dane cyklu*/

/* Zmienne warunkowe do usunięcia aktywnego czekania (pętli while) */
extern pthread_cond_t cycle_cond; // Budzi wątek główny, gdy zmieni się stan cyklu lub zakończy interakcja
extern pthread_cond_t ack_cond;   // Budzi wątek główny, gdy przyjdzie ACK lub zmieni się kolejka

extern pthread_t commThread;

/* Makra debug */

#ifdef DEBUG
#define debug(FORMAT,...) printf("%c[%d;%dm [%d][%d]: " FORMAT "%c[%d;%dm\n",  27, (1+(rank/7))%2, 31+(6+rank)%7, rank, lamport_clock, ##__VA_ARGS__, 27,0,37);
#else
#define debug(...) ;
#endif

#define println(FORMAT,...) { printf("%c[%d;%dm [%d][%d]: " FORMAT "%c[%d;%dm\n",  27, (1+(rank/7))%2, 31+(6+rank)%7, rank, lamport_clock, ##__VA_ARGS__, 27,0,37); fflush(stdout); }
#endif