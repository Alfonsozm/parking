#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>

typedef enum {
    unoccupied,
    occupied,
    reserved
} spot_status_t;

typedef struct {
    int id;
    spot_status_t spot_status;
} spot_t;

typedef struct {
    int size;
    spot_t *spots;
} parking_level_t;

typedef struct {
    int size;
    parking_level_t *levels;
    int spots_available;
} parking_t;

pthread_mutex_t parking_mutex;
pthread_cond_t spot_becoming_available;
pthread_cond_t truck_check;
parking_t parking;

_Noreturn void *car(void *args);

_Noreturn void *truck(void *args);

void print_parking();

char spot_status_tToChar(spot_status_t spotStatus);

_Noreturn void init_parking(int levels, int spotsPerLevel, int carAmount, int truckAmount);

int *search_empty_spot();

int *search_two_empty_spots();

int main(int argc, char *argv[]) {
    srand(time(NULL));
    int levels, spotsPerLevel, carAmount, truckAmount;
    switch (argc) {
        case 3:
            levels = (int) strtol(argv[1], NULL, 10);
            spotsPerLevel = (int) strtol(argv[2], NULL, 10);
            carAmount = levels * spotsPerLevel * 2;
            truckAmount = 0;
            break;
        case 4:
            levels = (int) strtol(argv[1], NULL, 10);
            spotsPerLevel = (int) strtol(argv[2], NULL, 10);
            carAmount = (int) strtol(argv[3], NULL, 10);
            truckAmount = 0;
            break;
        case 5:
            levels = (int) strtol(argv[1], NULL, 10);
            spotsPerLevel = (int) strtol(argv[2], NULL, 10);
            carAmount = (int) strtol(argv[3], NULL, 10);
            truckAmount = (int) strtol(argv[4], NULL, 10);
            break;
        default:
            levels = 2;
            spotsPerLevel = 10;
            carAmount = 20;
            truckAmount = 20;
    }
    init_parking(levels, spotsPerLevel, carAmount, truckAmount);
}

_Noreturn void *car(void *args) {
    int id = *((int *) args);
    printf("Car: %03d\n", id);
    while (1) {
        sleep(rand() % 5 + 5);
        pthread_mutex_lock(&parking_mutex);
        int *aux = search_empty_spot();
        while (parking.spots_available <= 0 || aux == NULL) {
            pthread_cond_wait(&spot_becoming_available, &parking_mutex);
            aux = search_empty_spot();
        }

        spot_t *spot = &parking.levels[aux[0]].spots[aux[1]];
        spot->spot_status = occupied;
        spot->id = id;
        parking.spots_available--;
        if (parking.spots_available > 0) {
            pthread_cond_signal(&spot_becoming_available);
        }
        printf("ENTRADA: Coche %03d aparca en planta %d, plaza %d. Plazas libres: %d.\n", id, aux[0], aux[1],parking.spots_available);
        print_parking();
        pthread_mutex_unlock(&parking_mutex);

        sleep(rand() % 5 + 5);

        pthread_mutex_lock(&parking_mutex);
        spot->id = 0;
        if (spot->spot_status == occupied) {
            spot->spot_status = unoccupied;
        }
        parking.spots_available++;
        printf("SALIDA: Coche %03d saliendo. Plazas libres: %d.\n", id, parking.spots_available);
        print_parking();
        pthread_cond_broadcast(&truck_check);
        if (spot->spot_status == unoccupied) {
            pthread_cond_signal(&spot_becoming_available);
        }
        pthread_mutex_unlock(&parking_mutex);

        free(aux);
    }
}

_Noreturn void *truck(void *args) {
    int id = *((int *) args);
    printf("Truck: %03d\n", id);
    sleep(rand() % 5 + 5);
    while (1) {

        pthread_mutex_lock(&parking_mutex);
        while (parking.spots_available <= 0) {
            pthread_cond_wait(&spot_becoming_available, &parking_mutex);
        }

        int *aux = search_two_empty_spots();
        spot_t *spot1, *spot2;
        if (aux == NULL) {
            aux = search_empty_spot();
            if (aux == NULL) {
                pthread_mutex_unlock(&parking_mutex);
                continue;
            }
            parking.levels[aux[0]].spots[aux[1]].spot_status = reserved;
            int check = 1;
            while (check) {
                pthread_cond_wait(&truck_check, &parking_mutex);
                if (aux[1] != 0) {
                    parking.levels[aux[0]].spots[aux[1] - 1].spot_status = reserved;
                }
                if (aux[1] != parking.levels[0].size - 1) {
                    parking.levels[aux[0]].spots[aux[1] + 1].spot_status = reserved;
                }
                if (aux[1] != 0 && parking.levels[aux[0]].spots[aux[1] - 1].id == 0) {
                    if (aux[1] != parking.levels[0].size - 1) {
                        if (parking.levels[aux[0]].spots[aux[1] + 1].id == 0) {
                            parking.levels[aux[0]].spots[aux[1] + 1].spot_status = unoccupied;
                        } else {
                            parking.levels[aux[0]].spots[aux[1] + 1].spot_status = occupied;
                        }
                    }
                    check = 0;
                    spot1 = &parking.levels[aux[0]].spots[aux[1]];
                    spot2 = &parking.levels[aux[0]].spots[aux[1] - 1];
                    aux[1] = aux[1] - 1;
                } else if (aux[1] != parking.levels[0].size - 1 && parking.levels[aux[0]].spots[aux[1] + 1].id == 0) {
                    if (aux[1] != 0) {
                        if (parking.levels[aux[0]].spots[aux[1] - 1].id == 0) {
                            parking.levels[aux[0]].spots[aux[1] - 1].spot_status = unoccupied;
                        } else {
                            parking.levels[aux[0]].spots[aux[1] - 1].spot_status = occupied;
                        }
                    }
                    check = 0;
                    spot1 = &parking.levels[aux[0]].spots[aux[1]];
                    spot2 = &parking.levels[aux[0]].spots[aux[1] + 1];
                }
            }

        } else {
            spot1 = &parking.levels[aux[0]].spots[aux[1]];
            spot2 = &parking.levels[aux[0]].spots[aux[1] + 1];
        }
        spot1->spot_status = occupied;
        spot1->id = id;
        spot2->spot_status = occupied;
        spot2->id = id;
        parking.spots_available-=2;
        if (parking.spots_available > 0) {
            pthread_cond_signal(&spot_becoming_available);
        }
        printf("ENTRADA: Camión %03d aparca en planta %d, plazas %d y %d. Plazas libres: %d.\n", id, aux[0], aux[1], aux[1] + 1, parking.spots_available);
        print_parking();
        pthread_mutex_unlock(&parking_mutex);
        sleep(rand() % 5 + 5);
        pthread_mutex_lock(&parking_mutex);
        spot1->id = 0;
        if (spot1->spot_status == occupied) {
            spot1->spot_status = unoccupied;
        }
        spot2->id = 0;
        if (spot2->spot_status == occupied) {
            spot2->spot_status = unoccupied;
        }
        parking.spots_available += 2;
        printf("SALIDA: Camión %03d saliendo. Plazas libres: %d.\n", id, parking.spots_available);
        print_parking();
        pthread_cond_broadcast(&truck_check);
        if (spot1->spot_status == unoccupied || spot2->spot_status == unoccupied) {
            pthread_cond_signal(&spot_becoming_available);
        }
        pthread_mutex_unlock(&parking_mutex);
        free(aux);
        sleep(rand() % 5 + 5);
    }
}

void print_parking() {
    printf("Parking:\n");
    for (int i = 0; i < parking.size; ++i) {
        printf("\tPlanta %d: ", i);
        for (int j = 0; j < parking.levels[i].size; ++j) {
            printf("[%03d] ", parking.levels[i].spots[j].id);
        }
        printf("\n");
    }
    printf("\n");
    fflush(stdout);
}

char spot_status_tToChar(spot_status_t spotStatus) {
    switch (spotStatus) {
        case unoccupied:
            return 'u';
        case occupied:
            return 'o';
        case reserved:
            return 'r';
        default:
            return ' ';
    }
}

_Noreturn void init_parking(int levels, int spotsPerLevel, int carAmount, int truckAmount) {
    pthread_mutex_init(&parking_mutex, NULL);
    pthread_cond_init(&spot_becoming_available, NULL);
    pthread_cond_init(&truck_check, NULL);
    pthread_t *cars = malloc(carAmount * sizeof(pthread_t));
    int *carPlates = malloc(carAmount * sizeof(int));
    pthread_t *trucks = malloc(truckAmount * sizeof(pthread_t));
    int *truckPlates = malloc(truckAmount * sizeof(int));
    parking.size = levels;
    parking.spots_available = levels * spotsPerLevel;
    parking.levels = malloc(levels * sizeof(parking_level_t));
    for (int i = 0; i < levels; ++i) {
        parking.levels[i].size = spotsPerLevel;
        parking.levels[i].spots = malloc(spotsPerLevel * sizeof(spot_t));
        for (int j = 0; j < spotsPerLevel; ++j) {
            parking.levels[i].spots[i].spot_status = unoccupied;
            parking.levels[i].spots[i].id = 0;
        }
    }

    for (int i = 0; i < carAmount; ++i) {
        carPlates[i] = i + 1;
        pthread_create(cars + i, NULL, &car, (void *) &(carPlates[i]));
    }

    for (int i = 0; i < truckAmount; ++i) {
        truckPlates[i] = i + 1 + 100;
        pthread_create(trucks + i, NULL, &truck, (void *) &(truckPlates[i]));
    }
    while (1);
}

int *search_empty_spot() {
    for (int i = 0; i < parking.size; ++i) {
        for (int j = 0; j < parking.levels[i].size; ++j) {
            if (parking.levels[i].spots[j].spot_status == unoccupied) {
                int *aux = malloc(2 * sizeof(int));
                aux[0] = i;
                aux[1] = j;
                return aux;
            }
        }
    }
    return NULL;
}

int *search_two_empty_spots() {
    for (int i = 0; i < parking.size; ++i) {
        for (int j = 0; j < parking.levels[i].size - 1; ++j) {
            if ((parking.levels[i].spots[j].spot_status == unoccupied) &&
                (parking.levels[i].spots[j + 1].spot_status == unoccupied)) {
                int *aux = malloc(2 * sizeof(int));
                aux[0] = i;
                aux[1] = j;
                return aux;
            }
        }
    }
    return NULL;
}
