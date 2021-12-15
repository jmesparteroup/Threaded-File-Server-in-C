#include <stdio.h>
#include <stdlib.h>
#include <semaphore.h>
#include <pthread.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

#define BUFFER_SIZE 250
#define MAX_FILES 20
#define WRITE 2
#define READ 4
#define EMPTY 8

// DEFINIING THE DATA STRUCTURES USED
typedef struct _rwlock_t
{
    sem_t writelock;
    sem_t lock;
    int readers;
} rwlock_t;

typedef struct _file_tracker
{
    char filename[50];
    int users;
    int sequence_id;
    int current_access;
} file_tracker;

typedef struct _argument
{
    char command[6];
    char dir[50];
    char string[50];
    char wholecommand[106];
    int sequence_id;
} argument;

// DEFINING GLOBAL VARIABLES
pthread_t tid, tid2;
rwlock_t *mutexes;

sem_t filequeue;
pthread_mutex_t workerlock;
pthread_mutex_t sequencelock;
pthread_mutex_t fsequencelock[MAX_FILES];
pthread_cond_t sequencecond;
pthread_cond_t fsequencecond[MAX_FILES];

rwlock_t *mutexes;
file_tracker *tracklist;
sem_t tracklock;
sem_t filequeue;
int activefiles = 0;
int thread_sequence = 0;

// HELPER FUNCTION PROTOTYPES
void rwlock_init(rwlock_t *lock);
void file_tracker_init(file_tracker *tracker);
void rwlock_acquire_readlock(rwlock_t *lock);
void rwlock_release_readlock(rwlock_t *lock);
void rwlock_acquire_writelock(rwlock_t *lock);
void rwlock_release_writelock(rwlock_t *lock);
void FILE_OPEN_SIMULATION();
void WRITE_LOGS(char *command, char *dest, char *contents);
void READ_CONTENT(char *command, char *dest, FILE *fp1);
void write_command(char *wholecommand, char *dest, char *string);
void read_command(char *wholecommand, char *dest);
void empty_command(char *wholecommand, char *dest);
int tracklist_check(char *dir);
void release_filespot(int index);
void fsequence_check(int rwlock_index, int sequence_id_filequeue);

void *worker_thread(void *arg)
{

    // fix sequence of threads
    // pthread_mutex_lock(&sequencelock);

    pthread_mutex_lock(&workerlock);

    int sequence_id;
    int rwlock_index;
    int sequence_id_filequeue;
    int commandtype;

    argument *thread_info = (argument *)arg;

    sequence_id = thread_info->sequence_id;

    while (sequence_id != thread_sequence)
    {
        pthread_cond_wait(&sequencecond, &workerlock);
    }

    thread_sequence++;

    char *command = malloc(6);
    char *truedir = malloc(50);
    char *string = malloc(50);
    char *wholecommandarg = malloc(106);
    strcpy(command, thread_info->command);
    strcpy(truedir, thread_info->dir);
    strcpy(string, thread_info->string);
    strcpy(wholecommandarg, thread_info->wholecommand);

    // check if write, or (read/empty)
    if (strcmp("write", command) == 0)
        commandtype = 2;
    if (strcmp("read", command) == 0)
        commandtype = 4;
    if (strcmp("empty", command) == 0)
        commandtype = 8;

    if (commandtype == WRITE)
    {
        rwlock_index = tracklist_check(truedir);
        sequence_id_filequeue = tracklist[rwlock_index].sequence_id;
        tracklist[rwlock_index].sequence_id++;

        pthread_mutex_unlock(&workerlock);

        fsequence_check(rwlock_index, sequence_id_filequeue);

        rwlock_acquire_writelock(&mutexes[rwlock_index]);
        printf("%s on slot %d | active files: %d\n", wholecommandarg, rwlock_index, activefiles);
        write_command(wholecommandarg, truedir, string);
        rwlock_release_writelock(&mutexes[rwlock_index]);
    }
    else
    {

        rwlock_index = tracklist_check(truedir);
        sequence_id_filequeue = tracklist[rwlock_index].sequence_id;
        tracklist[rwlock_index].sequence_id++;

        if (commandtype == READ)
        {
            pthread_mutex_unlock(&workerlock);

            fsequence_check(rwlock_index, sequence_id_filequeue);

            rwlock_acquire_readlock(&mutexes[rwlock_index]);
            printf("%s %s on slot %d | active files: %d\n", command, truedir, rwlock_index, activefiles);
            read_command(wholecommandarg, truedir);
            rwlock_release_readlock(&mutexes[rwlock_index]);
        }

        if (commandtype == EMPTY)
        {
            pthread_mutex_unlock(&workerlock);

            fsequence_check(rwlock_index, sequence_id_filequeue);

            rwlock_acquire_writelock(&mutexes[rwlock_index]);
            printf("%s %s on slot %d | active files: %d\n", command, truedir, rwlock_index, activefiles);
            empty_command(wholecommandarg, truedir);
            rwlock_release_writelock(&mutexes[rwlock_index]);
        }
    }
    release_filespot(rwlock_index);
    pthread_exit(NULL);
}

void *master_thread(void *arg)
{
    puts("FILESERVER READY TO ACCEPT INPUTS");
    FILE *fptr;
    time_t curtime;

    int i = 0;
    while (1)
    {
        char input[106];
        char *command;
        char *dir;
        char *string;
        argument *arg = malloc(sizeof(argument));

        if (fgets(input, 106, stdin) == NULL)
            break;
        if (strlen(input) < 4)
            break;

        input[strlen(input) - 2] = 0;

        // ORIGINAL
        // pthread_create(&tid, NULL, &worker_thread, &input);
        // pthread_detach(tid);

        strcpy(arg->wholecommand, input);
        command = strtok(input, " ");
        if (strcmp(command, "write") == 0)
        {
            dir = strtok(NULL, " ");
            string = strtok(NULL, "\0");
            strcpy(arg->string, string);
        }
        else
            dir = strtok(NULL, "\n\0");

        strcpy(arg->command, command);
        strcpy(arg->dir, dir);

        arg->sequence_id = i;

        // printf("NEW THREAD: %s %s %s id: %d\n", command, dir, string, i);

        pthread_create(&tid, NULL, &worker_thread, arg);
        pthread_detach(tid);

        if ((fptr = fopen("commands.txt", "a")) == NULL)
        {
            puts("LOGGING COMMAND FAILED");
        }

        time(&curtime);
        char *c_time_string = ctime(&curtime);

        // fprintf(stderr, "%s %s\n", input, c_time_string);
        fprintf(fptr, "%s %s\n", input, c_time_string);
        fclose(fptr);
        i++;
    }
    while (1)
    {
        sleep(1);
    };
}

void *waker(void *arg)
{
    while (1)
    {
        pthread_cond_signal(&sequencecond);
        for (int i = 0; i < MAX_FILES; i++)
            pthread_cond_signal(&fsequencecond[i]);
        usleep(20);
    }
}

int main()
{
    mutexes = malloc(sizeof(rwlock_t) * MAX_FILES);
    tracklist = malloc(sizeof(file_tracker) * MAX_FILES);

    pthread_mutex_init(&workerlock, NULL);
    pthread_mutex_init(&sequencelock, NULL);
    pthread_cond_init(&sequencecond, NULL);

    sem_init(&tracklock, 0, 1);
    sem_init(&filequeue, 0, MAX_FILES);

    for (int i = 0; i < MAX_FILES; i++)
    {
        rwlock_init(&mutexes[i]);
        file_tracker_init(&tracklist[i]);
        pthread_mutex_init(&fsequencelock[i], NULL);
        pthread_cond_init(&fsequencecond[i], NULL);
    }

    pthread_create(&tid, NULL, &master_thread, NULL);
    pthread_create(&tid2, NULL, &waker, NULL);
    pthread_join(tid, NULL);

    return 0;
}

// HELPER FUNCTIONS
void fsequence_check(int rwlock_index, int sequence_id_filequeue)
{
    pthread_mutex_lock(&fsequencelock[rwlock_index]);

    // check if it's the thread's turn on the filequeue slot
    while (sequence_id_filequeue != tracklist[rwlock_index].current_access)
        pthread_cond_wait(&fsequencecond[rwlock_index], &fsequencelock[rwlock_index]);
    printf("Fseq: %d | curr_access: %d\n", sequence_id_filequeue, tracklist[rwlock_index].current_access);

    // iterate current access to signal next job in the sequence that they can run now.
    tracklist[rwlock_index].current_access++;

    pthread_mutex_unlock(&fsequencelock[rwlock_index]);
}

void rwlock_init(rwlock_t *lock)
{
    lock->readers = 0;
    sem_init(&lock->lock, 0, 1);
    sem_init(&lock->writelock, 0, 1);
}

void file_tracker_init(file_tracker *tracker)
{
    strcmp(tracker->filename, "INIT");
    tracker->users = 0;
}

void rwlock_acquire_readlock(rwlock_t *lock)
{
    sem_wait(&lock->lock);
    lock->readers++;
    if (lock->readers == 1)
        sem_wait(&lock->writelock);
    sem_post(&lock->lock);
}

void rwlock_release_readlock(rwlock_t *lock)
{
    sem_wait(&lock->lock);
    lock->readers--;
    if (lock->readers == 0)
        sem_post(&lock->writelock);
    sem_post(&lock->lock);
}

void rwlock_acquire_writelock(rwlock_t *lock)
{
    sem_wait(&lock->writelock);
}

void rwlock_release_writelock(rwlock_t *lock)
{
    sem_post(&lock->writelock);
}

void FILE_OPEN_SIMULATION()
{
    srand(time(0));
    int SLEEP_TOSS = rand() % 100;

    if (SLEEP_TOSS >= 80)
        sleep(6);

    else
        sleep(1);
}

// WRITE ERROR LOGS FOR READ AND EMPTY
void WRITE_LOGS(char *command, char *dest, char *contents)
{
    FILE *fptr;

    if ((fptr = fopen(dest, "a")) == NULL)
    {
        fprintf(stderr, "ERROR LOGGING\n");
        return;
    }

    fprintf(fptr, "%s: %s\n", command, contents);
    fclose(fptr);
}

// READING FILES AND TRANSFERRING THEM INTO READ.TXT OR EMPTY.TXT
void READ_CONTENT(char *command, char *dest, FILE *fp1)
{
    FILE *fptr;
    // fprintf(stderr, "fptr: %p\n", fp1);
    long numbytes;
    char buffer[BUFFER_SIZE];

    if ((fptr = fopen(dest, "a")) == NULL)
    {
        fprintf(stderr, "ERROR READING\n");
        return;
    }

    // GRABBING CONTENTS CONTINUOUSLY UNTIL NULL
    /* Repeat this until read line is not NULL */
    fprintf(fptr, "%s: ", command);
    while (fgets(buffer, BUFFER_SIZE, fp1) != NULL)
    {
        /* Total character read count */
        int totalRead = strlen(buffer);
        buffer[totalRead - 1] = buffer[totalRead - 1] == '\n'
                                    ? '\0'
                                    : buffer[totalRead - 1];

        fprintf(fptr, "%s\n", buffer);
    }
    fclose(fptr);
}

void write_command(char *wholecommand, char *dest, char *string)
{
    // SIMULATE OPENING A FILE
    FILE_OPEN_SIMULATION();

    FILE *fptr;

    if ((fptr = fopen(dest, "a")) == NULL)
    {
        printf("Error reading, Lawdie\n");
        return;
    }

    int STRING_SIZE = strlen(string);
    int i;

    for (i = 0; i < STRING_SIZE; i++)
    {
        fputc(string[i], fptr);
        usleep(25);
    }
    // fprintf(fptr, "\n");
    fclose(fptr);
}

void read_command(char *wholecommand, char *dest)
{

    // SIMULATE OPENING A FILE
    FILE_OPEN_SIMULATION();
    FILE *fptr;

    // dest[strlen(dest)-1] = 0;

    if ((fptr = fopen(dest, "r")) == NULL)
    {
        WRITE_LOGS(wholecommand, "read.txt", "FILE DNE");
        return;
    }

    // fprintf(stderr, "fptr: %p\n", fptr);
    READ_CONTENT(wholecommand, "read.txt", fptr);
    fclose(fptr);
}

void empty_command(char *wholecommand, char *dest)
{

    // SIMULATE OPENING A FILE
    FILE_OPEN_SIMULATION();

    FILE *fptr;
    if ((fptr = fopen(dest, "r")) == NULL)
    {
        WRITE_LOGS(wholecommand, "empty.txt", "FILE ALREADY EMPTY");
        return;
    }

    READ_CONTENT(wholecommand, "empty.txt", fptr);
    fclose(fopen(dest, "w"));

    return;
}

int tracklist_check(char *dir)
{
    sem_wait(&tracklock);
    int rwlock_index = -1;

    while (1)
    {
        for (int i = 0; i < MAX_FILES; i++)
        {
            if (rwlock_index < 0)
                if ((tracklist[i].users == 0))
                {
                    rwlock_index = i;
                }

            if (strcmp(dir, tracklist[i].filename) == 0)
            {

                rwlock_index = i;
                ++(tracklist[rwlock_index].users);

                sem_post(&tracklock);
                return rwlock_index;
            }
        }
        sem_wait(&filequeue);
        if (rwlock_index >= 0)
        {
            sem_post(&tracklock);
            activefiles++;
            break;
        }
        sem_post(&filequeue);
    }

    strcpy(tracklist[rwlock_index].filename, dir);
    ++(tracklist[rwlock_index].users);

    sem_post(&tracklock);
    return rwlock_index;
}

void release_filespot(int index)
{
    sem_wait(&tracklock);

    --(tracklist[index].users);
    if (tracklist[index].users == 0)
    {
        activefiles--;
        tracklist[index].current_access = 0;
        tracklist[index].sequence_id = 0;
    }
    sem_post(&filequeue);

    sem_post(&tracklock);
}