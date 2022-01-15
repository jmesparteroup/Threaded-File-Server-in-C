//   A | M   CS 140 PROJECT 2
//  — + —    ESPARTERO, Joshua Allyn Marck M.
// D | G     2019-10009

#include <stdio.h>
#include <stdlib.h>
#include <semaphore.h>
#include <pthread.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

#define BUFFER_SIZE 600  // size of buffer for reading/emptying files
#define MAX_FILES 30     // maximum files that can be ran in parallel

// Command Type definitions
#define INVALID 1
#define WRITE 2
#define READ 4
#define EMPTY 8

// DEFINIING THE DATA STRUCTURES USED

/* file_tracker data structure
 * An array of file_trackers will be used as a tracking structure
 * that will allow the program to check if a certain file has queued up
 * instructions. Otherwise, it will check for a slot in the array.
 * If no slots are available, this array is protected by a semaphore
 * that is initialized with the same value as the size of the array.
 * Hence, if the array is full the next file will sleep to wait.
 */
typedef struct _file_tracker
{
    char filename[60];
    int users;
    int sequence_id;
    int current_access;
    int fnext;
} file_tracker;

/* Argument data structure
 * This data structure is created to be passed as an argument to the
 * threads.This is will contain all the necessary information worker
 * threads need to perform the task they are ought to do.
 */

typedef struct _argument
{
    char command[6];
    char dir[60];
    char string[60];
    char wholecommand[126];
    int sequence_id;
    int commandtype;
} argument;

// DEFINING GLOBAL VARIABLES
pthread_t tid;

pthread_mutex_t workerlock;
pthread_mutex_t sequencelock;
pthread_mutex_t fsequencelock[MAX_FILES];
pthread_mutex_t commandlocks[MAX_FILES];
pthread_cond_t sequencecond;
pthread_cond_t fsequencecond[MAX_FILES];

file_tracker tracklist[MAX_FILES];
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
void fsequence_check(int trackslot_index, int sequence_id_filequeue);
void log_commands(argument *arg);
argument *parse_input(char *input, int i);

/* worker_thread function
 * @param void*(argument* ) argument pointer type casted to a void pointer.
 * @ret NULL thread_exits at the en
 */
void *worker_thread(void *arg)
{
    // initialize local variables
    int sequence_id;
    int trackslot_index;
    int sequence_id_filequeue;

    argument *thread_info = (argument *)arg;    // typecast the void pointer into the correct one: argument typedef

    // Fixes sequence of thread

    pthread_mutex_lock(&workerlock);     // Acquire a lock

    // use a condition variable to fix up the sequence of threads:
    while (thread_info->sequence_id != thread_sequence)
    {
        pthread_cond_wait(&sequencecond, &workerlock);
    }

    // tell waker that it shouldnt send a signal first
    next = 0;

    // ask the tracking array if there is a slot for this thread
    // if there is, then queue up this thread on that slot grab a sequence_id for that slot
    // and increment the sequence_id readying it for the next possible thread that might also go into the same slot
    trackslot_index = tracklist_check(thread_info->dir);
    sequence_id_filequeue = tracklist[trackslot_index].sequence_id;
    tracklist[trackslot_index].sequence_id++;


    thread_sequence++;
    next = 1;                       // tell waker that it can send a signal to the condition variable to wake up the next waiting thread.

    pthread_mutex_unlock(&workerlock);     // release the lock for this section


    fsequence_check(trackslot_index, sequence_id_filequeue);    // check if it's the thread's turn in the trackerslot to be executed.


    // check commandtype and execute commands accordingly
    pthread_mutex_lock(&commandlocks[trackslot_index]);

    file_open_simulation();
    if (thread_info->commandtype == WRITE)
    {
        printf("%s %s %s on slot %d | active files: %d | t_id: %d | fseq_id: %d\n", thread_info->command, thread_info->dir, thread_info->string, trackslot_index, activefiles, thread_info->sequence_id, sequence_id_filequeue);
        write_command(thread_info->wholecommand, thread_info->dir, thread_info->string);
    }
    else
    {

        if (thread_info->commandtype == READ)
        {
            printf("%s %s on slot %d | active files: %d\n", thread_info->command, thread_info->dir, trackslot_index, activefiles);
            read_command(thread_info->wholecommand, thread_info->dir);
        }

        if (thread_info->commandtype == EMPTY)
        {
            printf("%s %s on slot %d | active files: %d\n", thread_info->command, thread_info->dir, trackslot_index, activefiles);
            empty_command(thread_info->wholecommand, thread_info->dir);
        }
    }
    pthread_mutex_unlock(&commandlocks[trackslot_index]);

    tracklist[trackslot_index].current_access++;
    tracklist[trackslot_index].fnext = 1;

    release_filespot(trackslot_index);     // releases the spot in the tracking array

    // frees the allocated memory for the argument data structure
    free(arg);

    // thread exits
    pthread_exit(NULL);
}

/* 
 * master_thread function
 *
 * @param void*(argument*) arg - argument pointer type casted to a void pointer.
 * @ret NULL (thread_exits at the end)
 * @func The master_thread's job is to accept inputs in a non-blocking manner
 *       and continuously spawn new threads for each of the commands that are inputted
 *       it converts the input into the argument data structure using the parse_input helper function.
 */
void *master_thread(void *arg)
{
    puts("FILESERVER READY TO ACCEPT INPUTS");

    int i = 0;
    char input[126];

    while (1)
    {

        argument *arg = malloc(sizeof(argument));
        if (fgets(input, 126, stdin) == NULL) // HANDLE EOF
            break;
        if (strlen(input) < 4)
            continue;

        arg = parse_input(input, i);
        if (arg->commandtype == INVALID)
            continue; // HANDLE INVALID INSTRUCTIONS

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

/* waker function
 * @param void* but isn't passed any argument
 * @ret NULL
 * @func for the waker thread
 *      This thread's job is only to check if the next / fnext variables are == 1.
 *      If so, it will call the signal for the condition variable to wake up the threads that are waiting on it
 */

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

/* 
 * Main will initialize all the semaphores, mutexes, and condition variables that will be used.
 * More over, it will spawn the master and the waker threads.
 */

int main()
{
    // Semaphore, mutex, and condition variable init
    pthread_mutex_init(&workerlock, NULL);
    pthread_mutex_init(&sequencelock, NULL);
    pthread_cond_init(&sequencecond, NULL);

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
    pthread_create(&tid, NULL, &waker, NULL);

    // Calling join so that the program will not end until the spawned threads die (which is never).
    pthread_join(tid, NULL);

    return 0;
}

// HELPER FUNCTIONS

/*
 * parse_input function
 *
 * @param char*, int
 * @ret argument pointer
 * @func parses the input from passed from master_thread and
 *       composes an argument struct from the parsed input
 */

argument *parse_input(char *input, int i)
{
    char *command;
    char *dir;
    char *string;
    argument *arg = malloc(sizeof(argument));

    // since we used fgets which includes the \n at the end, we need to remove it
    int input_size = strlen(input);
    input[input_size-1] = (input[input_size-1] == '\r' || input[input_size-1] == '\n')  ? 0 : input[input_size-1];
    input[input_size-2] = (input[input_size-2] == '\r' || input[input_size-2] == '\n') ? 0 : input[input_size-2];


    arg->commandtype = INVALID;       // commandtype initially invalid if not overwritten then we know it's invalid.
    strcpy(arg->wholecommand, input); // create a copy of the whole command for the arg struct

    command = strtok(input, " "); // grabs the command using strtok

    if (strcmp(command, "write") == 0) // if write we need to do 2 strtoks since we have a string argument
    {
        dir = strtok(NULL, " ");     // grabs directory
        string = strtok(NULL, "");   // grabs string
        strcpy(arg->string, string); // copies string to the arg struct
        arg->commandtype = WRITE;    // sets command type to WRITE
    }
    else // if not write we only need to grab the directory
    {
        dir = strtok(NULL, ""); // grabs directory from the input string
        if (strcmp(command, "read") == 0)
            arg->commandtype = READ; // if read: set commandtype to read
        if (strcmp(command, "empty") == 0)
            arg->commandtype = EMPTY; // if empty: set commandtype to empty
    }

    strcpy(arg->command, command); // copies command and directory to the arg struct
    strcpy(arg->dir, dir);
    arg->sequence_id = i; // gives the thread argument and id to let it know
                          // the order they have been created (then executed)

    return arg;
}
/*
 * log_commands function
 *
 * @param argument struct
 * @ret NULL
 * @func opens commands.txt and logs the command accepted by the master_thread
 *       logs using ctime() format
 */

void log_commands(argument *arg)
{
    FILE *fptr;     // creates a FILE pointer
    time_t curtime; // creates a time_t variable

    if ((fptr = fopen("commands.txt", "a")) == NULL) // opens commands.txt in append mode
    {
        fprintf(stderr, "LOGGING COMMAND FAILED"); // if it cannot open the file, output message on stderr
    }

    time(&curtime);                        // grabbing current time
    char *c_time_string = ctime(&curtime); // creates formatted timestamp using ctime()

    int time_length = strlen(c_time_string);
    c_time_string[time_length-1] = 0;
    fprintf(fptr, "%s: %s\n", c_time_string, arg->wholecommand); // appends log

    fclose(fptr);
}

/*
 * fsequence_check function
 *
 * @param int trackslot_index and int sequence_id_filequeue
 * @ret NULL
 * @func checks if it's the thread's turn in the tracker array's slot to execute
 *       else it sleeps in the cond variable for that tracker slot
 */

void fsequence_check(int trackslot_index, int sequence_id_filequeue)
{
    pthread_mutex_lock(&fsequencelock[trackslot_index]); // grabs lock for the array

    // check if it's the thread's turn on the tracker array slot
    while (sequence_id_filequeue != tracklist[trackslot_index].current_access)
        pthread_cond_wait(&fsequencecond[trackslot_index], &fsequencelock[trackslot_index]);

    // change fnext of the tracker slot to make the waker function stop signaling for that slot
    tracklist[trackslot_index].fnext = 0;
    pthread_mutex_unlock(&fsequencelock[trackslot_index]);
}

/*
 * file_tracker_init function
 *
 * @param file_tracker*
 * @ret NULL
 * @func tracker by setting users = 0 (no users yet)
 */

void file_tracker_init(file_tracker *tracker)
{
    tracker->users = 0; // set the tracker slot's users attribute to 0
}

/*
 * file_open_simulation function
 *
 * @param None
 * @ret NULL
 * @func makes the thread sleep 1s (80% chance) or 6s (20% chance)
 */

void file_open_simulation()
{
    srand(time(0));                // sets seeding function
    int SLEEP_TOSS = rand() % 100; // sets the random number to rand() mod 100
                                   //      to get a random number from [0,99]

    if (SLEEP_TOSS >= 80) // if (80-99) then sleep for 6s (20%)
        sleep(6);
    else
        sleep(1); // if (0-79) then sleep for 1s (80%)
}

/*
 * log_error function
 *
 * @param string command, string dest, string contents
 * @ret NULL
 * @func allows the user to write a string to a file
 */

void log_error(char *command, char *dest, char *contents)
{
    FILE *fptr; // intializes FILE pointer

    if ((fptr = fopen(dest, "a")) == NULL) // opens destination file in append mode
    {
        fprintf(stderr, "ERROR LOGGING\n"); // if error occurs, print out error message to stderr
        return;                             // stop function exec
    }

    fprintf(fptr, "%s: %s\n", command, contents); // write (string) contents to the destination file
    fclose(fptr);                                 // close file
}

/*
 * transfer_content function
 *
 * @param string command, string dest, FILE pointer fp1
 * @ret NULL
 * @func transfers the contents of the file pointed to by the fp1 parameter to the destination file
 */

void transfer_content(char *command, char *dest, FILE *fp1)
{

    FILE *fptr;               // opens a new file designated to *fptr
    char buffer[BUFFER_SIZE]; // initializes a buffer for string transfer

    if ((fptr = fopen(dest, "a")) == NULL) // opens destination file on append mode
    {
        fprintf(stderr, "ERROR READING\n"); // if error then flush out error message in stderr
        return;
    }

    fprintf(fptr, "%s: ", command); // writing the necessary command: actualcommand formatting

    while (fgets(buffer, BUFFER_SIZE, fp1) != NULL) // GRABBING CONTENTS CONTINUOUSLY UNTIL NULL
    {
        fprintf(fptr, "%s", buffer); // write the contents grabbed from fp1 to the destination file using the fptr pointer
    }
    fprintf(fptr, "\n"); // adds a newline for formatting
    fclose(fptr);        // close file pointer
}

/*
 * write_command function
 *
 * @param string wholecommand, string dest, string string
 * @ret NULL
 * @func transfers the contents of the file pointed to by the fp1 parameter to the destination file
 */

void write_command(char *wholecommand, char *dest, char *string)
{
    FILE *fptr;                       // initialize FILE pointer
    int STRING_SIZE = strlen(string); // get the string size for later use
    int i;                            // initializer iterator variable for the for loop

    if ((fptr = fopen(dest, "a")) == NULL) // opens destination file in append mode
    {
        fprintf(stderr, "Error opening\n"); // if error opening, flushout error message
        return;
    }

    for (i = 0; i < STRING_SIZE; i++) // for loop to write each character one by one to the destination file
    {
        fputc(string[i], fptr); // writes character to the dest file
        usleep(25);             // sleeps for 25ms
    }
    fclose(fptr); // close file
}

/*
 * read_command function
 *
 * @param string wholecommand, string dest
 * @ret NULL
 * @func opens destination file and transfers content to read.txt
 */

void read_command(char *wholecommand, char *dest)
{
    FILE *fptr;                            // initialize FILE pointer
    if ((fptr = fopen(dest, "r")) == NULL) // opens destination file in append mode
    {
        log_error(wholecommand, "read.txt", "FILE DNE"); // if error, write FILE DNE in read.txt
        return;
    }
    transfer_content(wholecommand, "read.txt", fptr);
    fclose(fptr);
}

/*
 * empty_command function
 *
 * @param string wholecommand, string dest
 * @ret NULL
 * @func opens destination file and transfers content to read.txt
 */
void empty_command(char *wholecommand, char *dest)
{
    FILE *fptr;                            // initialize FILE pointer
    if ((fptr = fopen(dest, "r")) == NULL) // opens destination file in append mode
    {
        log_error(wholecommand, "empty.txt", "FILE ALREADY EMPTY"); // if error, write FILE ALREADY EMPTY in empty.txt
        return;
    }

    transfer_content(wholecommand, "empty.txt", fptr); // transfers content to empty.txt
    fclose(fopen(dest, "w"));                          // empties file by opening using write mode then close it

    usleep(7000 + rand() % 3001); // sleeps for 7-10 seconds
    return;
}

/*
 * tracklist_check function
 *
 * @param string dir
 * @ret int trackslot_index
 * @func checks the tracker array if there are any other threads
 *       that is also going to perform a command on the same directory
 *       if there is: queue up in the same slot and increment users of that slot
 *       if none: then it will check for a slot in the tracker array
 *          if a slot is found: it will claim the slot by putting its directory in the
 *                              filename attribute of the slot
 *          if no slot is found: it will sleep in the semaphore protecting the array
 */

int tracklist_check(char *dir)
{
    int trackslot_index = -1; // initialize the index to -1 so we know we havent acquired a slot yet

    while (1) // loop until we find a slot
    {
        for (int i = 0; i < MAX_FILES; i++) // linear search for a slot while searching for a active slot with the same file directory
        {
            if (trackslot_index < 0)           // if no slot found yet
                if ((tracklist[i].users == 0)) // if current slot has no users (not used)
                {
                    trackslot_index = i; // set the index of this thread to that slot's index
                }
            // NOTE: we dont break out of the loop since we need to check
            //       the entire array if there exists an active thread using the same file directory

            if (strcmp(dir, tracklist[i].filename) == 0) // check if current slot's filename is the same with the current threads'
            {
                trackslot_index = i;                  // if so: set index to the current slot's
                ++(tracklist[trackslot_index].users); // increment the users of the current file

                return trackslot_index; // return the index
            }
        }

        // This part is only reached if no same file directory is active in our tracker array

        sem_wait(&filequeue); // We wait on the semaphore protecting this tracker array
                              // if more files than the set size of our tracker is waiting
                              // the files that will wait after the limit is reached will sleep

        if (trackslot_index >= 0) // proceeding, we check if the thread has found a slot
        {
            activefiles++;        // if it does, we increment the number of active files for analytics
            break;                // we break out of our searching loop
        }
        sem_post(&filequeue);
    }

    strcpy(tracklist[trackslot_index].filename, dir); // we set the filename attribute of the slot to the current thread's
    ++(tracklist[trackslot_index].users);             // increment the number of users of the slot

    return trackslot_index;
}

void release_filespot(int index)
{
    // get the lock so that there will be no other threads that will try to get a slot in the filetracker

    --(tracklist[index].users); // subtract users from the current slot

    // if it's the last user for that slot, reset the sequence_id and current_access so that it'll be usable for another file
    if (tracklist[index].users == 0)
    {
        activefiles--;
        tracklist[index].current_access = 0;
        tracklist[index].sequence_id = 0;
    }
    // send a sem_post so that if there are waiting threads they can check if there is a slot
    sem_post(&filequeue);

}
