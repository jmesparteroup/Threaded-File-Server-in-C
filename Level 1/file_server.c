#include <stdio.h>
#include <stdlib.h>
#include <semaphore.h>
#include <pthread.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

#define MAX_THREADS 10
#define BUFFER_SIZE 1000
#define MAX_FILES 10

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
} file_tracker;

// DEFINING GLOBAL VARIABLES
pthread_t tid[MAX_THREADS];
char *delim = " ";
rwlock_t *mutexes;
file_tracker *tracklist;
sem_t workerlock;
sem_t tracklock;
sem_t filequeue;
int activefiles = 0;

// DEFINING HELPER FUNCTIONS
void rwlock_init(rwlock_t *lock)
{
    lock->readers = 0;
    sem_init(&lock->lock, 0, 1);
    sem_init(&lock->writelock, 0, 1);
}

void file_tracker_init(file_tracker *tracker)
{
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
    int SLEEP_TOSS = rand() % 100;

    if (SLEEP_TOSS > 80)
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
    fprintf(stderr, "\nEXECUTING: %s\n", wholecommand);

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
    fprintf(fptr, "\n");

    puts("DONE");

    fclose(fptr);
}

void read_command(char *wholecommand, char *dest)
{

    // SIMULATE OPENING A FILE
    FILE_OPEN_SIMULATION();
    FILE *fptr;
    fprintf(stderr, "\nEXECUTING: %s\n", wholecommand);

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
    fprintf(stderr, "\nEXECUTING: %s\n", wholecommand);

    FILE *fptr;
    if ((fptr = fopen(dest, "r")) == NULL)
    {
        puts("HELLO");
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
    puts("AQUIRED TRACKLOCK");
    int rwlock_index = -1;

    while (1)
    {
        for (int i = 0; i < MAX_FILES; i++)
        {
            if (rwlock_index < 0)
                if ((tracklist[i].users == 0))
                {
                    puts("FOUND INDEX");
                    rwlock_index = i;
                }

            if (strcmp(dir, tracklist[i].filename) == 0)
            {
                puts("FILENAME BEING USED");
                ++(tracklist[i].users);
                rwlock_index = i;

                sem_post(&tracklock);
                return rwlock_index;
            }
        }
        puts("OUT OF FORLOOP");
        sem_wait(&filequeue);
        if (rwlock_index >= 0)
        {
            puts("BREAKING OUT OF LOOP");
            break;
        }
        sem_post(&filequeue);
    }

    puts("ATTEMPTING TO EDIT TRACKLIST");
    strcpy(tracklist[rwlock_index].filename, dir);
    ++(tracklist[rwlock_index].users);
    puts("SUCCESS EDITING TRACKLIST");

    sem_post(&tracklock);
    return rwlock_index;
}

void release_filespot(int index)
{
    --(tracklist[index].users);
    if (tracklist[index].users == 0)
        sem_post(&filequeue);
}

void *worker_thread(void *arg)
{

    sem_wait(&workerlock);
    ++activefiles;
    printf("FILE %d\n", activefiles);
    usleep(25);
    int rwlock_index;

    // lock critical section up
    char *wholecommand = (char *)arg;
    fprintf(stdout, "\nINSIDE WORKER THREAD OF: %s\n", wholecommand);
    char *command = strtok(wholecommand, delim);

    if (strcmp("write", command) == 0)
    {
        char *dir = strtok(NULL, delim);
        char *string = strtok(NULL, "\0");
        // fprintf(stdout, "\nINSIDE WORKER THREAD OF: %s %s %s\n", command, dir, string);
    }
    else
    {
        char *dir = strtok(NULL, "\n");
        // fprintf(stdout, "\nINSIDE WORKER THREAD OF: %s %s\n", command, dir);
    }

    sem_post(&workerlock);

    // lock release lock

    // if (strcmp("write", truecommand) == 0)
    // {
    //     // puts("INSIDE WRITE");
    //     char *dir = strtok(NULL, delim);
    // char *truedir = malloc(sizeof(char) * 50);
    // strcpy(truedir, dir);

    //     // GET FILE_QUEUE INDEX
    //     rwlock_index = tracklist_check(truedir);
    //     printf("CHECKED TRACKLIST with index: %d\n", rwlock_index);

    // char *string = strtok(NULL, "\0");
    // char str[50];
    // strcpy(str, string);

    //     rwlock_acquire_writelock(&mutexes[rwlock_index]);
    //     puts("ACQUIRED WRITELOCK");
    //     write_command(wholecommand_copy, truedir, str);
    //     rwlock_release_writelock(&mutexes[rwlock_index]);
    // }
    // else
    //     {
    // char *dir = strtok(NULL, "\n");
    // char *truedir = malloc(sizeof(char) * 50);
    // strcpy(truedir, dir);

    //         // GET FILE_QUEUE INDEX
    //         rwlock_index = tracklist_check(truedir);

    //         if (strcmp("read", truecommand) == 0)
    //         {
    //             rwlock_acquire_readlock(&mutexes[rwlock_index]);
    //             read_command(wholecommand_copy, truedir);
    //             rwlock_release_readlock(&mutexes[rwlock_index]);
    //         }

    //         if (strcmp("empty", truecommand) == 0)
    //         {
    //             rwlock_acquire_writelock(&mutexes[rwlock_index]);
    //             empty_command(wholecommand_copy, truedir);
    //             rwlock_release_writelock(&mutexes[rwlock_index]);
    //         }
    //     }

    //     release_filespot(rwlock_index);
    //     pthread_exit(NULL);
}

void master_thread()
{
    char input[106];
    FILE *fptr;
    time_t curtime;
    puts("MASTER THREAD CREATED");

    int i = 1;

    while (1)
    {
        if (fgets(input, 106,stdin) == EOF)
            break;
        pthread_create(&tid[2], NULL, &worker_thread, &input);
        pthread_detach(tid[2]);

        if ((fptr = fopen("commands.txt", "a")) == NULL)
        {
            puts("LOGGING COMMAND FAILED");
        }

        time(&curtime);
        char *c_time_string = ctime(&curtime);
        strtok(input, "\n");

        fprintf(stderr, "%s %s\n", input, c_time_string);
        fprintf(fptr, "%s %s\n", input, c_time_string);
        fclose(fptr);
    }
    fprintf(stderr, "PROGRAM EXECUTION | i: %d\n", i);
    while (1)
    {
    };
}

int main()
{
    void *fs[] = {master_thread};

    mutexes = malloc(sizeof(rwlock_t *) * MAX_FILES);
    tracklist = malloc(sizeof(file_tracker *) * MAX_FILES);

    sem_init(&tracklock, 0, 1);
    sem_init(&filequeue, 0, MAX_FILES);
    sem_init(&workerlock, 0, 1);

    puts("INIT DONE");
    for (int i = 0; i < MAX_FILES; i++)
    {
        rwlock_init(&mutexes[i]);
        file_tracker_init(&tracklist[i]);
    }

    pthread_create(&tid[0], NULL, fs[0], NULL);
    pthread_join(tid[0], NULL);

    return 0;
}
