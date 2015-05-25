#include <stdio.h>
#include <stdlib.h>
#include <linux/limits.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <unistd.h>
#include <string.h>
#include <sys/queue.h>
#include <sys/wait.h>


/*************
* Struct to hold Processes
* Includes: pid,pname,jobnum, status
*************/
typedef struct{
	struct process *next;
	struct process *prev;
	pid_t processid;
	char *processname;
	int jobnumber;
	char status[10];
}process;

/*************
* Initialize head and tail of
* Processes Linked List
*************/
process *head = NULL;
process *curr = NULL;

// Variable to detect when a process finishes
//Reference: http://www.makelinux.net/alp/026
sig_atomic_t child_exit_status;

/*************
* Signature: 
* 	process * findProcess(pid_t pid);
* Accepts: pid
* Action: Returns process with pid
*************/
process * findProcess(pid_t pid){
	process* finder = head;
	while (finder != NULL){
		if(finder->processid == pid){
			return finder;
		}
	finder = (process*)finder->next;
	}
	return NULL;
}

/*************
* Signature: 
* 	void printProcesses();
* Accepts: nothing
* Action: prints all processes in processes list
*************/
void printProcesses(){
	process *toprint = head;
	if(toprint == NULL){
		printf("No Processes Currently Running\n");
	}
	while(toprint != NULL){
		printf("Processid: %i        Name: %s         Jobid:%i       Status:%s\n",
			toprint->processid,toprint->processname,toprint->jobnumber, toprint->status);
		toprint = (process *)toprint->next;
	}
}

/*************
* Signature: 
* 	void emptylist();
* Accepts: nothing
* Action: Emptys and frees all pointers in processes list
*************/
void emptylist(){
//	curr = head;
	process *tofree = head;
	while(head != NULL){
		tofree = head;
		pid_t pid = tofree->processid;
		kill(pid,SIGKILL);
		head = (process *)head->next;
		free(tofree);
		fprintf(stderr,"Process with id %i forcibly closed\n",tofree->processid);
	}
}


/*************
* Signature: 
* 	void addProcess(pid_t pid, char* name, int *jobnum);
* Accepts: pid, name, jobnum
* Action: adds process with above info at end of processes list
*************/
void addProcess(pid_t pid, char* name, int *jobnum){

	*jobnum += 1;
	process *prev;
	if(head == NULL){
		head = malloc(sizeof(process));
		if(head == NULL) exit(1);

		curr = (process *)head;
	}else{
		curr->next = malloc(sizeof(process));
		if(curr->next == NULL) exit(1);

		prev = (process *)curr;
		curr = (process *)prev->next;
		curr->prev = (void *)prev;
	}
	curr->processid = pid;
	curr->processname = malloc(sizeof(strlen(name)+1));
	if(curr->processname == NULL) exit(1);

	strcpy(curr->processname,name);
	curr->jobnumber = *jobnum;	
	curr->next = NULL;
	strcpy(curr->status, "Running");

	 
}

/*************
* Signature: 
* 	void pauseProcess(pid_t pid);
* Accepts: pid
* Action: pauses process with above id
*************/
void pauseProcess(pid_t pid){
	process* topause = findProcess(pid);
	if (topause == NULL){
		fprintf(stderr,"Process %i not found",pid);
		return;
	}
	if(strcmp(topause->status, "Running") == 0){
		strcpy(topause->status, "Paused");
		kill(pid, SIGSTOP);
		fprintf(stderr,"Process %i has been paused\n",pid);
	}else{
		fprintf(stderr,"Process %i is not currently running\n",pid);
	}
}

/*************
* Signature: 
* 	void resumeProcess(pid_t pid);
* Accepts: pid
* Action: resumes process with above id
*************/
void resumeProcess(pid_t pid){
	process* toresume = findProcess(pid);
	if (toresume == NULL){
		fprintf(stderr,"Process %i not found",pid);
		return;
	}
	if(strcmp(toresume->status, "Paused") == 0){
		strcpy(toresume->status, "Running");
		kill(pid, SIGCONT);
		fprintf(stderr,"Process %i has been resumed\n",pid);
	}else{
		fprintf(stderr,"Process %i is currently running\n",pid);
	}
}

/*************
* Signature: 
* 	void removeProcess(pid_t pid);
* Accepts: pid
* Action: removes process with above id from processes list
*************/
int removeProcess(pid_t pid){
	process* toremove = findProcess(pid);
	if (toremove == NULL) return 0;
	if (toremove == head && head->next != NULL){
		head = (process *)head->next;
	}else if(toremove == head && head == curr){
		head = NULL;
		curr = NULL;
	}else if(toremove == curr){
		curr = (process *)curr->prev;
		curr->next = NULL;
	}else{
		process* prev = (process *)toremove->prev;
		process* next = (process *)toremove->next;
		prev->next = toremove->next;
		next->prev = toremove->prev;
	}
	free(toremove);
	return 1;
}

/*************
* Signature: 
* 	void clean_up_child_process (int signal_number);
* Accepts: signal
* Action: Handler, removes process from list when process completes
*************/

//Reference: http://www.makelinux.net/alp/026
void clean_up_child_process (int signal_number){
	pid_t pid;
	while((pid = waitpid((pid_t)(-1), NULL, WNOHANG)) > 0){
		removeProcess(pid);
		fprintf(stderr,"Process with id %i completed\n",pid);
	}
}

/*************
* Signature: 
* 	void killProcess(pid_t pid);
* Accepts: pid
* Action: sends temination signal to process
*************/
int killProcess(int processid){
	pid_t pid = (pid_t) processid;
	int success = removeProcess(pid);
	if(success == 0) fprintf(stderr,"No Process with id %i\n",pid);
	if(success == 1){
		kill(pid, SIGKILL);
	}
	
}



/*************
* Signature: 
* 	int main(void);
* Accepts: nothing
* Action: main control flow
*		a) accept user input
*		b) perform action based on input
*************/
int main(void){

	//Init Variables
	char *user_input = NULL;
	int bailout = 0;
	char *homeDir = getenv("HOME");
	int jobnum = 0;

	//Define Signal Handler
	struct sigaction sigchld_action;
	memset(&sigchld_action, 0, sizeof(sigchld_action));
	sigchld_action.sa_handler = &clean_up_child_process;
	sigaction(SIGCHLD, &sigchld_action, NULL);

	//While user has not quit
	while(bailout == 0){

		//Init variables
		char ** results = NULL;
		char * input_args;
		char prompt[PATH_MAX + 10];
		int num_args = 0;
		int bg_tag = 0;
		pid_t pid;
		char *currentDir =getcwd(NULL,0);

		//Create Prompt for user
		strcpy(prompt,"RSI: ");
		strcat(prompt,currentDir);
		strcat(prompt, " >");

		//Prompt user for input
		user_input = readline(prompt);
		
		//Parse Input
		input_args = strtok(user_input, " ");

		//If starts with bg raise flag
		if (strcmp(input_args, "bg") == 0){
			bg_tag = 1;
			input_args = strtok(NULL," ");
		}//end if

		//if not commands raise error
		if(input_args == NULL){
			fprintf(stderr,"No command given\n");
			continue;
		}//end if
		
		//Parse Inputs
		while(input_args){
			results = realloc(results, sizeof(char*) * ++num_args);
			if (results == NULL) exit(-1);
			if (strncmp(input_args,"~",1)==0){
				char out[PATH_MAX + 10];
				input_args++;
				strcpy(out,homeDir);
				strcat(out,input_args);
				results[num_args-1] = out;
			}else{
				results[num_args-1] = input_args;
			}//end if
			input_args = strtok(NULL," ");
		}//end While
		
		//Add null string pointer to results (for use with execvp())
		results = realloc(results, sizeof(char*) * ++num_args);

		//If no information entered
		if (results == NULL) fprintf(stderr, "No Commands Given");
		results[num_args-1] = 0;
		
		//Check first input for commands		
		if(strcmp(results[0], "cd") == 0){
			if(results[1] == 0){
				fprintf(stderr,"No Directory Given\n");
				continue;
			}//end if
			int success;
			success = chdir(results[1]);
			if(success == -1){
				fprintf(stderr,"Failure\n");
				chdir(currentDir);
			}//end if
		}else if(strcmp(results[0], "quit") == 0){
			bailout = 1;
		}else if(strcmp(results[0], "bglist") == 0){
			printProcesses();
		}else if(strcmp(results[0], "kill") == 0){
			if(results[1] == NULL){
				fprintf(stderr, "No Process id Given");
			}//end if
			char* garbage = NULL;
    			int processid= strtol(results[1], &garbage, 0);
			if(processid == 0){
				fprintf(stderr, "%s is not a processid\n",results[1]);
				continue;
			}//end if
			killProcess(processid);
		}else if(strcmp(results[0], "resume") == 0){
			if(results[1] == NULL){
				fprintf(stderr, "No Process id Given");
			}//end if
			char* garbage = NULL;
    			int processid= strtol(results[1], &garbage, 0);
			if(processid == 0){
				fprintf(stderr, "%s is not a processid\n",results[1]);
				continue;
			}//end if
			resumeProcess(processid);
		}else if(strcmp(results[0], "pause") == 0){
			if(results[1] == NULL){
				fprintf(stderr, "No Process id Given");
			}//end if
			char* garbage = NULL;
    			int processid= strtol(results[1], &garbage, 0);
			if(processid == 0){
				fprintf(stderr, "%s is not a processid\n",results[1]);
				continue;
			}//end if
			pauseProcess(processid);
		}else{
			pid = fork();
			int status;
			if(pid == -1){
				fprintf(stderr,"Problem creating process\n");
			}else if( pid == 0 ){
				// 
				int success;
				success = execvp(results[0],results);
				if(success == -1){
					fprintf(stderr,"Process did not complete sucessfully\n");
					exit(1);
				}//end if
			}//end if
			
			//check tag if in background add process to list
			//else wait for process to complete
			if (bg_tag == 0){
				waitpid(pid,&status,0);
			}else{
				addProcess(pid,results[0],&jobnum);
			}//end if


		}//end if
		bg_tag = 1;
		//Reset memory for next loop
		free(input_args);
		free(user_input);
	}//end While

	//Terminate memory
	emptylist();
	fprintf(stderr,"Goodbye!\n");
	exit(EXIT_SUCCESS);
}//end main

