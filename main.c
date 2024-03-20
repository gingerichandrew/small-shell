#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <signal.h>
#include <stdbool.h>

/*
*	Struct argument that will store a single argument as a linked list
*	meaning the head of the list will be the command, and the following nodes
*	will be commands arguments
*/
struct argument {
	char* arg;		// Undefined size
	int nArgs;		// Only head of the list will ever have a value of nArgs
	struct argument* next;	// Next argument
};

/*
*	Struct process that will store a current process that is being ran
*	in the background to check and see if it's completed yet
*/
struct process {
	pid_t processId;
	struct process* next;
};

/*
*	Takes a string, with this use case being a user entered
*	string on the command line, and parses it so it can later
*	be used as intended.
*/
struct argument* getArgs(char* line) {
	struct argument* tail = NULL;
	int i = 0;
	// For use with strtok_r
	char* saveptr;
	char* token = strtok_r(line, " ", &saveptr);
	// Create and fill head(command)
	struct argument* command = malloc(sizeof(struct argument));
	command->arg = (char*)calloc(strlen(token) + 1, sizeof(char));
	strcpy(command->arg, token);
	// Set tail to head(command)
	tail = command;
	while (token != NULL) {
		token = strtok_r(NULL, " ", &saveptr);
		// Is there a valid token?
		if (token != NULL) {
			// Create new argument node
			struct argument* newArg = malloc(sizeof(struct argument));
			newArg->arg = (char*)calloc(strlen(token) + 1, sizeof(char));
			strcpy(newArg->arg, token);
			// Set tail to new argument node
			tail->next = newArg;
			// Add this node to the list and advance the tail
			tail = newArg;
			// Add to counter for total number of args
			i++;
		}
	}
	tail->next = NULL;
	command->nArgs = i+1;
	return command;
}

/*
*	Sets env("bgMode") equal to 1 or 0, 1 meaning foreground-only, 0 bg allowed
*	when a ctrl+z, or SIGSTP is caught
*/
void catchSIGTSTP(int signo) {
	if (strcmp(getenv("bgMode"), "0") == 0) {
		setenv("bgMode", "1", 1);
		char* message = "\nEntering foreground-only mode (& is now ignored)\n:";
		write(STDOUT_FILENO, message, 52);
	}
	else {
		setenv("bgMode", "0", 1);
		char* message = "\nExiting foreground - only mode\n:";
		write(STDOUT_FILENO, message, 34);
	}

}

/*
*	Whenever a SIGUSER2 is caught will set exit status of
*	previous foreground process
*/
void catchSIGUSR2(int signo){
	char* message = "terminated by signal 2\n";
	setenv("eStatus", message, 1);
}

/*
*	Whenever a SIGINT is caught, write message to terminal and then
*	wait to prune zombie, then raise a SIGUSER2 to set exit status
*/
void catchSIGINT(int signo){
	char* message = "terminated by signal 2\n";
	write(STDOUT_FILENO, message, 24);
	wait(NULL);
	raise(SIGUSR2);
}

/*
*	Built in function cd, takes a single argument(ignores if more are
*	given by user) and then sets the directory if possible(exists)
*/
void cd__(struct argument* args) {
	// If there is one argument (cd <directory>)
	if (args->next != NULL) {
		if (chdir(args->next->arg) != 0) { 
			perror(""); 
		}
	}
	// Else cd was called with no args
	else {
		chdir(getenv("HOME"));
	}
}

/*
*	Built in function status, looks at env "eStatus" to find
*	most recent exit status of last foreground process
*/
void status__(int exitStatus) {
	printf("%s", getenv("eStatus"));
	fflush(stdout);
}

/*
*	Built in function exit, takes the linked list of process's running in the background
*	and terminates them before then calling exit(EXIT_SUCCESS)
*/
void exit__(struct process* process) {
	while (process != NULL) {
		kill(process->processId, SIGKILL);
		process = process->next;
	}
	exit(EXIT_SUCCESS);
}

/*
*	Takes a char sPID(from getpid()), and a char* arg(argument from command line)
*	and replaces anywhere with "$$" to "PID", ex "echo $$" -> "echo 4021" 
*/
char* swapDelimiter(char* sPID, char* arg) {
	// Temp string to store text of arg
	char text[2048];
	strcpy(text, arg);
	while (1) {
		// Set delimiter
		char delim[] = "$$";
		size_t dlen = sizeof(delim) / sizeof(delim[0]) - 1;
		char* p;
		// Get position of delimiter in string, or exit if doesnt exist
		p = strstr(text, delim);
		if (p == NULL) {
			break;
		}
		// Split into two halfs, then cat PID for variable expansion
		char strFirst[2048];
		char strSecond[2048];
		strncpy(strFirst, text, (strlen(text) - strlen(p)));
		strFirst[strlen(text) - strlen(p)] = '\0';
		strcpy(strSecond, p + dlen);

		strcat(strFirst, sPID);
		strcat(strFirst, strSecond);

		strcpy(text, strFirst);

	}
	char* newArg = (char*)calloc(strlen(text) + 1, sizeof(char));
	strcpy(newArg, text);
	// return the new line with $$ removed and PID put in its place
	return newArg;
}

int main(){
	// Head and tail of process linked list
	struct process* processArrayHead = NULL;
	struct process* processArrayTail = NULL;
	// Setup signal handler
	struct sigaction SIGINT_action = { 0 }, SIGUSR2_action = { 0 }, SIGTSTP_action = { 0 }, ignore_action = { 0 };

	SIGINT_action.sa_handler = catchSIGINT;
	sigfillset(&SIGINT_action.sa_mask);
	SIGINT_action.sa_flags = SA_RESTART;

	SIGUSR2_action.sa_handler = catchSIGUSR2;
	sigfillset(&SIGUSR2_action.sa_mask);
	SIGUSR2_action.sa_flags = SA_RESTART;

	SIGTSTP_action.sa_handler = catchSIGTSTP;
	sigfillset(&SIGTSTP_action.sa_mask);
	SIGTSTP_action.sa_flags = SA_RESTART;

	ignore_action.sa_handler = SIG_IGN;

	sigaction(SIGUSR2, &SIGUSR2_action, NULL);
	sigaction(SIGTSTP, &SIGTSTP_action, NULL);
	sigaction(SIGINT, &ignore_action, NULL);
	// Set default exit status, and set bgMode to allowed
	int childExitStatus = 0;
	setenv("eStatus", "exit status 0\n", 1);
	setenv("bgMode", "0", 1);
	/*
	*	Begin forever while loop now that setup is done
	*	exit once the exit command is entered
	*/
	while (1) {
		//---- Before any command prompt control, check if any background proccess have finished ----//
		struct process* currProcess = processArrayHead;
		while (currProcess != NULL) {
			if (waitpid(currProcess->processId, &childExitStatus, WNOHANG) > 0) {
				printf("background pid %d is done : ", currProcess->processId);
				// Exited normally
				if (WIFEXITED(childExitStatus)){
					printf("exit value %d\n", WEXITSTATUS(childExitStatus));
				}
				// Exited due to signal
				else if (WIFSIGNALED(childExitStatus)){
					printf("terminated by signal %d\n", WTERMSIG(childExitStatus));
				}
				fflush(stdout);
			}
			currProcess = currProcess->next;
		}

		//---- Command Prompt ----//
		char line[2048];
		int i;
		// Command is head of linked list containing argument line
		struct argument* command = NULL;
		// Get user input from shell
		if (printf(": ") == 2 && fgets(line, sizeof(line), stdin)) {
			// Input is safe to be used, convert into linked list
			if (1 == sscanf(line, "%s", &i)) {
				// Remove newline character from end
				int strlength = strlen(line);
				if (strlength > 1) {
					line[strlength - 1] = '\0';
				}
				// get head(command) of argument linked list
				command = getArgs(line);
			}else{ continue; } // Blankline thus ignored
		}
		//---- If line starts with #, ignore ----//
		if (strncmp(command->arg, "#",1) == 0) { continue; }

		//---- Look for any $$ in line, and change to PID of smallsh ----//
		struct argument* argvLL = command;
		char sPID[2048];
		sprintf(sPID, "%d", getpid());
		while (argvLL != NULL) {
			argvLL->arg = swapDelimiter(sPID,argvLL->arg);
			argvLL = argvLL->next;
		}

		//---- Check if built in command, run built in command and return to top of loop if true----//
		if (strcmp(command->arg, "cd") == 0) { cd__(command); continue; }
		if (strcmp(command->arg, "status") == 0) { status__(childExitStatus); continue; }
		if (strcmp(command->arg, "exit") == 0) { exit__(processArrayHead); continue; }

		//---- Check if process should be ran in the background ----//
		bool bgProcess = false;

		argvLL = command;
		struct argument* prev = command;
		while (argvLL != NULL) {
			if (strcmp(argvLL->arg, "&") == 0 && argvLL->next == NULL) {
				// If "&" character is found and is at end of list
				prev->next = NULL;
				command->nArgs = command->nArgs - 1;
				// then remove it from argument list and set bg equal to true
				bgProcess = true;
				break;
			}
			argvLL = argvLL->next;
			prev = argvLL;
		}
		//---- If process should be background, add it to linked list ----//
		pid_t spawnPid = -5;
		if (bgProcess && strcmp(getenv("bgMode"), "0") == 0) {
			struct process* newProcess = malloc(sizeof(struct argument));
			if (processArrayHead == NULL) {
				processArrayHead = newProcess;
				processArrayTail = newProcess;
			}
			else {
				processArrayTail->next = newProcess;
				processArrayTail = newProcess;
			}
			fflush(stdout);
			fflush(stdin);
			//---- Background process fork ----//
			spawnPid = fork();
			sigaction(SIGTSTP, &ignore_action, NULL);
			newProcess->processId = spawnPid;
			newProcess->next = NULL;
		}else{
			//---- Foreground process fork ----//
			spawnPid = fork();
			sigaction(SIGTSTP, &ignore_action, NULL);
			sigaction(SIGINT, &SIGINT_action, NULL);
		}
		switch (spawnPid) {
			case -1: { 
				perror("Hull Breach!\n"); exit(1); break; 
			}
			case 0: {
				//---- Set stdin and stdout, and remove "<" ">" ----//
				if (bgProcess) {
					int targetFD = open("/dev/null", O_WRONLY | O_CREAT | O_TRUNC, 0644);
					int sourceFD = open("/dev/null", O_RDONLY);
					int result = dup2(targetFD, 1);
					result = dup2(sourceFD, 0);
				}

				struct argument* argvLL = command;
				struct argument* prev = NULL;
				while (argvLL != NULL) {
					if (strcmp(argvLL->arg, ">") == 0) {
						int targetFD = open(argvLL->next->arg, O_WRONLY | O_CREAT | O_TRUNC, 0644); 
						if (targetFD == -1) {
							printf("cannot open %s for output\n", argvLL->next->arg);
							fflush(stdout);
							exit(1); 
						}
						fflush(stdout);
						fflush(stdin);
						int result = dup2(targetFD, 1);

						prev->next = NULL;
						command->nArgs = command->nArgs - 2;
					}
					if (strcmp(argvLL->arg, "<") == 0) {
						int sourceFD = open(argvLL->next->arg, O_RDONLY); 
						if (sourceFD == -1) {
							printf("cannot open %s for input\n", argvLL->next->arg);
							fflush(stdout);
							exit(1);
						}
						fflush(stdout);
						fflush(stdin);
						int result = dup2(sourceFD, 0);
						
						prev->next = NULL;
						command->nArgs = command->nArgs - 2;
					}
					prev = argvLL;
					argvLL = argvLL->next;
				}
				//---- Convert Linked List into Array ----//
				char* argv[(command->nArgs) + 1];
				argvLL = command;
				for (i = 0; i < command->nArgs; i++) {
					argv[i] = argvLL->arg;
					argvLL = argvLL->next;
				}
				// set last element to NULL
				argv[(command->nArgs)] = NULL;

				//---- exec into new process ----//
				fflush(stdout);
				execvp(argv[0], argv); 

				//---- Command not found/execvp failed ----//
				printf("%s:", argv[0]);
				fflush(stdout);
				perror("");
				exit(1);  
			}
			default: {
				//---- If its not a background task, or bgMode isnt set to 0 ----//
				if (!bgProcess || strcmp(getenv("bgMode"), "1") == 0) {
					sigaction(SIGTSTP, &SIGTSTP_action, NULL);
					// wait till foreground process has finished
					pid_t actualPid = waitpid(spawnPid, &childExitStatus, 0);
					if (actualPid != -1) {
						int exitStatusC = WEXITSTATUS(childExitStatus);
						char intToStr[100];
						char message[100] = "exit value ";
						sprintf(intToStr, "%d", exitStatusC);
						strcat(message, intToStr);
						strcat(message, "\n");
						setenv("eStatus", message, 1);
					}
				}
				else {
					// dont wait for child, but print it's PID
					sigaction(SIGTSTP, &SIGTSTP_action, NULL);
					printf("background pid is %d\n", spawnPid);
					fflush(stdout);
				}
				// reset handler
				ignore_action.sa_handler = SIG_IGN;
				sigaction(SIGINT, &ignore_action, NULL);
			}
		}
	}
}
