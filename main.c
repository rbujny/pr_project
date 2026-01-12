#include "main.h"
#include "watek_glowny.h"
#include "watek_komunikacyjny.h"

int rank, size;
int ackCount = 0;
int lamport_clock = 0;
int cycle = 0;
int score = 0;

int *roles_matrix;

request_t *request_queue;
int request_queue_size = 0;

int *current_cycle_status;
int *received_ack;
int *player_scores;

pthread_mutex_t clock_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t ack_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t queue_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t cycle_mutex = PTHREAD_MUTEX_INITIALIZER;

/* Inicjalizacja zmiennych warunkowych */
pthread_cond_t cycle_cond = PTHREAD_COND_INITIALIZER;
pthread_cond_t ack_cond = PTHREAD_COND_INITIALIZER;

pthread_t commThread;

void finalize()
{
    println("Czekam na wątek \"komunikacyjny\"\n" );
    pthread_join(commThread,NULL);

    pthread_mutex_destroy(&clock_mutex);
    pthread_mutex_destroy(&ack_mutex);
    pthread_mutex_destroy(&queue_mutex);
    pthread_mutex_destroy(&cycle_mutex);
    
    pthread_cond_destroy(&cycle_cond);
    pthread_cond_destroy(&ack_cond);
    
    if (request_queue) free(request_queue);
    if (current_cycle_status) free(current_cycle_status);
    if (player_scores) free(player_scores);
    if (received_ack) free(received_ack);
    if (roles_matrix) free(roles_matrix);

    MPI_Type_free(&MPI_PACKET_T);
    MPI_Finalize();
}

void check_thread_support(int provided)
{
    switch (provided) {
        case MPI_THREAD_SINGLE: 
            printf("Brak wsparcia dla wątków, kończę\n");
            MPI_Finalize();
            exit(-1);
            break;
        case MPI_THREAD_FUNNELED: 
            printf("tylko te wątki, ktore wykonaly mpi_init_thread mogą wykonać wołania do biblioteki mpi\n");
            break;
        case MPI_THREAD_SERIALIZED: 
            printf("tylko jeden watek naraz może wykonać wołania do biblioteki MPI\n");
            break;
        case MPI_THREAD_MULTIPLE: printf("Pełne wsparcie dla wątków\n"); 
            break;
        default: printf("Nikt nic nie wie\n");
    }
}

int main(int argc, char **argv)
{
    MPI_Status status;
    int provided;
    MPI_Init_thread(&argc, &argv, MPI_THREAD_MULTIPLE, &provided);
    check_thread_support(provided);
    initiate_package_type();
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    if (rank == 0) {
        if (X_CYCLES % 2 != 0) {
            printf("UWAGA: Liczba cykli (%d) jest nieparzysta. Rozgrywka może być niezbalansowana (brak równego podziału ról).\n", X_CYCLES);
        }
        if (size % 2 != 0) {
            printf("UWAGA: Liczba procesów (%d) jest nieparzysta. Rozgrywka może być niezbalansowana (ktoś będzie pauzował).\n", size);
        }
    }

    srand(rank);

    request_queue = malloc(sizeof(request_t) * size * 2);
    request_queue_size = 0;

    current_cycle_status = malloc(sizeof(int) * size);
    memset(current_cycle_status, -1, sizeof(int) * size);
    
    player_scores = malloc(sizeof(int) * size);
    memset(player_scores, 0, sizeof(int) * size);

    received_ack = malloc(sizeof(int) * size);
    memset(received_ack, 0, sizeof(int) * size);

    /* Alokacja macierzy ról */
    roles_matrix = malloc(sizeof(int) * size * (X_CYCLES + 1)); 

    pthread_create( &commThread, NULL, startComThread , 0);
    mainLoop();
    
    finalize();
    return 0;
}