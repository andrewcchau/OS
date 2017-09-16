/*
Name: Andrew Chau
Lab: 1
Last Edit: 8:01 PM 9/12/17
*/

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>
#include <fcntl.h>

#define buffLength 2001

int pipefd[2];
int status, pid_ch1, pid_ch2, pid;
char buff[buffLength];
char* tokens[buffLength / 3];

static void sig_int(int signo) {
//  printf("Sending signals to group:%d\n",pid_ch1); // group id is pid of first in pipeline
  kill(-pid_ch1,SIGINT);
}

static void sig_tstp(int signo) {
//  printf("Sending SIGTSTP to group:%d\n",pid_ch1); // group id is pid of first in pipeline
  kill(-pid_ch1,SIGTSTP);
}


void getTokens(){
	if(feof(stdin)){
		exit(0);
		printf("eof fail");				//should never print
	}
	printf("# ");
	fgets(buff, buffLength, stdin);

	//get rid of &
	char* temp = strrchr(buff, '&');    
	if(temp != NULL){
		*temp = 0;
	}

	//parse through tokens and store them
	char* token = strtok(buff, " ");
	int ctr = 0;
	while(token != NULL){
		/*if(strcmp(token, "&\n") == 0){
			break;
		}*/
		tokens[ctr] = token;
		token = strtok(NULL, " ");
		ctr++;
	}
	tokens[ctr] = NULL;
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
void fileRedir(){
	int curr = 0; //current token
	int changed = 0;
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
}

void copyTokens(char** dest, int start, int stop){
	int ctr = 0;
	while(tokens[start] != NULL && start <= stop){
		dest[ctr] = tokens[start];
		start++;
		ctr++;
	}
	dest[ctr] = NULL;
}

int shell(){
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
			tokens[counter] = NULL;
		}else if(strcmp(tokens[counter], "|") == 0){
			pipeLoc = counter;
		}
		counter++;
	}

	pid_ch1 = fork();
	if(pid_ch1 > 0){
		//parent
		//printf("Child1 pid = %d\n", pid_ch1);
		if(pipeLoc != 0){
			pid_ch2 = fork();
			if(pid_ch2 > 0){
				//SIGNALS HERE (I think)
				if (signal(SIGINT, sig_int) == SIG_ERR)
					printf("signal(SIGINT) error");
				if (signal(SIGTSTP, sig_tstp) == SIG_ERR)
					printf("signal(SIGTSTP) error");

				//close parent pipe
				close(pipefd[0]);
				close(pipefd[1]);
				int count = 0;
				while(count < 2 && !background){
					pid = waitpid(-1, &status, WUNTRACED | WCONTINUED);
					
					if(pid == -1){
						perror("waitpid");
						exit(EXIT_FAILURE);
					}

					//status messages (won't need when turn in)
					if (WIFEXITED(status)) {
					//	printf("child %d exited (2), status=%d\n", pid, WEXITSTATUS(status));
						count++;
					} else if (WIFSIGNALED(status)) {
						//printf("child %d killed by signal %d\n", pid, WTERMSIG(status));
						count++;
					} else if (WIFSTOPPED(status)) {
						printf("%d stopped by signal %d\n", pid,WSTOPSIG(status));
						sleep(2); //sleep for 2 seconds before sending CONT
						kill(pid,SIGCONT);
					} else if (WIFCONTINUED(status)) {
						printf("Continuing %d\n",pid);
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
				char* args[buffLength / 10];
				copyTokens(args, pipeLoc + 1, pipeLoc + 100);

				int ret = execvp(args[0], args);
				if(ret != 0){
					exit(0);	//exit to prevent grandchildren if token isn't valid command
				}
			}
		}else{
			//parent
			int count = 0;
			/*if(background){
				waitpid(-1, &status, 0);
			}*/

			while(count < 1 && !background) {
				pid = waitpid(-1, &status, WUNTRACED | WCONTINUED);

				if (WIFEXITED(status)) {
					//printf("child %d exited (1), status=%d\n", pid, WEXITSTATUS(status));
					count++;
				} else if (WIFSIGNALED(status)) {
					//printf("child %d killed by signal %d\n", pid, WTERMSIG(status));
					count++;
				} else if (WIFSTOPPED(status)) {
					//printf("%d stopped by signal %d\n", pid,WSTOPSIG(status));
					//sleep(4); //sleep for 4 seconds before sending CONT
					//kill(pid,SIGCONT);
				} else if (WIFCONTINUED(status)) {
					//printf("Continuing %d\n",pid);
				}
			} 



			if(pid == -1){
				perror("waitpid");
				exit(EXIT_FAILURE);
			}

		}
	}else{
		//child 1
		if(feof(stdin)){
			exit(0);
		}
		if(background){
			setpgid(0,0);
		}else{
			setsid(); //child 1 creates new session & new group and becomes leader
		}
		char* args[buffLength / 10];
		if(pipeLoc != 0){
			close(pipefd[0]); //closes read end
			dup2(pipefd[1], STDOUT_FILENO);
			copyTokens(args, 0, pipeLoc - 1);
		}else{
			copyTokens(args, 0, buffLength / 10);
		}

		int ret = execvp(args[0], args);
		tokens[0] = NULL;
		if(ret != 0){
			exit(0);	//exit to prevent grandchildren if token isn't valid command
		}
	}

	return 1;
}

int main(void){
	int status;

	do{
		getTokens();
		//displayTokens();
		fileRedir();			//file redirect first
		status = shell();
		if(status != 1){
			printf("something went wrong\n");
		}
	} while(status);
}