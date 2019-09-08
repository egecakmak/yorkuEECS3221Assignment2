/*

Family Name: Cakmak

Given Name: Ege

Student Number: 215173131
 
Section : EECS 3221 E

CS Login: cakmake

 */

#define TEN_MILLIS_IN_NANOS 10000000

#include <stdio.h>
#include <stdlib.h>
#include <semaphore.h>
#include <pthread.h>
#include <unistd.h>

typedef struct { // A struct for the buffer.
    char data;

    off_t offset;

    int state;

} BufferItem;

typedef struct { // A struct that is going to be used to pass parameters to threads.
    BufferItem *array;
    pthread_mutex_t *locks;
    FILE* file;
    int length;
    int bufferSize;

} param;

void *inThread(void *params);
void *workThread(void *params);
void *outThread(void *params);
void sleepSome();
void handleInput(int argc, char** argv, int *key, int *numberOfInThreads, int *numberOfWorkThreads, int *numberOfOutThreads, char *inputName, FILE **inputFile, char *outputName, FILE **outputFile, int *bufferSize);


pthread_mutex_t inputMutex; // A mutex for the input file.
pthread_mutex_t outputMutex; // A mutex for the output file.

int keyGlobal;

int charsRead;        // Values here keep record of the number of processed characters.
int charsWorkedOn;
int charsOutputted;
pthread_mutex_t charsReadMutex;
pthread_mutex_t charsWorkedOnMutex;
pthread_mutex_t charsOutputtedMutex;

int main(int argc, char** argv) {

    int key;
    int numberOfInThreads;
    int numberOfWorkThreads;
    int numberOfOutThreads;
    char *inputName = argv[5];
    FILE *inputFile;
    char *outputName = argv[6];
    FILE *outputFile;
    int bufferSize;
    int i;

    handleInput(argc, argv, &key, &numberOfInThreads, &numberOfWorkThreads, &numberOfOutThreads, inputName, &inputFile, outputName, &outputFile, &bufferSize);

    /* Counts the number of characters in the file. Could have done fflush to
     reset the character cursor position. However it did not work for some reason.
     Because of that opened the same file again.*/
    
    int numberOfCharacters = 0;

    int c;
    while ((c = fgetc(inputFile)) != EOF) {
        numberOfCharacters++;
    }
    fclose(inputFile);

    if (!(inputFile = fopen(inputName, "rb"))) {
        fprintf(stdin, "Could not open the input file.\n");
        exit(-6); // Invalid inputName.
    }

    pthread_mutex_init(&inputMutex, 0); // Initializes mutexes.
    pthread_mutex_init(&outputMutex, 0);
    pthread_mutex_init(&charsReadMutex, 0);
    pthread_mutex_init(&charsWorkedOnMutex, 0);
    pthread_mutex_init(&charsOutputtedMutex, 0);

    keyGlobal = key;
    
    // Initializes data structures.
    pthread_mutex_t *locks = calloc(bufferSize, sizeof (pthread_mutex_t)); 
    BufferItem *buffer = calloc(bufferSize, sizeof (BufferItem));

    for (i = 0; i < bufferSize; i++) { // Initializes all locks.
        pthread_mutex_init(&locks[i], 0);
    }
    // Creates all parameters to be passed to the threads and assigns their values.
    param *inParam = malloc(sizeof (param));
    inParam->array = buffer;
    inParam->file = inputFile;
    inParam->locks = locks;
    inParam->length = numberOfCharacters;
    inParam->bufferSize = bufferSize;

    param *workParam = malloc(sizeof (param));
    workParam->array = buffer;
    workParam->file = NULL;
    workParam->locks = locks;
    workParam->length = numberOfCharacters;
    workParam->bufferSize = bufferSize;

    param *outParam = malloc(sizeof (param));
    outParam->array = buffer;
    outParam->file = outputFile;
    outParam->locks = locks;
    outParam->length = numberOfCharacters;
    outParam->bufferSize = bufferSize;

    pthread_t threadINIDs[numberOfInThreads];
    pthread_t threadWORKIDs[numberOfWorkThreads];
    pthread_t threadOUTIDs[numberOfOutThreads];


    for (i = 0; i < numberOfInThreads; i++) {
        pthread_t *currentThreadID = &threadINIDs[i];
        pthread_attr_t attr; // Attribute for thread.
        pthread_attr_init(&attr); // Initializes attribute.

        pthread_create(currentThreadID, &attr, inThread, (void *) inParam);
    }
    for (i = 0; i < numberOfWorkThreads; i++) {
        pthread_t *currentThreadID = &threadWORKIDs[i];
        pthread_attr_t attr; // Attribute for thread.
        pthread_attr_init(&attr); // Initializes attribute.

        pthread_create(currentThreadID, &attr, workThread, (void *) workParam);
    }
    for (i = 0; i < numberOfOutThreads; i++) {
        pthread_t *currentThreadID = &threadOUTIDs[i];
        pthread_attr_t attr; // Attribute for thread.
        pthread_attr_init(&attr); // Initializes attribute.

        pthread_create(currentThreadID, &attr, outThread, (void *) outParam);
    }

    for (i = 0; i < numberOfInThreads; i++) {
        pthread_join(threadINIDs[i], NULL); // Waits for thread ID i.
    }
    for (i = 0; i < numberOfWorkThreads; i++) {
        pthread_join(threadWORKIDs[i], NULL); // Waits for thread ID i.
    }
    for (i = 0; i < numberOfOutThreads; i++) {
        pthread_join(threadOUTIDs[i], NULL); // Waits for thread ID i.
    }
    // Garbage cleaning.
    fclose(inputFile);
    fclose(outputFile);
    free(inParam);
    free(workParam);
    free(outParam);
    free(locks);
    free(buffer);
    exit(0);
}

void *inThread(void *parameters) {
    sleepSome();
    param *params = (param*) parameters;
    BufferItem *item = malloc(sizeof (BufferItem));
    int itemWillBeWritten = 0;
    do {
        pthread_mutex_lock(&charsReadMutex); // Tests the exit condition.
        if (charsRead >= params->length) {
            pthread_mutex_unlock(&charsReadMutex);
            break;
        }
        pthread_mutex_unlock(&charsReadMutex);
        /*If there is an item that is left over from
         * previous iterations then no new item will be fetched. We can have
         * left over items when the buffer is full.
         */
        if (itemWillBeWritten == 0) { 
            pthread_mutex_lock(&inputMutex);
            item->data = fgetc(params->file);
            item->offset = ftell(params->file);
            pthread_mutex_unlock(&inputMutex);
        }

        int i;
        int isWritten = 0; // A boolean to check if item is written to the buffer.
        for (i = 0; i < params->bufferSize; i++) {
            pthread_mutex_lock(&params->locks[i]);
            if (params->array[i].state == 0 || params->array[i].state == 3) {
                params->array[i].data = item->data;
                params->array[i].offset = item->offset;
                params->array[i].state = 1;
                pthread_mutex_unlock(&params->locks[i]);
                isWritten = 1;
                break;
            } else {
                pthread_mutex_unlock(&params->locks[i]);
                isWritten = 0;
            }
        }
        // Tests if is written, if so it will increment charsRead.
        if (isWritten == 1) {
            itemWillBeWritten = 0;
            pthread_mutex_lock(&charsReadMutex);
            charsRead++;
            pthread_mutex_unlock(&charsReadMutex);
        } else {
            itemWillBeWritten = 1;
        }

        sleepSome();
    } while (1);
    free(item);
    pthread_exit(NULL);
}

void *workThread(void *parameters) {
    sleepSome();
    param *params = (param*) parameters;
    BufferItem *item = malloc(sizeof (BufferItem));
    do {
        pthread_mutex_lock(&charsWorkedOnMutex); // Tests the exit condition.
        if (charsWorkedOn >= params->length) {
            pthread_mutex_unlock(&charsWorkedOnMutex);
            break;
        }
        pthread_mutex_unlock(&charsWorkedOnMutex);

        int i;
        for (i = 0; i < params->bufferSize; i++) { // Encrypts adequate items with the given key.
            pthread_mutex_lock(&params->locks[i]);
            if (params->array[i].state == 1) {
                if (params->array[i].data > 31 && params->array[i].data < 127) {
                    params->array[i].data = (((int) params->array[i].data - 32) + 2 * 95 + keyGlobal) % 95 + 32;
                }
                params->array[i].state = 2; // Promotes state of the item.
                pthread_mutex_unlock(&params->locks[i]);
                pthread_mutex_lock(&charsWorkedOnMutex); // Increments charsWorkedOn.
                charsWorkedOn++;
                pthread_mutex_unlock(&charsWorkedOnMutex);
                break;
            }
            pthread_mutex_unlock(&params->locks[i]);
        }
        sleepSome();
    } while (1);
    free(item);
    pthread_exit(NULL);
}

void *outThread(void *parameters) {
    sleepSome();
    param *params = (param*) parameters;
    BufferItem *item = malloc(sizeof (BufferItem));
    int willPrint = 0;
    do {
        pthread_mutex_lock(&charsOutputtedMutex);
        if (charsOutputted >= params->length) { // Tests the exit condition.
            pthread_mutex_unlock(&charsOutputtedMutex);
            break;
        }
        pthread_mutex_unlock(&charsOutputtedMutex);
        int i;
        for (i = 0; i < params->bufferSize; i++) { // Grabs items to be printed from the buffer.
            pthread_mutex_lock(&params->locks[i]);
            if (params->array[i].state == 2) {
                *item = params->array[i];
                params->array[i].state = 3;
                pthread_mutex_unlock(&params->locks[i]);
                willPrint = 1;
                break;
            }
            pthread_mutex_unlock(&params->locks[i]);
        }

        if (willPrint) { // Prints item.
            pthread_mutex_lock(&outputMutex);
            if (fseek(params->file, item->offset, SEEK_SET) == -1) {
                fprintf(stderr, "error setting output file position to %u\n",
                        (unsigned int) item->offset);
                exit(-1);
            }
            if (fputc(item->data, params->file) == EOF) {
                fprintf(stderr, "error writing byte %d to output file\n", item->data);
                exit(-1);
            }
            pthread_mutex_unlock(&outputMutex);
            willPrint = 0;
            pthread_mutex_lock(&charsOutputtedMutex); // Increments charsOutputted.
            charsOutputted++;
            pthread_mutex_unlock(&charsOutputtedMutex);
        }

        sleepSome();
    } while (1);
    free(item);
    pthread_exit(NULL);
}

void sleepSome() { // Sleeps for some short amount of time.

    struct timespec t;
    int seed = 0;

    t.tv_sec = 0;
    t.tv_nsec = rand_r(&seed) % (TEN_MILLIS_IN_NANOS + 1);
    nanosleep(&t, NULL);
}

// Checks if input is valid and if so assigns them to their variables.
void handleInput(int argc, char** argv, int *key, int *numberOfInThreads, int *numberOfWorkThreads, int *numberOfOutThreads, char *inputName, FILE **inputFile, char *outputName, FILE **outputFile, int *bufferSize) {
    if (argc != 8) {
        fprintf(stderr, "Invalid number of arguments.\n");
        fprintf(stderr, "encrypt <KEY> <nIN> <nWORK> <nOUT> <file_in> <file_out> <bufSize>\n");
        exit(-1); // Invalid use.
    }
    if (atoi(argv[1]) >= -127 && atoi(argv[1]) <= 127) {
        *key = atoi(argv[1]);
    } else {
        fprintf(stderr, "Key is not within range [-127,127]\n");
        exit(-2); // Invalid key.
    }

    if (atoi(argv[2]) > 0) {
        *numberOfInThreads = atoi(argv[2]);
    } else {
        fprintf(stderr, "There should be at least 1 in thread.\n");
        exit(-3); // Invalid numberOfInThreads.
    }

    if (atoi(argv[3]) > 0) {
        *numberOfWorkThreads = atoi(argv[3]);
    } else {
        fprintf(stderr, "There should be at least 1 work thread.\n");
        exit(-4); // Invalid numberOfWorkThreads.
    }

    if (atoi(argv[4]) > 0) {
        *numberOfOutThreads = atoi(argv[4]);
    } else {
        fprintf(stderr, "There should be at least 1 out thread.\n");
        exit(-5); // Invalid numberOfOutThreads.
    }

    if (!(*inputFile = fopen(inputName, "rb"))) {
        fprintf(stderr, "Could not open the input file.\n");
        exit(-6); // Invalid inputName.
    }
    if (!(*outputFile = fopen(outputName, "wb"))) {
        fprintf(stderr, "Could not open the output file.\n");
        exit(-7); // Invalid inputName.
    }

    if (atoi(argv[7]) > 0) {
        *bufferSize = atoi(argv[7]);
    } else {
        fprintf(stderr, "Buffer size should be at least 1.\n");
        exit(-8); // Invalid bufferSize.
    }

}

