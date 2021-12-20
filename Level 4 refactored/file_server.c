#include <stdio.h>
#include <stdlib.h>
#include <semaphore.h>
#include <pthread.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

#define BUFFER_SIZE 500
#define MAX_FILES 20
#define INVALID 0
#define WRITE 2
#define READ 4
#define EMPTY 8

// DEFINIING THE DATA STRUCTURES USED

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

pthread_mutex_t workerlock;
pthread_mutex_t sequencelock;
pthread_mutex_t fsequencelock[MAX_FILES];
pthread_mutex_t commandlocks[MAX_FILES];
pthread_cond_t sequencecond;
pthread_cond_t fsequencecond[MAX_FILES];

file_tracker tracklist[MAX_FILES];
sem_t tracklock;
sem_t filequeue;
int activefiles = 0;
int thread_sequence = 0;
int next = 0;

// HELPER FUNCTION PROTOTYPES
void file_tracker_init(file_tracker *tracker);
void file_open_simulation();
void log_error(char *command, char *dest, char *contents);
void transfer_content(char *command, char *dest, FILE *fp1);
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
    // initialize local variables
    int sequence_id;
    int rwlock_index;
    int sequence_id_filequeue;

    // typecast the void pointer into the correct one: argument typedef
    argument *thread_info = (argument *)arg;

    // fix sequence of threads
    // Acquire a lock
    pthread_mutex_lock(&workerlock);

    // use a condition variable to fix up the sequence of threads:
    while (thread_info->sequence_id != thread_sequence)
    {
        pthread_cond_wait(&sequencecond, &workerlock);
    }

    // tell waker that it shouldnt send a signal first
    next = 0;

    // ask the "cache" if there is a slot for this thread
    // if there is, then queue up this thread on that slot grab a sequence_id for that slot
    // and increment the sequence_id readying it for the next possible thread that might also go into the same slot
    rwlock_index = tracklist_check(thread_info->dir);
    sequence_id_filequeue = tracklist[rwlock_index].sequence_id;
    tracklist[rwlock_index].sequence_id++;

    // tell waker that it can send a signal to the condition variable to wake up the next waiting thread.
    thread_sequence++;
    next = 1;

    // release the lock for this section
    pthread_mutex_unlock(&workerlock);

    // check if it's the thread's turn in the cacheslot to be executed.
    fsequence_check(rwlock_index, sequence_id_filequeue);

    // check commandtype and execute commands accordingly
    pthread_mutex_lock(&commandlocks[rwlock_index]);
    if (thread_info->commandtype == WRITE)
    {
        printf("%s %s %s on slot %d | active files: %d | t_id: %d | fseq_id: %d\n", thread_info->command, thread_info->dir, thread_info->string, rwlock_index, activefiles, thread_info->sequence_id, sequence_id_filequeue);
        write_command(thread_info->wholecommand, thread_info->dir, thread_info->string);
    }
    else
    {

        if (thread_info->commandtype == READ)
        {
            printf("%s %s on slot %d | active files: %d\n", thread_info->command, thread_info->dir, rwlock_index, activefiles);
            read_command(thread_info->wholecommand, thread_info->dir);
        }

        if (thread_info->commandtype == EMPTY)
        {
            printf("%s %s on slot %d | active files: %d\n", thread_info->command, thread_info->dir, rwlock_index, activefiles);
            empty_command(thread_info->wholecommand, thread_info->dir);
        }
    }
    pthread_mutex_unlock(&commandlocks[rwlock_index]);

    tracklist[rwlock_index].current_access++;
    tracklist[rwlock_index].fnext = 1;

    //
    release_filespot(rwlock_index);
    free(arg);
    pthread_exit(NULL);
}

// The master_thread's job is to accept inputs in a non-blocking manner
// and continuously spawn new threads for each of the commands that are inputted
// it converts the input into the argument data structure using the parse_input helper function.

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

// Function for the waker thread

// This thread's job is only to check if the next / fnext variables are == 1.
// If so, it will call the signal for the condition variable to wake up the threads that are waiting on it

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

// Main will initialize all the semaphores, mutexes, and condition variables that will be used.
// More over, it will spawn the master and the waker threads.

int main()
{
    // Semaphore, mutex, and condition variable init
    pthread_mutex_init(&workerlock, NULL);
    pthread_mutex_init(&sequencelock, NULL);
    pthread_cond_init(&sequencecond, NULL);

    sem_init(&tracklock, 0, 1);
    sem_init(&filequeue, 0, MAX_FILES);

    for (int i = 0; i < MAX_FILES; i++)
    {
        file_tracker_init(&tracklist[i]);
        pthread_mutex_init(&fsequencelock[i], NULL);
        pthread_cond_init(&fsequencecond[i], NULL);
        pthread_mutex_init(&commandlocks[i], NULL);
    }

    // Spawning the master and waker threads
    pthread_create(&tid, NULL, &master_thread, NULL);
    pthread_create(&tid2, NULL, &waker, NULL);

    // Calling join so that the program will not end until the spawned threads die (which is never).
    pthread_join(tid, NULL);

    return 0;
}

// HELPER FUNCTIONS

// parse_input accepts 2 arguments, the raw input and i which corresponds to the order the command was inputted.

argument *parse_input(char *input, int i)
{
    char *command;
    char *dir;
    char *string;
    argument *arg = malloc(sizeof(argument));

    arg->commandtype = INVALID; // commandtype initially invalid if not overwritten then we know it's invalid.
    input[strcspn(input, "\n")] = 0;
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

void file_tracker_init(file_tracker *tracker)
{
    tracker->users = 0;
}

void file_open_simulation()
{
    srand(time(0));
    int SLEEP_TOSS = rand() % 100;

    if (SLEEP_TOSS >= 80)
        sleep(6);

    else
        sleep(1);
}

// WRITE ERROR LOGS FOR READ AND EMPTY
void log_error(char *command, char *dest, char *contents)
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

// transferring the contents of the file pointed to by fp1 to the destination file

void transfer_content(char *command, char *dest, FILE *fp1)
{
    FILE *fptr;
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

        fprintf(fptr, "%s", buffer);
    }
    fprintf(fptr, "\n");

    fclose(fptr);
}

void write_command(char *wholecommand, char *dest, char *string)
{
    // SIMULATE OPENING A FILE
    FILE *fptr;
    int STRING_SIZE = strlen(string);
    int i;

    if ((fptr = fopen(dest, "a")) == NULL)
    {
        printf("Error reading, Lawdie\n");
        return;
    }

    for (i = 0; i < STRING_SIZE; i++)
    {
        fputc(string[i], fptr);
        usleep(25);
    }
    fclose(fptr);
}

void read_command(char *wholecommand, char *dest)
{

    FILE *fptr;

    if ((fptr = fopen(dest, "r")) == NULL)
    {
        log_error(wholecommand, "read.txt", "FILE DNE");
        return;
    }

    transfer_content(wholecommand, "read.txt", fptr);
    fclose(fptr);
}

void empty_command(char *wholecommand, char *dest)
{

    FILE *fptr;
    if ((fptr = fopen(dest, "r")) == NULL)
    {
        log_error(wholecommand, "empty.txt", "FILE ALREADY EMPTY");
        return;
    }

    transfer_content(wholecommand, "empty.txt", fptr);
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
    // get the lock so that there will be no other threads that will try to get a slot in the filetracker
    sem_wait(&tracklock);

    // subtract users from the current slot
    --(tracklist[index].users);

    // if it's the last user for that slot, reset the sequence_id and current_access so that it'll be usable for another file
    if (tracklist[index].users == 0)
    {
        activefiles--;
        tracklist[index].current_access = 0;
        tracklist[index].sequence_id = 0;
    }
    // send a sem_post so that if there are waiting thread's they can check if there is a slot
    sem_post(&filequeue);

    sem_post(&tracklock);
}
