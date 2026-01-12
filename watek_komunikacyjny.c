#include "main.h"
#include "watek_komunikacyjny.h"

void *startComThread(void *ptr)
{
    MPI_Status status;
    packet_t packet;
    
    while ( state != State_FinishGame ) {
        if (MPI_Recv( &packet, 1, MPI_PACKET_T, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status) != MPI_SUCCESS) {
            debug("Błąd MPI_Recv, kończę wątek");
            break;
        }
        
        pthread_mutex_lock(&clock_mutex);
        if (packet.ts > lamport_clock) {
            lamport_clock = packet.ts;
        }
        lamport_clock++;
        pthread_mutex_unlock(&clock_mutex);

        switch ( status.MPI_TAG ) {
            case CYCLE_SYNC:
                debug("Dostałem CYCLE_SYNC od %d (cykl %d)", status.MPI_SOURCE, packet.data);
                pthread_mutex_lock(&cycle_mutex);
                current_cycle_status[status.MPI_SOURCE] = packet.data;
                player_scores[status.MPI_SOURCE] = packet.score;
                pthread_cond_broadcast(&cycle_cond);
                pthread_mutex_unlock(&cycle_mutex);
                break;

            case CS_REQ: 
                debug("Dostałem CS_REQ od %d (ts=%d)", status.MPI_SOURCE, packet.ts);
                queue_add(packet.ts, status.MPI_SOURCE);
                sendPacket(0, status.MPI_SOURCE, CS_ACK); 
                break;

            case CS_ACK: 
                debug("Dostałem CS_ACK od %d", status.MPI_SOURCE);
                pthread_mutex_lock(&ack_mutex);
                received_ack[status.MPI_SOURCE] = 1;
                ackCount++; 
                pthread_cond_broadcast(&ack_cond);
                pthread_mutex_unlock(&ack_mutex);
                break;

            case CS_REL:
                debug("Dostałem CS_REL od %d", status.MPI_SOURCE);
                queue_remove(status.MPI_SOURCE);
                pthread_cond_broadcast(&ack_cond);
                break;

            case INTERACT_SHOOT:
                debug("Dostałem SHOOT od %d", status.MPI_SOURCE);
                int outcome = packet.data;
                int points = (outcome == 0) ? 1 : 0; // Ofiara dostaje pkt za unik
                
                score += points;
                debug("Wynik starcia z %d: %d (0=Unik, 1=Traf). Pkt: +%d", status.MPI_SOURCE, outcome, points);

                pthread_mutex_lock(&cycle_mutex);
                interaction_finished = 1;
                pthread_cond_broadcast(&cycle_cond);
                pthread_mutex_unlock(&cycle_mutex);
                break;

            case GAME_OVER:
                debug("Dostałem GAME_OVER od %d", status.MPI_SOURCE);
                if (status.MPI_SOURCE == rank) {
                    return NULL;
                }
                break;

            default:
            break;
        }
    }
    return NULL;
}