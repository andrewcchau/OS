/*
Name: Andrew Chau
Lab: 1
Last Edit: 4:43 PM 9/19/17
*/

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h> //define S_IRUSR and S_IWUSR
#include <string.h>
#include <fcntl.h>
#include <stdbool.h>
#include <errno.h>

#define buffLength 2001

//global vars
int pipefd[2];
int status, pid_ch1, pid_ch2, pid, runningPid;
char buff[buffLength];
char* tokens[buffLength / 3];
bool recentlyStopped;
bool deleteOverride;

int savedSTDOUT, savedSTDIN, savedSTDERR;

//job manager vars
typedef struct job{
	int jobNo;
	int pid;
	int sisterPid;
	bool running;
	bool current;
	bool background;
	bool passed;
	char* argu;
} job;

job jobList[100];
int numberOfJobs = 0;
job job_default = {-1, -1, -1, false, false, false, false, 0};


//============================================JOBS========================================


//displays a list of jobs 
void displayJobs(){
	int i;
	for(i = 0; i < numberOfJobs; i++){
		printf("[%i]", jobList[i].jobNo);
		
		if(jobList[i].current == true){
			printf("+ ");
		}else{
			printf("- ");
		}

		if(jobList[i].running == true){
			printf("Running   ");
		}else{
			printf("Stopped   ");
		}

		printf("%s\n", jobList[i].argu);
	}
}

//used to make each job (to keep track of) and adds them to job list
void createJob(){
	char holder[2001];
	int ctr = 1;
	if(tokens[0] != NULL){
		strcpy(holder, tokens[0]);
	}
	while(tokens[ctr] != NULL){
		strcat(holder, " ");
		strcat(holder, tokens[ctr]);
		ctr++;
	}

	//find exact length of line
	ctr = 0;
	int len = 0;
	while(holder[ctr]){
		len++;
		ctr++;
	}

	//make the job
	jobList[numberOfJobs].running = true;
	jobList[numberOfJobs].argu = (char*) malloc((len + 1)*sizeof(char));
	strcpy(jobList[numberOfJobs].argu, holder);
	jobList[numberOfJobs].sisterPid = -1;
	jobList[numberOfJobs].current = true;
	jobList[numberOfJobs].background = false;
	jobList[numberOfJobs].passed = false;
	if(numberOfJobs == 0){
		jobList[numberOfJobs].jobNo = 1;
	}else{
		jobList[numberOfJobs].jobNo = jobList[numberOfJobs - 1].jobNo + 1;
	}

	//set other jobs to not be current
	ctr = 0;
	for(ctr = 0; ctr < numberOfJobs; ctr++){
		jobList[ctr].current = false;
	}

	//incr number of jobs
	numberOfJobs++;
	jobList[numberOfJobs] = job_default;
}

//deletes a specific job given the pid
void deleteJob(int id){
	int ctr = 0;
	bool found = false;
	for(ctr = 0; ctr < numberOfJobs; ctr++){
		//found pid, destroy job
		if(((jobList[ctr].pid == id && (jobList[ctr].sisterPid == -1 || jobList[ctr].passed)) || jobList[ctr].sisterPid == id) && !found){
			if(strstr(jobList[ctr].argu, "sleep") == 0 || jobList[ctr].passed || jobList[ctr].sisterPid == -1 || deleteOverride){	//checks for sleep piped into a command
				//print the done job if it was in the background
				if(jobList[ctr].background == true){
					printf("\n[%i]", jobList[ctr].jobNo);
					if(jobList[ctr].current == true){
						printf("+ ");
					}else{
						printf("- ");
					}
					printf("Done      %s\n", jobList[ctr].argu);
					fflush(stdout);
				}

				//delete job
				if(jobList[ctr].argu != 0){
					free(jobList[ctr].argu);
					jobList[ctr].argu = 0;
				}
				found = true;
				deleteOverride = false;
			}else{	
				//sleep was found at least once
				jobList[ctr].passed = true;
			}
		}
		//shift everything up one
		if(found){
			jobList[ctr].pid = jobList[ctr + 1].pid;
			jobList[ctr].running = jobList[ctr + 1].running;
			jobList[ctr].current = jobList[ctr + 1].current;
			jobList[ctr].background = jobList[ctr + 1].background;
			jobList[ctr].sisterPid = jobList[ctr + 1].sisterPid;
			jobList[ctr].argu = jobList[ctr + 1].argu;
			jobList[ctr].jobNo = jobList[ctr + 1].jobNo;
			jobList[ctr].passed = jobList[ctr + 1].passed;
		}
	}

	if(numberOfJobs > 0 && found){
		numberOfJobs--;

		//last job will be "current"
		jobList[numberOfJobs - 1].current = true;	
	}
}

//returns the index of the most recent stopped process, otherwise returns -1
int findStoppedProc(){
	int ctr;
	for(ctr = numberOfJobs - 1; ctr >= 0; ctr--){
		if(jobList[ctr].running == false){
			return ctr;
		}
	}

	return -1;
}


//=====================================SIGNALS===============================================

//quits the current foreground process (if one exists)
//DOESN'T exit shell
//DOESN'T print the process
static void sig_int(int signo) {
  	if(pid_ch1 != 0){
  		deleteOverride = true;
  		kill(-pid_ch1, SIGINT);
  	}
}

//stops the current foreground process
//DOESN'T print the process
static void sig_tstp(int signo) {
	if(pid_ch1 != 0 && jobList[numberOfJobs - 1].background == false){
		jobList[numberOfJobs - 1].running = false;
		recentlyStopped = true;
		kill(-pid_ch1, SIGTSTP);
	}
}

int markProcessStatus(int id, int status){
	if(id > 0){
		if(WIFSTOPPED(status)){
			//process has stopped
		}else{
			//process is completed
			deleteJob(id);
			if(WIFSIGNALED(status)){
				//process was terminated
			}
			return 0;
		}
		return -1;
	}else if(id == 0 || errno == ECHILD){
		return -1;
	}else{
	//	perror("waitpid");
		return -1;
	}
}

void sigchld_handler(int signum){
	int pid, status, serrno;
	serrno = errno;

	while (!recentlyStopped)
	{
		pid = waitpid (WAIT_ANY, &status, WNOHANG);

		if (pid < 0)
		{
			//perror ("waitpid handler");
			break;
		}
		if (pid == 0)
			break;
		markProcessStatus(pid, status);
	}
	errno = serrno;
}

//=================================================TOKENS=============================================


//grabs tokens that are separated by a space, will ignore last instance of '&'
void getTokens(){
	printf("# ");
	fgets(buff, buffLength, stdin);

	if(feof(stdin)){
		exit(0);
		printf("eof fail");				//should never print
	}

	//get rid of new line
	char* temp = strrchr(buff, '\n');    
	if(temp != NULL){
		*temp = 0;
	}

	//account for just hitting return
	if(buff[0] == 0){
		return;
	}

	//parse through tokens and store them
	char* token = strtok(buff, " ");
	int ctr = 0;
	while(token != NULL){
		tokens[ctr] = token;
		token = strtok(NULL, " ");
		ctr++;
	}
	tokens[ctr] = NULL;

	recentlyStopped = false;
}

void deleteTokens(){
	int i;
	for(i = 0; i < buffLength / 3; i++){
		tokens[i] = 0;
	}
}

//Used for debugging purposes. Displays tokens to the terminal
void displayTokens(){
	int ctr = 0;
	while(tokens[ctr] != NULL){
		printf("%s\n", tokens[ctr]);
		ctr++;
	}
}

//shifts the tokens from 'start' to place
//ex. [0 1 2 3 4 5 6 7] place = 2 start = 4
//result is [0 1 4 5 6 7]
void shiftTokens(int place, int start){
	while(tokens[start] != NULL){
		tokens[place] = tokens[start];
		start++;
		place++;
	}
	tokens[place] = NULL;
}

//redirects any input/output files as needed
//removes redirect tokens for execvp later on
int fileRedir(){
	if(feof(stdin)){
		exit(0);
	}
	int curr = 0; //current token
	int changed = 0;
	bool invalid = false;

	while(tokens[curr] != NULL){
		if(strcmp(tokens[curr], "<") == 0){ //file input
			//check if file exists
			if(access(tokens[curr + 1], F_OK) != -1){
				//exists
				int file = open(tokens[curr + 1], O_RDONLY);
				dup2(file, STDIN_FILENO);
				close(file);
			}else{
				//doesn't exist
				printf("Invalid file: %s\n", tokens[curr + 1]);
				invalid = true;
				break;
			}
			changed = 1;
		}else if(strcmp(tokens[curr], ">") == 0){ //file output
			int file = open(tokens[curr + 1], O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
			dup2(file, STDOUT_FILENO);
			close(file);
			changed = 1;
		}else if(strcmp(tokens[curr], "2>") == 0){ //stderr
			int file = open(tokens[curr + 1], O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
			dup2(file, STDERR_FILENO);
			close(file);
			changed = 1;
		}
		if(changed){
			shiftTokens(curr, curr + 2);
		}else{
			curr++;
			changed = 0;
		}
	}

	if(invalid){
		return -1;
	}else{
		return 1;
	}
}

void reset(){
	dup2(savedSTDERR, STDERR_FILENO);
	dup2(savedSTDOUT, STDOUT_FILENO);
	dup2(savedSTDIN, STDIN_FILENO);
}

//copies tokens from 'tokens' to dest
void copyTokens(char** dest, int start, int stop){
	int ctr = 0;
	while(tokens[start] != NULL && start <= stop){
		dest[ctr] = tokens[start];
		start++;
		ctr++;
	}
	dest[ctr] = NULL;
}

//=================================================SHELL=============================================

//the shell program
int shell(){
	if(feof(stdin)){
		exit(0);
	}

	int status;
	int counter;
	int background = 0;

	if(tokens[0] == NULL){
		return 1;
	}

	//most of this is from sig_ex3.c
	if(pipe(pipefd) == -1){
		perror("pipe");
		exit(-1);
	}

	//search for pipes (will only have to worry about a single pipe per line)
	int pipeLoc = 0;
	counter = 0;
	while(tokens[counter] != NULL){
		if(strcmp(tokens[counter], "&") == 0){
			background = 1;
			jobList[numberOfJobs - 1].background = true;
			tokens[counter] = NULL;
		}else if(strcmp(tokens[counter], "|") == 0){
			pipeLoc = counter;
		}
		counter++;
	}

	
	pid_ch1 = fork();
	if(pid_ch1 > 0){
		//parent
		if(strcmp(tokens[0], "jobs") != 0 && strcmp(tokens[0], "fg") != 0 && strcmp(tokens[0], "bg") != 0){
			jobList[numberOfJobs - 1].pid = pid_ch1;
			//jobList[numberOfJobs - 1].current = true;
		}


		if(pipeLoc != 0){
			pid_ch2 = fork();
			if(pid_ch2 > 0){
				//parent
				//close parent pipe
				close(pipefd[0]);
				close(pipefd[1]);
				int count = 0;

				if(strcmp(tokens[0], "jobs") != 0 && strcmp(tokens[0], "fg") != 0 && strcmp(tokens[0], "bg") != 0){
					jobList[numberOfJobs - 1].sisterPid = pid_ch2;
				}

				while(count < 2 && !background){
					runningPid = waitpid(-1, &status, WUNTRACED | WCONTINUED);

					if(runningPid == -1){
						perror("waitpid c2");
						exit(EXIT_FAILURE);
					}

					//status messages (won't need when turn in)
					if (WIFEXITED(status)) {
					//	printf("child %d exited (2), status=%d\n", pid, WEXITSTATUS(status));
						count++;
					} else if (WIFSIGNALED(status)) {
						//printf("child %d killed by signal %d\n", pid, WTERMSIG(status));
						count = 2;
					} else if(WIFSTOPPED(status)){
						break;
					}

					if(count == 2 && strcmp(tokens[0], "jobs") != 0 && strcmp(tokens[0], "bg") != 0){
						deleteJob(pid_ch2);
					}
				}

			}else{
				//child 2
				if(feof(stdin)){
					exit(0);
				}
				sleep(1);
				setpgid(0, pid_ch1);	//child 2 joins group whose id is the same as child 1's pid
				close(pipefd[1]);
				dup2(pipefd[0], STDIN_FILENO);
				char* args[buffLength / 5];
				copyTokens(args, pipeLoc + 1, pipeLoc + 200);

				int ret = execvp(args[0], args);
				if(ret != 0){
					exit(0);	//exit to prevent grandchildren if token isn't valid command
				}
			}
		}else{
			if(feof(stdin)){
				exit(0);
			}
			//parent
			int count = 0;
			int proc = -1;

			if(strcmp(tokens[0], "fg") == 0){
				if(numberOfJobs > 0){
					proc = jobList[numberOfJobs - 1].pid;
					if(jobList[numberOfJobs - 1].sisterPid != -1){
						kill(-proc, SIGCONT);
					}else{
						kill(proc, SIGCONT);
					}
					jobList[numberOfJobs - 1].running = true;
					jobList[numberOfJobs - 1].current = true;
					jobList[numberOfJobs - 1].background = false;
					if(jobList[numberOfJobs - 1].argu != NULL){
						printf("[%i]+ Running   %s\n", jobList[numberOfJobs - 1].jobNo, jobList[numberOfJobs - 1].argu);
					}
				}
				recentlyStopped = false;
				pid_ch1 = jobList[numberOfJobs - 1].pid;
			}else if(strcmp(tokens[0], "bg") == 0){
				proc = findStoppedProc();
				if(proc != -1){
					if(jobList[proc].sisterPid == -1){
						kill(jobList[proc].pid, SIGCONT);
					}else{
						kill(-jobList[proc].pid, SIGCONT);
					}
					jobList[proc].running = true;
					jobList[proc].background = true;
					if(jobList[proc].argu != 0){
						printf("[%i]", jobList[proc].jobNo);
						if(jobList[proc].current == true){
							printf("+ ");
						}else{
							printf("- ");
						}
						printf("Running   %s\n", jobList[proc].argu);
					}
					background = true;
				}
				recentlyStopped = false;
			}

			if(proc <= 0){
				proc = -1;
			}

			while(count < 1 && !background) {
				runningPid = waitpid(proc, &status, WUNTRACED | WCONTINUED);

				if (WIFEXITED(status)) {
					//printf("child %d exited (1), status=%d\n", pid, WEXITSTATUS(status));
					count++;
				} else if (WIFSIGNALED(status)) {
					//printf("child %d killed by signal %d\n", runningPid, WTERMSIG(status));
					count++;
				}else if (WIFSTOPPED(status)){
				//	printf("should be stopped\n");
					break;
				}

				//deletes completed/exited jobs
				if(count == 1 && strcmp(tokens[0], "jobs") != 0 && strcmp(tokens[0], "bg") != 0){
					deleteJob(proc);
				}

				//deletes lingering jobs after bg command but not after fg command
				if(proc != -1 && strcmp(tokens[0], "fg") != 0){
					deleteJob(proc);
				}
			} 

			// if(runningPid == -1){
			// 	perror("waitpid c1");
			// 	exit(EXIT_FAILURE);
			// }
		}
	}else{
		//child 1
		if(feof(stdin)){
			exit(0);
		}

		if(background || pipeLoc != 0){
			setpgid(0,0);	//setpgid(process id, group id). 
							//process = 0 (current proccess), group = 0 (create new group)
							//new group id will be process id
		}

		char* args[buffLength / 5];
		if(pipeLoc != 0){
			close(pipefd[0]); //closes read end
			dup2(pipefd[1], STDOUT_FILENO);
			copyTokens(args, 0, pipeLoc - 1);
		}else{
			copyTokens(args, 0, buffLength / 5);
		}

		if(strcmp(tokens[0], "jobs") == 0){
			displayJobs();
		}

		int ret = execvp(args[0], args);
		if(ret != 0){
			exit(0);	//exit to prevent grandchildren if token isn't valid command
		}
	}
	return 1;
}

//runs the shell
int main(void){
	//setup
	int status = 1;
	int temp;
	recentlyStopped = false;
	deleteOverride = false;
	if(signal(SIGCHLD, sigchld_handler) == SIG_ERR)
		printf("sigchld error\n");
	if (signal(SIGINT, sig_int) == SIG_ERR)
		printf("signal(SIGINT) error");
	if (signal(SIGTSTP, sig_tstp) == SIG_ERR)
		printf("signal(SIGTSTP) error");

	savedSTDIN = dup(STDIN_FILENO);
	savedSTDOUT = dup(STDOUT_FILENO);
	savedSTDERR = dup(STDERR_FILENO);

	do{
		getTokens();
	//	displayTokens();
		temp = fileRedir();			//file redirect first
		if(temp > 0){
			if(strcmp(tokens[0],"jobs") != 0 && strcmp(tokens[0], "fg") != 0 && strcmp(tokens[0], "bg") != 0){
				createJob();
			}
			status = shell();
		}
		if(status != 1){
			printf("something went wrong in shell\n");
		}
		reset();
	} while(status);

	close(savedSTDERR);
	close(savedSTDOUT);
	close(savedSTDIN);

	return 0;
}
