#include "main.h"
#include "util.h"
MPI_Datatype MPI_PACKET_T;

state_t state = State_Init;
int interaction_finished = 0;

pthread_mutex_t stateMutex = PTHREAD_MUTEX_INITIALIZER;

struct tagNames_t{
    const char *name;
    int tag;
} tagNames[] = { 
    { "CYCLE_SYNC", CYCLE_SYNC }, 
    { "CS_REQ", CS_REQ }, 
    { "CS_ACK", CS_ACK }, 
    { "CS_REL", CS_REL }, 
    { "INTERACT_SHOOT", INTERACT_SHOOT }, 
    { "GAME_OVER", GAME_OVER }
};

const char *const tag2string( int tag )
{
    for (int i=0; i <sizeof(tagNames)/sizeof(struct tagNames_t);i++) {
	if ( tagNames[i].tag == tag )  return tagNames[i].name;
    }
    return "<unknown>";
}

void initiate_package_type()
{
    int blocklengths[NITEMS] = {1,1,1,1};
    MPI_Datatype types[NITEMS] = {MPI_INT, MPI_INT, MPI_INT, MPI_INT};

    MPI_Aint     offsets[NITEMS]; 
    offsets[0] = offsetof(packet_t, ts);
    offsets[1] = offsetof(packet_t, src);
    offsets[2] = offsetof(packet_t, data);
    offsets[3] = offsetof(packet_t, score);

    MPI_Type_create_struct(NITEMS, blocklengths, offsets, types, &MPI_PACKET_T);

    MPI_Type_commit(&MPI_PACKET_T);
}

void sendPacket(packet_t *pkt, int destination, int tag)
{
    int freepkt=0;
    if (pkt==0) { pkt = malloc(sizeof(packet_t)); freepkt=1;}
    pkt->src = rank;
    pthread_mutex_lock(&clock_mutex);
    lamport_clock++;
    pkt->ts = lamport_clock;
    pthread_mutex_unlock(&clock_mutex);
    MPI_Send( pkt, 1, MPI_PACKET_T, destination, tag, MPI_COMM_WORLD);
    debug("Wysyłam %s do %d\n", tag2string( tag), destination);
    if (freepkt) free(pkt);
}

void changeState( state_t newState )
{
    pthread_mutex_lock( &stateMutex );
    if (state==State_FinishGame) { 
	pthread_mutex_unlock( &stateMutex );
        return;
    }
    state = newState;
    pthread_mutex_unlock( &stateMutex );
}


int compare_requests(const void *a, const void *b) {
    request_t *reqA = (request_t *)a;
    request_t *reqB = (request_t *)b;
    if (reqA->ts != reqB->ts) {
        return reqA->ts - reqB->ts;
    }
    return reqA->src - reqB->src;
}

void queue_add(int ts, int src) {
    pthread_mutex_lock(&queue_mutex);
    for(int i=0; i<request_queue_size; i++) {
        if (request_queue[i].src == src) {
            pthread_mutex_unlock(&queue_mutex);
            return;
        }
    }
    request_queue[request_queue_size].ts = ts;
    request_queue[request_queue_size].src = src;
    request_queue_size++;
    qsort(request_queue, request_queue_size, sizeof(request_t), compare_requests);
    pthread_mutex_unlock(&queue_mutex);
}

void queue_remove(int src) {
    pthread_mutex_lock(&queue_mutex);
    int found_idx = -1;
    for (int i=0; i<request_queue_size; i++) {
        if (request_queue[i].src == src) {
            found_idx = i;
            break;
        }
    }
    if (found_idx != -1) {
        for (int i=found_idx; i<request_queue_size-1; i++) {
            request_queue[i] = request_queue[i+1];
        }
        request_queue_size--;
    }
    pthread_mutex_unlock(&queue_mutex);
}

int queue_get_pos(int src) {
    pthread_mutex_lock(&queue_mutex);
    int pos = -1;
    for (int i=0; i<request_queue_size; i++) {
        if (request_queue[i].src == src) {
            pos = i;
            break;
        }
    }
    pthread_mutex_unlock(&queue_mutex);
    return pos;
}

void print_queue() {
    // Funkcja debugująca
    #ifdef DEBUG
    pthread_mutex_lock(&queue_mutex);
    printf("Kolejka (size=%d): [", request_queue_size);
    for(int i=0; i<request_queue_size; i++) {
        printf("(%d, %d) ", request_queue[i].ts, request_queue[i].src);
    }
    printf("]\n");
    pthread_mutex_unlock(&queue_mutex);
    #endif
}
