#include <stdio.h>
#include <stdlib.h>
#include <linux/limits.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <unistd.h>
#include <string.h>
#include <sys/queue.h>
#include <sys/wait.h>




typedef struct{
	struct process *next;
	struct process *prev;
	pid_t processid;
	char *processname;
	int jobnumber;
	char status[10];
}process;


process *head = NULL;
process *curr = NULL;

sig_atomic_t child_exit_status;

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

void emptylist(){
	curr = head;
	process *tofree = head;
	while(tofree != NULL){
		process *temp = tofree;
		fprintf(stderr,"Process with id %i forcibly closed\n",tofree->processid);
		tofree = (process *)tofree->next;
		killProcess(temp->processid);
		free(temp);	
	}
	free(head);
}


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

void clean_up_child_process (int signal_number){
	pid_t pid;
	while((pid = waitpid((pid_t)(-1), NULL, WNOHANG)) > 0){
		removeProcess(pid);
		fprintf(stderr,"Process with id %i completed\n",pid);
	}
}

int killProcess(int processid){
	pid_t pid = (pid_t) processid;
	int success = removeProcess(pid);
	if(success == 0) fprintf(stderr,"No Process with id %i\n",pid);
	if(success == 1){
		kill(pid, 9);
	}
	
}


int main(void)
{

	char *user_input = NULL;
	int bailout = 0;
	char *homeDir = getenv("HOME");
	int jobnum = 0;

	struct sigaction sigchld_action;
	memset(&sigchld_action, 0, sizeof(sigchld_action));
	sigchld_action.sa_handler = &clean_up_child_process;
	sigaction(SIGCHLD, &sigchld_action, NULL);

	while(bailout == 0){
		char ** results = NULL;
		char * input_args;
		char prompt[PATH_MAX + 10];
		int num_args = 0;
		int bg_tag = 0;
		pid_t pid;
		char *currentDir =getcwd(NULL,0);

		strcpy(prompt,"RSI: ");
		strcat(prompt,currentDir);
		strcat(prompt, " >");

		user_input = readline(prompt);
		
		input_args = strtok(user_input, " ");
		if (strcmp(input_args, "bg") == 0){
			bg_tag = 1;
			input_args = strtok(NULL," ");
		}
		if(input_args == NULL){
			fprintf(stderr,"No command given\n");
			continue;
		}
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
			}
			input_args = strtok(NULL," ");
		}

		results = realloc(results, sizeof(char*) * ++num_args);
		if (results == NULL) exit(-1);
		results[num_args-1] = 0;
		
		if(strcmp(results[0], "cd") == 0){
			if(results[1] == 0){
				fprintf(stderr,"No Directory Given\n");
				continue;
			}
			int success;
			success = chdir(results[1]);
			if(success == -1){
				fprintf(stderr,"Failure\n");
				chdir(currentDir);
			}
		}else if(strcmp(results[0], "quit") == 0){
			bailout = 1;
		}else if(strcmp(results[0], "bglist") == 0){
			printProcesses();
		}else if(strcmp(results[0], "kill") == 0){
			if(results[1] == NULL){
				fprintf(stderr, "No Process id Given");
			}
			char* garbage = NULL;

    			int processid= strtol(results[1], &garbage, 0);
			if(processid == 0){
				fprintf(stderr, "%s is not a processid\n",results[1]);
				continue;
			}

			killProcess(processid);
		}else if(strcmp(results[0], "resume") == 0){
			if(results[1] == NULL){
				fprintf(stderr, "No Process id Given");
			}
			char* garbage = NULL;

    			int processid= strtol(results[1], &garbage, 0);
			if(processid == 0){
				fprintf(stderr, "%s is not a processid\n",results[1]);
				continue;
			}

			resumeProcess(processid);
		}else if(strcmp(results[0], "pause") == 0){
			if(results[1] == NULL){
				fprintf(stderr, "No Process id Given");
			}
			char* garbage = NULL;

    			int processid= strtol(results[1], &garbage, 0);
			if(processid == 0){
				fprintf(stderr, "%s is not a processid\n",results[1]);
				continue;
			}

			pauseProcess(processid);
		}else{
			pid = fork();
			int status;
			if(pid == -1){
				fprintf(stderr,"Problem creating process\n");
			}else if( pid == 0 ){
				int success;
				success = execvp(results[0],results);
				if(success == -1){
					fprintf(stderr,"Process did not complete sucessfully\n");
					exit(1);
				}
			}else{
				if(bg_tag == 1){
					addProcess(pid,results[0],&jobnum);
				}	
			}

			if (bg_tag == 0){
				waitpid(pid,&status,0);
			}


		}//end if
		bg_tag = 1;
		free(input_args);
		free(user_input);
	}//end While

	emptylist();
	fprintf(stderr,"Goodbye!\n");
	exit(EXIT_SUCCESS);
}//end main

