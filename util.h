#ifndef UTILH
#define UTILH
#include "main.h"

// Typ pakietu
typedef struct {
    int ts;       // Zegar Lamporta
    int src;  
    int data; 
    int score;
} packet_t;

#define NITEMS 4

// Typy wiadomości
#define CYCLE_SYNC 1
#define CS_REQ 2
#define CS_ACK 3
#define CS_REL 4
#define INTERACT_SHOOT 5
#define GAME_OVER 6

// Role graczy
#define ROLE_VICTIM 0
#define ROLE_KILLER 1

extern MPI_Datatype MPI_PACKET_T;
void initiate_package_type();

void sendPacket(packet_t *pkt, int destination, int tag);

typedef enum {
    State_Init,
    State_StartCycle,
    State_Matchmaking,
    State_RequestGun,
    State_CriticalSection,
    State_ReleasingGun,
    State_WaitForAction,
    State_CycleFinished,
    State_FinishGame
} state_t;

extern state_t state;
extern pthread_mutex_t stateMutex;
void changeState( state_t );

// Funkcje pomocnicze do kolejki
void queue_add(int ts, int src);
void queue_remove(int src);
int queue_get_pos(int src);
void print_queue();

#endif
