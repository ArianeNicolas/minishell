#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <stdlib.h>
#include "parser.h"
#include <signal.h>

//Struct background process and all its data
typedef struct background_process
{
	int ncommands;
	char *command; //command(s) to execute
	int pid; //local pid
	char *status; //Running or Done
	int id_to_wait; //global pid
	struct background_process* next; //next in the list
} background_process;


//GLOBAL VARIABLES
background_process *list_bg_processes = NULL; //list of bachground processes
int bg_pid = 1; //inicialization of the local pid

//---------------------------

/**
 * Returns a new background_process with a command to execute, a pid, a status, and which is linked to the list
*/
background_process* new_background_process(background_process **head, tline *command)
{
	background_process *new_bg_pc = (background_process *)malloc(sizeof(background_process));
	background_process *current;
	int i, j;

	new_bg_pc->status = strdup("Running");

	//copy the command with strdup() for each arg
	new_bg_pc->command = strdup(command->commands[0].argv[0]);
	for(i = 1; i<command->commands[0].argc; i++){
		new_bg_pc->command = strcat(new_bg_pc->command, " ");
		new_bg_pc->command = strcat(new_bg_pc->command, command->commands[0].argv[i]);
	}
	if(command->ncommands > 1){
		new_bg_pc->command = strcat(new_bg_pc->command, " |");
		for(i = 1; i<command->ncommands-1; i++){
			for(j = 0; j<command->commands[i].argc; j++){
				new_bg_pc->command = strcat(new_bg_pc->command, " ");
				new_bg_pc->command = strcat(new_bg_pc->command, command->commands[i].argv[j]);
			}
			new_bg_pc->command = strcat(new_bg_pc->command, " |");
		}
		for(i = 0; i<command->commands[command->ncommands - 1].argc; i++){
			new_bg_pc->command = strcat(new_bg_pc->command, " ");
			new_bg_pc->command = strcat(new_bg_pc->command, command->commands[command->ncommands - 1].argv[i]);
		}
	}

	//local pid assignation
	new_bg_pc->pid = bg_pid;
	bg_pid++;

	//link to the list
	new_bg_pc->next = NULL;
    if ((*head) == NULL) {
        (*head) = new_bg_pc;
    } else {
        current = *head;
        while (current->next != NULL) {
            current = current->next;
        }
        current->next = new_bg_pc;
    }

	return new_bg_pc;
}

/**
 * Free a background process, by freeing each component that has been allocated with malloc()
*/
void free_process(background_process *temp)
{
	free(temp->command);
	free(temp->status);
	free(temp);
}

/**
 * Check if there is a file for the input, verify if it is valid, and open it in read mode if so
*/
int redirect_input(char* file)
{
	if(file != NULL){ 
		if(access(file, F_OK)==0){
			freopen(file, "r", stdin);
			return 0;
		}
		else{
			return 1;
		}
	}
	return 0;
}

/**
 * Check if there is a file for the output and the error, and open them in write mode if so
 * Creates a new file if it doesn't exist
*/
void redirect_output(char *output, char *error)
{
	if(output != NULL){
		freopen(output, "w", stdout);
	}
	if(error != NULL){
		freopen(error, "w", stderr);
	}
}

/**
 * Execute a line
 * In case of a background command, returns the global pid of the background process 
*/
int execute(tline *line)
{
	int nb_com = line->ncommands;
	int i, j;

	//a pipe for each son
	int p[nb_com][2];

	//the forks pids will be stored in an array
	int hijos[nb_com];

	
	//initialization of the pipes
	for(i = 0; i<nb_com; i++){
		pipe(p[i]);
	}

	for(i = 0; i<nb_com; i++){
		hijos[i] = fork();
		if( hijos[i] == 0 ) //We are in the son
		{
			//turning the Ctrl + C and Ctrl + \ back
			signal(SIGINT, SIG_DFL); 
			signal(SIGQUIT, SIG_DFL);

			if(i == 0){//first command

				//checks the input 
				if(redirect_input(line->redirect_input)==1){
					printf("ERROR: this file does not exist\n");
					return 1;
				};

				if(nb_com == 1){//only one command -> checks the output and execute
					redirect_output(line->redirect_output, line->redirect_error);
					execvp(line->commands[0].argv[0], line->commands[i].argv);
				}
				//if more than one command, redirects the output in the pipe
				dup2(p[i][1],1);
				close(p[i][0]);
			}

			else if(i == nb_com-1){//Last command
				redirect_output(line->redirect_output, line->redirect_error);
				//redirects the input from the previous pipe
				dup2(p[i-1][0],0);
				close(p[i-1][1]);
			}

			else{
				//redirects input and output
				dup2(p[i-1][0],0);
				close(p[i-1][1]);
				dup2(p[i][1],1);
				close(p[i][0]);
			}

			//close all the unused pipes
			for (j = 0; j < nb_com - 1; j++) {
                if (j != i - 1 && j != i)
                    close(p[j][0]), close(p[j][1]);
            }
			execvp(line->commands[i].argv[0], line->commands[i].argv);
		}
	}
	//in the main process : 

	//close the pipes
	for (i = 0; i < nb_com - 1; i++) {
		close(p[i][0]);
		close(p[i][1]);
		
	}

	//if it's a background process : doesn't wait and returns the pid
	if(line->background){
		return hijos[nb_com-1];
	}
	else{
		//waits for each son to finish
		for(i = 0; i < nb_com; i++) {
			waitpid(hijos[i], NULL, 0);
		}
    	return 0;
	}
}

/**
 * Returns whether the command is a valid command or not
*/
bool invalid_command(tline * line)
{
	int i;

	for(i = 0; i<line->ncommands; i++){
		if(line->commands[i].filename == NULL) {
			printf("command %s: command not found\n", line->commands[i].argv[0]);
			return true;
		}
	}
	return false;
}

/**
 * Changes the working directory
*/
void cd(char* arg)
{
	char* home = getenv("HOME");
	char buffer[512];

    if(strlen(arg) == 0){ //no arg -> switch to HOME
		chdir(home);
        printf("%s\n", home);
	}
	else{
		if (chdir(arg) != 0) {
			printf("ERROR: this folder does not exist\n");
		} 
		else {
			printf("%s\n", getcwd(buffer,-1));
		}
	}
}

/**
 * Put a background process in foreground according to the id in parameter,
 * remove it from the list and free its data
*/
void fg(const char* id)
{
	int pid;
	background_process *current = list_bg_processes;
	background_process *next;

	if(strlen(id) == 0){
		pid = bg_pid-1;//if no arg, we wake up the last process
	}
	else {
		pid = atoi(id);
	}

	if(current == NULL) printf("msh: fg: no background jobs\n"); //no list

	else if(current->pid == pid){ //the process to wake up is the first of the list
		if(current->next == NULL) {
			bg_pid = current->pid;
		}
		if(strcmp(current->status, "Done") == 0) printf("msh: fg: job has terminated\n");
		list_bg_processes = current->next;
		waitpid(current->id_to_wait, NULL, 0);
		free_process(current);
	}
	else {
		next = current->next;
		while (next != NULL && next->pid != pid) {
			current = next;
			next = next->next;
		}
		if(next == NULL){ //we are at the end of the list
			printf("msh: fg: no such job (id not found)\n");
		} 
		else { //we found the right id
			current->next = next->next;
			if(next->next == NULL) {
				bg_pid = current->pid + 1;
			}	
			if(strcmp(next->status, "Done") == 0) printf("msh: fg: job has terminated\n");
			waitpid(next->id_to_wait, NULL, 0);
			free_process(next);
		}
	}
}

/**
 * Displays info about backgournd processes
 * if a job is done, removes it
*/
void jobs()
{
	background_process *current = list_bg_processes;
	background_process *next;

	if(current == NULL) return; //no list
	else{
		// begins by removing all the Done processes of the beginning of the list
		while(current != NULL && strcmp(current->status, "Done") == 0){
			printf("[%i] %s		%s\n",current->pid, current->status, current->command);
			list_bg_processes = current->next;
			free_process(current);
			current = list_bg_processes;
		}
		if(current == NULL) return;
		else{
			printf("[%i] %s		%s\n",current->pid, current->status, current->command);
			next = current->next;
			while (next != NULL) {
				printf("[%i] %s		%s\n",next->pid, next->status, next->command);
				if(strcmp(next->status, "Done") == 0){
					current->next = next->next;
					free_process(next);
					next = current->next;
				}
				else{
					current = next;
					next = next->next;
				}
			}
		}
	}
}

/**
 * Quits properly the program by freeing every background process
*/
void quit_properly()
{
	background_process *current = list_bg_processes;
	background_process *temp;

    while (current != NULL) {
		kill(current->id_to_wait, SIGINT); //kill the process
        temp = current;
		current = current->next; //remove it frome the list
		free_process(temp); //and free it
    }
}

/**
 * Signal handler for SIGCHLD -> when a child process finishes
 * Change its status to Done
*/
void sigchld_handler()
{
    pid_t pid;
    int   status;
	background_process *current;

    while ((pid = waitpid(-1, &status, WNOHANG)) > 0)
    {
        current = list_bg_processes;
		while (current != NULL) {
			if(current->id_to_wait == pid){
				free(current->status);
				current->status = strdup("Done");
			}
			current = current->next;
		}
    }
}

/**
 * Principal function, loops over the user entries, verify if the command is valid, calls the others functions
*/
int main(void) 
{
	char buf[1024];
	tline * line;
	int argc;
	background_process* pc;

	//definition of the behavior in case of signal received
	signal(SIGINT, SIG_IGN); 
	signal(SIGQUIT, SIG_IGN); 
	signal(SIGCHLD, sigchld_handler);

	//loop
	printf("msh > ");	
	while (fgets(buf, 1024, stdin)) {
		line = tokenize(buf);
		if (line->ncommands==0){
			printf("msh > ");
			continue;
		}
		//checks if the command is an intern command (cd, fg, jobs, quit)
		else if(strcmp("cd", line->commands[0].argv[0]) == 0){
			argc = line->commands[0].argc;
			if(argc == 1) {
				cd("");
			}
			else if(argc == 2) {
				cd(line->commands[0].argv[1]);
			}
			else {
				printf("msh: cd: too many arguments");
			}
		}
		else if(strcmp("fg", line->commands[0].argv[0]) == 0){
			argc = line->commands[0].argc;
			if(argc == 1) {
				fg("");
			}
			else if(argc == 2) {
				fg(line->commands[0].argv[1]);
			}
		}
		else if(strcmp("jobs", line->commands[0].argv[0]) == 0){
			jobs();
		}
		else if(strcmp("quit", line->commands[0].argv[0]) == 0 || strcmp("q", line->commands[0].argv[0]) == 0){
			quit_properly();
			return 0;
		}

		//checks if the command is valid
		else if(!invalid_command(line)){
			//then if the process is executed in background
			if(line->background){
				pc = new_background_process(&list_bg_processes, line);
				pc->id_to_wait = execute(line); //executes the line
				printf("[%i] 	%i\n",pc->pid, pc->id_to_wait);
			}
			else {
				execute(line); //executes the line
			}
		}
		printf("msh > ");	
	}
	return 0;
}