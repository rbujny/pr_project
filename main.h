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

// Parametry gry
#define P_GUNS 1     
#define X_CYCLES 6

extern int rank;
extern int size;
extern int ackCount;
extern int lamport_clock;
extern int cycle;

extern int *roles_matrix; 

// Struktura żądania do sekcji krytycznej
typedef struct {
    int ts;  // Zegar Lamporta
    int src; // ID procesu
} request_t;

extern request_t *request_queue;
extern int request_queue_size;

extern int *current_cycle_status; // W którym cyklu jest dany proces?
extern int *player_scores;        // Aktualne wyniki graczy
extern int *received_ack;         // Tablica potwierdzeń
extern int score;

// Flaga synchronizująca zakończenie interakcji w parze.
extern int interaction_finished;

// Muteksy
extern pthread_mutex_t clock_mutex;
extern pthread_mutex_t ack_mutex;   
extern pthread_mutex_t queue_mutex; 
extern pthread_mutex_t cycle_mutex; 

// Zmienne warunkowe do usunięcia aktywnego czekania
extern pthread_cond_t cycle_cond;
extern pthread_cond_t ack_cond; 

extern pthread_t commThread;

// Makra debug
#ifdef DEBUG
#define debug(FORMAT,...) printf("%c[%d;%dm [%d][%d]: " FORMAT "%c[%d;%dm\n",  27, (1+(rank/7))%2, 31+(6+rank)%7, rank, lamport_clock, ##__VA_ARGS__, 27,0,37);
#else
#define debug(...) ;
#endif

#define println(FORMAT,...) { printf("%c[%d;%dm [%d][%d]: " FORMAT "%c[%d;%dm\n",  27, (1+(rank/7))%2, 31+(6+rank)%7, rank, lamport_clock, ##__VA_ARGS__, 27,0,37); fflush(stdout); }
#endif