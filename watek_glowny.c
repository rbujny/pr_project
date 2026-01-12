#include "main.h"
#include "watek_glowny.h"

/* Pomocnicze */
typedef struct {
    int id;
    int score;
} player_info_t;

int compare_players(const void *a, const void *b) {
    player_info_t *pA = (player_info_t *)a;
    player_info_t *pB = (player_info_t *)b;
    if (pA->score != pB->score) return pB->score - pA->score; // Malejąco po wyniku
    return pA->id - pB->id; // Rosnąco po ID
}


void generate_matrix() {
    
    // Tylko ROOT może generować
    if (rank == ROOT) {
        println("ROOT: Generuję macierz...");
        srand(time(NULL)); 

        int *base = malloc(sizeof(int) * X_CYCLES);
        for (int k = 0; k < X_CYCLES; k++) base[k] = (k < X_CYCLES/2) ? 1 : 0;

        // Przypisujemy wzorce z przesunięciem, żeby wykorzystać przestrzeń kombinacji
        for (int i = 0; i < size; i++) {
            int shift = i % X_CYCLES; 
            
            if (i % 2 == 0) {
                // Gracz parzysty: Wzorzec z przesunięciem
                for (int c = 1; c <= X_CYCLES; c++) {
                    int idx = (c - 1 + (i/2)) % X_CYCLES;
                    roles_matrix[i * (X_CYCLES + 1) + c] = base[idx];
                }
            } else {
                // Gracz nieparzysty: Dokładny mirror poprzednika
                for (int c = 1; c <= X_CYCLES; c++) {
                    int partner_val = roles_matrix[(i-1) * (X_CYCLES + 1) + c];
                    roles_matrix[i * (X_CYCLES + 1) + c] = (partner_val == 1) ? 0 : 1;
                }
            }
        }
        free(base);

        // Tasowanie kolumn
        for (int k = 0; k < X_CYCLES * 3; k++) {
            int c1 = 1 + rand() % X_CYCLES;
            int c2 = 1 + rand() % X_CYCLES;
            if (c1 == c2) continue;

            for (int i = 0; i < size; i++) {
                int temp = roles_matrix[i * (X_CYCLES + 1) + c1];
                roles_matrix[i * (X_CYCLES + 1) + c1] = roles_matrix[i * (X_CYCLES + 1) + c2];
                roles_matrix[i * (X_CYCLES + 1) + c2] = temp;
            }
        }

        // Tasowanie wierszy
        for (int k = 0; k < size * 3; k++) {
            int r1 = rand() % size;
            int r2 = rand() % size;
            if (r1 == r2) continue;

            for (int c = 1; c <= X_CYCLES; c++) {
                int temp = roles_matrix[r1 * (X_CYCLES + 1) + c];
                roles_matrix[r1 * (X_CYCLES + 1) + c] = roles_matrix[r2 * (X_CYCLES + 1) + c];
                roles_matrix[r2 * (X_CYCLES + 1) + c] = temp;
            }
        }

        printf("\n   === MACIERZ RÓL ===\n");
        printf("   Gracz |");
        for(int c=1; c<=X_CYCLES; c++) printf(" C%d |", c);
        printf("\n");
        for (int i = 0; i < size; i++) {
            printf("   ID %d  |", i);
            int k_count = 0;
            for (int c = 1; c <= X_CYCLES; c++) {
                int role = roles_matrix[i * (X_CYCLES + 1) + c];
                if(role == ROLE_KILLER) k_count++;
                printf(" %s |", (role == ROLE_KILLER ? "Z " : "O "));
            }
            printf("\n");
        }
        printf("   =========================================\n\n");
        fflush(stdout);
    }

    int buffer_size = size * (X_CYCLES + 1);
    MPI_Bcast(roles_matrix, buffer_size, MPI_INT, ROOT, MPI_COMM_WORLD);
}

void mainLoop()
{
    changeState(State_Init);
    
    // 1. Synchronizacja startu - czekamy na wszystkich graczy
    MPI_Barrier(MPI_COMM_WORLD);

    println("Wszyscy gracze obecni. Generuję macierz.");
    generate_matrix();

    while (1) {
        changeState(State_StartCycle);
        cycle++;
        if (cycle > X_CYCLES) {
            break;
        }

        println("=== CYKL %d ===", cycle);

        // Reset kolejki na początku nowego cyklu
        pthread_mutex_lock(&queue_mutex);
        request_queue_size = 0;
        pthread_mutex_unlock(&queue_mutex);

        // Reset flagi interakcji
        pthread_mutex_lock(&cycle_mutex);
        interaction_finished = 0;
        pthread_mutex_unlock(&cycle_mutex);
        
        
        // Reset statusów cyklu
        pthread_mutex_lock(&ack_mutex);
        memset(received_ack, 0, sizeof(int)*size);
        ackCount = 0;
        pthread_mutex_unlock(&ack_mutex);

        // Rozgłoś start i wynik
        packet_t *pkt = malloc(sizeof(packet_t));
        pkt->data = cycle;
        pkt->score = score;
        for(int i=0; i<size; i++) {
            if (i!=rank) sendPacket(pkt, i, CYCLE_SYNC);
        }
        free(pkt);
        
        // Zapisz swój stan lokalnie
        pthread_mutex_lock(&cycle_mutex);
        current_cycle_status[rank] = cycle;
        player_scores[rank] = score;
        pthread_mutex_unlock(&cycle_mutex);

        // Oczekiwanie na wszystkie procesy
        pthread_mutex_lock(&cycle_mutex);
        while(1) {
            int ready = 1;
            for(int i=0; i<size; i++) {
                if (current_cycle_status[i] < cycle) {
                    ready = 0;
                    break;
                }
            }
            if(ready) break;
            pthread_cond_wait(&cycle_cond, &cycle_mutex);
        }
        pthread_mutex_unlock(&cycle_mutex);

        // Matchmaking
        changeState(State_Matchmaking);
        
        int my_role = roles_matrix[rank * (X_CYCLES + 1) + cycle];
        
        player_info_t *killers = malloc(sizeof(player_info_t) * size);
        player_info_t *victims = malloc(sizeof(player_info_t) * size);
        int k_count = 0, v_count = 0;

        pthread_mutex_lock(&cycle_mutex);
        for(int i=0; i<size; i++) {
            int role = roles_matrix[i * (X_CYCLES + 1) + cycle];
            if (role == ROLE_KILLER) {
                killers[k_count].id = i;
                killers[k_count].score = player_scores[i];
                k_count++;
            } else {
                victims[v_count].id = i;
                victims[v_count].score = player_scores[i];
                v_count++;
            }
        }
        pthread_mutex_unlock(&cycle_mutex);

        qsort(killers, k_count, sizeof(player_info_t), compare_players);
        qsort(victims, v_count, sizeof(player_info_t), compare_players);

        int partner = -1;
        int my_idx = -1;

        if (my_role == ROLE_KILLER) {
            for(int i=0; i<k_count; i++) if(killers[i].id == rank) my_idx = i;
            if(my_idx < v_count) partner = victims[my_idx].id;
        } else {
            for(int i=0; i<v_count; i++) if(victims[i].id == rank) my_idx = i;
            if(my_idx < k_count) partner = killers[my_idx].id;
        }

        free(killers);
        free(victims);

        println("Rola: %s, Partner: %d", (my_role==ROLE_KILLER?"ZABÓJCA":"OFIARA"), partner);

        if (partner == -1) {
            changeState(State_CycleFinished);
        } else {
            if (my_role == ROLE_VICTIM) changeState(State_WaitForAction);
            else changeState(State_RequestGun);
        }

        // Logika gry
        if (state == State_RequestGun) {

            // Aktualizacja zegaru Lamporta
            pthread_mutex_lock(&clock_mutex);
            lamport_clock++;
            int my_req_ts = lamport_clock;
            pthread_mutex_unlock(&clock_mutex);
            
            queue_add(my_req_ts, rank);
            
            packet_t *req_pkt = malloc(sizeof(packet_t));
            req_pkt->ts = my_req_ts; 
            req_pkt->src = rank;

            for(int i=0; i<size; i++) {
                if (i!=rank) {
                    pthread_mutex_lock(&clock_mutex);
                    lamport_clock++;
                    pthread_mutex_unlock(&clock_mutex);
                    MPI_Send(req_pkt, 1, MPI_PACKET_T, i, CS_REQ, MPI_COMM_WORLD);
                }
            }
            free(req_pkt);

            /* 
             * Oczekwianie na dostęp do sekcji krytycznej
             */
            pthread_mutex_lock(&ack_mutex);
            while (1) {
                int pos = queue_get_pos(rank);
                if (ackCount == size - 1 && pos != -1 && pos < P_GUNS) {
                    break;
                }
                pthread_cond_wait(&ack_cond, &ack_mutex);
            }
            pthread_mutex_unlock(&ack_mutex);

            changeState(State_CriticalSection);
            println("Mam broń! Strzelam do %d", partner);
                    
            int outcome = (rank ^ partner) & 1;
            score += outcome; 
            
            // Prześlij wynik do ofiary (ofiara czeka w stanie WaitForAction)
            packet_t *shoot_pkt = malloc(sizeof(packet_t));
            shoot_pkt->data = outcome; 
            sendPacket(shoot_pkt, partner, INTERACT_SHOOT);
            free(shoot_pkt);
            
            println("Wynik: %d (1=Traf). Razem: %d", outcome, score);
            
            sleep(1); // Dla lepszej symulacji

            changeState(State_ReleasingGun);
            queue_remove(rank);
            
            for(int i=0; i<size; i++) {
                if(i!=rank) sendPacket(0, i, CS_REL);
            }
            changeState(State_CycleFinished);

        } else if (state == State_WaitForAction) {

             // Ofiara czeka na starzł od zabójcy (interaction_finished)
            pthread_mutex_lock(&cycle_mutex);
            while (interaction_finished != 1) {
                pthread_cond_wait(&cycle_cond, &cycle_mutex);
            }
            pthread_mutex_unlock(&cycle_mutex);
            
            println("Interakcja zakończona.");
            changeState(State_CycleFinished);
        }

        println("Koniec cyklu %d", cycle);
    }

    // Oczekiwanie na wszystkie wyniki
    MPI_Barrier(MPI_COMM_WORLD);

    // Koniec gry
    changeState(State_FinishGame);
    sendPacket(0, rank, GAME_OVER);
    
    int *final_scores = NULL;
    if (rank == ROOT) {
        final_scores = malloc(sizeof(int) * size);
    }
    
    // Zebranie wszystkich wyników
    MPI_Gather(&score, 1, MPI_INT, final_scores, 1, MPI_INT, ROOT, MPI_COMM_WORLD);

    if (rank == 0) {
        println("=== FINAL RANKING ===");
        
        player_info_t *final_ranking = malloc(sizeof(player_info_t) * size);
        for(int i=0; i<size; i++) {
            final_ranking[i].id = i;
            final_ranking[i].score = final_scores[i];
        }

        // Sortowanie
        qsort(final_ranking, size, sizeof(player_info_t), compare_players);

        for (int i = 0; i < size; i++) {
            printf("   MIEJSCE %d: Gracz %d  ---  Wynik: %d\n", i + 1, final_ranking[i].id, final_ranking[i].score);
        }
        printf("=====================\n");
        free(final_ranking);
        free(final_scores);
    }

    // Bariera estetyczna
    MPI_Barrier(MPI_COMM_WORLD);
}