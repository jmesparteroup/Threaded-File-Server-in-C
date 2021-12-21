#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>
#include <pthread.h>

void *masterthread();
void write_command (char* path, char *string);
void read_command (char* path, char *string);
void empty_command (char* path, char *string);

void *masterthread() {
	char buffer[100];
	char command[100];
	char argstr[100];
	char *args[100];

	while (1) {
		memset(buffer, 0, 100);
		memset(command, 0, 100);
		memset(argstr, 0, 100);
		memset(args, 0, 100);

		fgets(buffer, 100, stdin);
		sscanf(buffer, "%s %[^\n]s", command, argstr);

		args[0] = command;
		int idx = 1;
		args[idx++] = strtok(argstr, " ");
		while ((args[idx++] = strtok(NULL,  ""))) {}

		if (strcmp(command, "write") == 0) {
			write_command(args[1], args[2]);
			printf("args[2] = %s\n", args[2]);
		}
		else {printf("not write command\n");}
	}
}

void write_command(char* path, char *string) {
	printf("writing..\n");
	FILE *file = fopen(path, "a");
	fprintf(file, "%s\n", string);
	printf("..written\n");
}

int main() {
	pthread_t master_thread;

	pthread_create(&master_thread, NULL, masterthread, NULL);
	pthread_join(master_thread, NULL);
	return 0;
}


global linkedlist

masterthread:
    create new node
    new node(wholecommand)
    append to linkedlist

thread_creatorinator:
    while(1){
        pop linkedlist
        create new thread for instruction
        sleep(50)
    }
