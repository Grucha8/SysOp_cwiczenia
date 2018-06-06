/* ============INCLUDE BLOCK============ */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/stat.h>
#include <semaphore.h>
#include <fcntl.h>

/* ============END OF INCLUDE BLOCK============ */


/* ============DEFINE BLOCK============ */
#define ERROR_MSG(format, ...) { printf(format, ##__VA_ARGS__); exit(-1);}
#define SEM_NAME "smeafor"

/* ============END OF DEFINE BLOCK============ */

/* enum */
enum data_status{
    NOTFULL = 0, FULL = 1
};

enum which_searching{
    EQUAL = 0, SMALLER = 1, GREATER = 2
};


/* shared memory struct */
struct shared_memory_data{
    char** dataArray;
    int manufacturerPosition;
    int customerPosition;
    enum data_status status;
};

/* thread arg struct */
struct thread_arg{
    int P, K, N, L, nk;
    unsigned char writingFlag;       // 1 - opisowy, 0 - uproszczony
    enum which_searching searching;
};


/* ============GLOBAL VARIABLES BLOCK============ */
struct shared_memory_data data;
FILE* dataFile;
sem_t sem;

/* ============END OF GLOBAL VARIABLES BLOCK============ */


/* ============FUNCTION DECLARATIONS BLOCK============ */
void init(char* fileName, struct thread_arg* tArgs);
void* cus_function(void* arg);
void* man_function(void* arg);

/* ============END OF FUNCTION DECLARATION BLOCK============ */


/* ============FUNCTIONS============ */
int main(int argc, char* argv[]){
    if (argc < 2)
    ERROR_MSG("Too few arguments\n");

    struct thread_arg tArgs;

    init(argv[1], &tArgs);

    pthread_t man[tArgs.P], cus[tArgs.K];

    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);


    for (int i = 0; i < tArgs.P; ++i) {
        if (pthread_create(&man[i], &attr, man_function, (void *) &tArgs) == -1)
        ERROR_MSG("Couldn't create a thread\n");
    }
    // sleep(1);
    for (int i = 0; i < tArgs.K; ++i) {
        if (pthread_create(&cus[i], &attr, cus_function, (void *) &tArgs) == -1)
        ERROR_MSG("Couldn't create a thread\n");
    }

    pthread_attr_destroy(&attr);

    if(tArgs.nk > 0) {
        sleep(tArgs.nk);

        for (int j = 0; j < tArgs.P; ++j) {
            pthread_cancel(man[j]);
        }
        for (int i = 0; i < tArgs.K; ++i) {
            pthread_cancel(cus[i]);
        }
    }else if (tArgs.nk == 0){
        for (int j = 0; j < tArgs.P; ++j) {
            pthread_join(man[j], NULL);
        }
        for (int i = 0; i < tArgs.K; ++i) {
            pthread_cancel(cus[i]);
        }
    }
}


//P, K, N, name, L, trybszukania, trybwypisywania, nk
void init(char* fileName, struct thread_arg* tArgs){
    FILE *pFILE = fopen(fileName, "r");

    char* buf = NULL;
    size_t len;


    if (getline(&buf, &len, pFILE) == -1)
    ERROR_MSG("No P\n");
    tArgs->P = atoi(buf);

    if (getline(&buf, &len, pFILE) == -1)
    ERROR_MSG("No K\n");
    tArgs->K = atoi(buf);

    if (getline(&buf, &len, pFILE) == -1)
    ERROR_MSG("No N\n");
    tArgs->N = atoi(buf);

    if (getline(&buf, &len, pFILE) == -1)
    ERROR_MSG("No dataFile\n");
    buf[strlen(buf)-1] = '\0';
    if ((dataFile = fopen(buf, "r")) == NULL)
    ERROR_MSG("Coudln't open pan_tadeusz\n");


    if (getline(&buf, &len, pFILE) == -1)
    ERROR_MSG("No L\n");
    tArgs->L = atoi(buf);

    if (getline(&buf, &len, pFILE) == -1)
    ERROR_MSG("No searching_type\n");
    switch (buf[0]){
        case '=':
            tArgs->searching = EQUAL;
            break;
        case '>':
            tArgs->searching = GREATER;
            break;
        case '<':
            tArgs->searching = SMALLER;
            break;
        default:
        ERROR_MSG("Wrong searching type\n");
    }

    if (getline(&buf, &len, pFILE) == -1)
    ERROR_MSG("No writingFlag\n");
    tArgs->writingFlag = atoi(buf);

    if (getline(&buf, &len, pFILE) == -1)
    ERROR_MSG("No nk\n");
    tArgs->nk = atoi(buf);

    /* initializing main array */
    data.dataArray = (char**)calloc(tArgs->N, sizeof(char*));
    data.customerPosition = 0;
    data.manufacturerPosition = 0;
    data.status = NOTFULL;

    // initializing sem
    if((sem_init(&sem, 0, 1)) == -1)
        ERROR_MSG("Couldnt make semaphore\n");

    fclose(pFILE);
}

/* ========================================================== */
/* ====================MANUFACTURER BLOCK==================== */
/* ========================================================== */
void parse_line(int whichBlock, int writingFlag){
    char* tmp = NULL;
    size_t size = 0;
    int len;

    if(getline(&tmp, &size, dataFile) < 0) {
        if(writingFlag)
            printf("SHUTTING DOWN\n");
        sem_post(&sem);
        pthread_exit((void *) 0);
    }

    len = strlen(tmp);

    if (tmp[len-1] = '\n')
        tmp[len-1] = '\0';

    data.dataArray[whichBlock] = calloc(len+1, sizeof(char));
    strcpy(data.dataArray[whichBlock], tmp);

    if (writingFlag)
        printf("Manufacturer: adding to data:<<%s>>\n", tmp);
}


void* man_function(void* arg){
    struct thread_arg* my_data;
    my_data = (struct thread_arg*) arg;

    if (my_data->writingFlag)
        printf("Manufacturer: starring work\n");

    while (1) {
        sem_wait(&sem);

        if (my_data->writingFlag)
            printf("Manufacturer: getting mutex\n");

        // checking if full
        while (data.status == FULL){
            sem_post(&sem);
            sem_wait(&sem);
        }

        // manufacturer is adding new line
        int whichBlock = (data.manufacturerPosition);

        // putting line into data array
        parse_line(whichBlock, my_data->writingFlag);

        if ((data.manufacturerPosition+1)%my_data->N == data.customerPosition){
            data.status = FULL;
        }
        if ((data.customerPosition) == data.manufacturerPosition){
            data.manufacturerPosition = (whichBlock + 1) % my_data->N;
            sem_post(&sem);     // if man added product next on the cus list
            continue;

        }

        data.manufacturerPosition = (whichBlock + 1) % my_data->N;

        if (my_data->writingFlag)
            printf("Manufacturer: giving away mutex\n");
        sem_post(&sem);
    }
}
/* ============END OF MANUFACTURER BLOCK============ */


/* ============CUSTOMER BLOCK============ */

void write_string(int whichBlock, int searching, int maxSize){
    char tmp[strlen(data.dataArray[whichBlock])+1];

    strcpy(tmp, data.dataArray[whichBlock]);

    free(data.dataArray[whichBlock]);
    data.dataArray[whichBlock] = NULL;

    switch (searching){
        case EQUAL:
            if (strlen(tmp) == maxSize)
                printf("CustomerWriting: %s\n", tmp);
            break;
        case GREATER:
            if (strlen(tmp) > maxSize)
                printf("CustomerWriting: %s\n", tmp);
            break;
        case SMALLER:
            if (strlen(tmp) < maxSize)
                printf("CustomerWriting: %s\n", tmp);
            break;
        default:
            printf("ERROR\n");
    }


    // free(tmp);
}

void* cus_function(void *arg){
    struct thread_arg* my_data;
    my_data = (struct thread_arg*) arg;

    if (my_data->writingFlag)
        printf("Customer: starting work\n");

    while (1){
        sem_wait(&sem);

        if (my_data->writingFlag)
            printf("Customer: getting mutex\n");

        // waiting for an item
        while (data.customerPosition == data.manufacturerPosition && data.status == NOTFULL){
            sem_post(&sem);
            sem_wait(&sem);
        }

        int whichBlock = (data.customerPosition);

        // deleting a block
        write_string(whichBlock, my_data->searching, my_data->L);
        data.customerPosition = (whichBlock+1) % my_data->N;

        // if array was full then broadcast
        if (data.status == FULL){
            data.status = NOTFULL;
            sem_post(&sem);
            continue;
        }


        if (my_data->writingFlag)
            printf("Customer: giving away mutex\n");

        sem_post(&sem);
    }

}

/* ============END OF CUSTOMER BLOCK============ */


