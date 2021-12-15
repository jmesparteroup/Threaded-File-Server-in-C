#include <stdio.h>
#include <stdlib.h>
#include <semaphore.h>
#include <pthread.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

#define MAX_THREADS 10
#define BUFFER_SIZE 1000

typedef struct _rwlock_t
{
    sem_t writelock;
    sem_t lock;
    int readers;
} rwlock_t;


pthread_t tid[MAX_THREADS];
char *delim = " ";
rwlock_t mutex;

void rwlock_init(rwlock_t *lock)
{
    lock->readers = 0;
    sem_init(&lock->lock, 0, 1);
    sem_init(&lock->writelock, 0, 1);
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

void *worker_thread(void *arg)
{
    char *wholecommand = (char *)arg;
    char wholecommand_copy[106];
    strcpy(wholecommand_copy, wholecommand);

    char *command = strtok(wholecommand, delim);
    char *truecommand = malloc(sizeof(char) * 50);
    strcpy(truecommand, command);

    if (strcmp("write", truecommand) == 0)
    {
        char *dir = strtok(NULL, delim);
        char *truedir = malloc(sizeof(char) * 50);
        strcpy(truedir, dir);

        char *string = strtok(NULL, "\0");
        char str[50];
        strcpy(str, string);
        rwlock_acquire_writelock(&mutex);
        write_command(wholecommand_copy, truedir, str);
        rwlock_release_writelock(&mutex);
    }
    else
    {
        char *dir = strtok(NULL, "\n");
        char *truedir = malloc(sizeof(char) * 50);
        strcpy(truedir, dir);

        if (strcmp("read", truecommand) == 0)
        {
            rwlock_acquire_readlock(&mutex);
            read_command(wholecommand_copy, truedir);
            rwlock_release_readlock(&mutex);
        }

        if (strcmp("empty", truecommand) == 0)
        {
            rwlock_acquire_writelock(&mutex);
            empty_command(wholecommand_copy, truedir);
            rwlock_release_writelock(&mutex);
        }
    }

    pthread_exit(NULL);
}

void master_thread()
{
    char input[106];
    int curr = 1;
    FILE *fptr;
    time_t curtime;

    while (1)
    {
        fgets(input, 50, stdin);
        pthread_create(&tid[curr], NULL, &worker_thread, &input);
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

    for (int i = 0; i < curr; ++i)
    {
        pthread_join(tid[i], NULL);
    }
}

int main()
{
    void *fs[] = {master_thread};
    rwlock_init(&mutex);

    pthread_create(&tid[0], NULL, fs[0], NULL);
    pthread_join(tid[0], NULL);

    return 0;
}
