#include <stdio.h>
#include <stdlib.h>
#include <semaphore.h>
#include <pthread.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

#define BUFFER_SIZE 500
#define MAX_FILES 30
#define INVALID 0
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
    int fnext;
} file_tracker;

typedef struct _argument
{
    char command[6];
    char dir[50];
    char string[50];
    char wholecommand[106];
    int sequence_id;
    int commandtype;
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
int next = 0;

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
void log_commands(argument *arg);
argument *parse_input(char *input, int i);

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

    while (thread_info->sequence_id != thread_sequence)
    {
        pthread_cond_wait(&sequencecond, &workerlock);
    }

    next = 0;

    rwlock_index = tracklist_check(thread_info->dir);
    sequence_id_filequeue = tracklist[rwlock_index].sequence_id;
    tracklist[rwlock_index].sequence_id++;
    thread_sequence++;

    next = 1;

    if (thread_info->commandtype == WRITE)
    {

        pthread_mutex_unlock(&workerlock);
        fsequence_check(rwlock_index, sequence_id_filequeue);

        rwlock_acquire_writelock(&mutexes[rwlock_index]);
        printf("%s %s %s on slot %d | active files: %d | t_id: %d | fseq_id: %d\n", thread_info->command, thread_info->dir, thread_info->string, rwlock_index, activefiles, thread_info->sequence_id, sequence_id_filequeue);
        write_command(thread_info->wholecommand, thread_info->dir, thread_info->string);
        rwlock_release_writelock(&mutexes[rwlock_index]);
    }
    else
    {

        if (thread_info->commandtype == READ)
        {
            pthread_mutex_unlock(&workerlock);

            fsequence_check(rwlock_index, sequence_id_filequeue);

            rwlock_acquire_writelock(&mutexes[rwlock_index]);
            printf("%s %s on slot %d | active files: %d\n", thread_info->command, thread_info->dir, rwlock_index, activefiles);
            read_command(thread_info->wholecommand, thread_info->dir);
            rwlock_release_writelock(&mutexes[rwlock_index]);
        }

        if (thread_info->commandtype == EMPTY)
        {
            pthread_mutex_unlock(&workerlock);

            fsequence_check(rwlock_index, sequence_id_filequeue);

            rwlock_acquire_writelock(&mutexes[rwlock_index]);
            printf("%s %s on slot %d | active files: %d\n", thread_info->command, thread_info->dir, rwlock_index, activefiles);
            empty_command(thread_info->wholecommand, thread_info->dir);
            rwlock_release_writelock(&mutexes[rwlock_index]);
        }
    }
    tracklist[rwlock_index].current_access++;
    tracklist[rwlock_index].fnext = 1;
    release_filespot(rwlock_index);
    pthread_exit(NULL);
}

void *master_thread(void *arg)
{
    puts("FILESERVER READY TO ACCEPT INPUTS");

    int i = 0;
    char input[106];

    while (1)
    {

        argument *arg = malloc(sizeof(argument));
        if (fgets(input, 106, stdin) == NULL) // HANDLE EOF
            break;

        arg = parse_input(input, i);
        if (arg->commandtype == INVALID)
            break; // HANDLE INVALID INSTRUCTIONS
        log_commands(arg);

        pthread_create(&tid, NULL, &worker_thread, arg);
        pthread_detach(tid);

        i++;
    }
    puts("Reached end of the file. Executing remaining commands");
    while (1)
    {
        sleep(1);
    };
}

void *waker(void *arg)
{
    while (1)
    {
        if (next == 1)
        {
            pthread_cond_signal(&sequencecond);
        }
        for (int i = 0; i < MAX_FILES; i++)
            if (tracklist[i].fnext == 1)
                pthread_cond_signal(&fsequencecond[i]);
        usleep(10);
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

argument *parse_input(char *input, int i)
{
    char *command;
    char *dir;
    char *string;
    argument *arg = malloc(sizeof(argument));

    input[strlen(input) - 2] = 0;
    arg->commandtype = INVALID; // commandtype initially invalid if not overwritten then we know it's invalid.

    strcpy(arg->wholecommand, input);
    command = strtok(input, " ");
    if (strcmp(command, "write") == 0)
    {
        dir = strtok(NULL, " ");
        string = strtok(NULL, "\0");
        strcpy(arg->string, string);
        arg->commandtype = WRITE;
    }
    else
    {
        dir = strtok(NULL, "\0");
        if (strcmp(command, "read") == 0)
            arg->commandtype = READ;
        if (strcmp(command, "empty") == 0)
            arg->commandtype = EMPTY;
    }

    strcpy(arg->command, command);
    strcpy(arg->dir, dir);
    arg->sequence_id = i;

    free(command);
    free(dir);
    free(string);

    return arg;
}

void log_commands(argument *arg)
{
    FILE *fptr;
    time_t curtime;
    if ((fptr = fopen("commands.txt", "a")) == NULL)
    {
        puts("LOGGING COMMAND FAILED");
    }

    time(&curtime);
    char *c_time_string = ctime(&curtime);

    // fprintf(stderr, "%s %s\n", input, c_time_string);
    if (arg->commandtype == WRITE)
        fprintf(fptr, "%s %s %s %s", arg->command, arg->dir, arg->string, c_time_string);
    else
        fprintf(fptr, "%s %s %s", arg->command, arg->dir, c_time_string);

    fclose(fptr);
}

void fsequence_check(int rwlock_index, int sequence_id_filequeue)
{
    pthread_mutex_lock(&fsequencelock[rwlock_index]);

    // check if it's the thread's turn on the filequeue slot
    while (sequence_id_filequeue != tracklist[rwlock_index].current_access)
        pthread_cond_wait(&fsequencecond[rwlock_index], &fsequencelock[rwlock_index]);
    // printf("Fseq: %d | curr_access: %d\n", sequence_id_filequeue, tracklist[rwlock_index].current_access);

    // iterate current access to signal next job in the sequence that they can run now.
    tracklist[rwlock_index].fnext = 0;

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
            activefiles++;
            sem_post(&tracklock);
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
