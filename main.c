#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

int status = 0; // Status value for the status command

void catchSIGINT(int signo) {

    char* message = "^C";
    write(STDOUT_FILENO, message, 3);
    /*raise(SIGUSR2);
    sleep(5);*/
    exit(0);
}

void printStatus() {
    printf("exit value %d\n", status);
}

// Clean up and exit with the given error status.
void exitShell() {
    exit(0);
}

void prompt() {
    printf(": "); fflush(stdout);
}

// Given a string, go through and replace instances of "$$" with the process ID.
char* expandPids(char* command) {
    int pid = getpid();
    char* pidString = calloc(7, sizeof(char)); // 6 is max length of a pid on os1
    int pidDigits = sprintf(pidString, "%d", pid); // number of digits in the pid

    char* readCommand = NULL; // Make a copy of the original command so it doesn't get destroyed
    readCommand = calloc(strlen(command) + 1, 1);
    strcpy(readCommand, command);
    char* expandedCommand = calloc(strlen(readCommand) + 1, 1); // Final expanded command to be returned
    char* currString = readCommand; // String left to parse
    char* pidLocation = strstr(readCommand, "$$");
    if (!pidLocation) {
        return command;
    }

    while (pidLocation && currString[0] != '\0') {
        pidLocation[0] = '\0'; // Terminate the current token for strcat'ing currString
        pidLocation[1] = '\0'; // Blank out the next $ too

        expandedCommand = realloc(expandedCommand, strlen(expandedCommand) + strlen(currString) + pidDigits + 1);
        
        strcat(expandedCommand, currString);
        strcat(expandedCommand, pidString);
        
        currString = pidLocation + 2;
        pidLocation = strstr(currString, "$$");
    }
    if (currString[0] != '\0') {
        expandedCommand = realloc(expandedCommand, strlen(expandedCommand) + strlen(currString) + 1);
        strcat(expandedCommand, currString);
    }
    
    return expandedCommand;
}

// Look at a command and find out if it needs redirection????? this will be
// after all the other args except for the arg for where to redirect to, ALSO running in the background is also at the very
// very very end!!!
int findRedirection() {
    return 0;
}

int main(int argc, char* argv[]) {
    char* readBuffer = NULL;     // Command entered by user
    size_t len = -1;             // Length of line entered by user
    char* expandedCommand = NULL;    // Expanded version of current command

    while (1) {
        prompt();
        getline(&readBuffer, &len, stdin);

        // Don't do anything if empty line (only '\n' entered) or line starts with '#'
        if (len != 1 && readBuffer[0] != '#') {
            expandedCommand = expandPids(readBuffer);
            // printf("Expanded command: %s\n", expandedCommand); fflush(stdout);

            if (strncmp(readBuffer, "exit", 4) == 0) {
                exitShell(0);
            }
            else if (strncmp(readBuffer, "cd", 2) == 0) {
                int error = 0; // non-zero if chdir was unsuccessful
                char* arg = readBuffer + 2; // pointer to spot after "cd", which
                                            // is ' ' if arguments
                                            // were provided and '\n' if not
                
                if (arg[0] == '\n') { // No arguments provided
                    // Go to home directory
                    error = chdir(getenv("HOME"));
                }
                else {
                    // An argument exists after the space, so give it to chdir()
                    // Replace the '\n' at the end with a '\0' first
                    strtok(++arg, "\n");
                    error = chdir(arg);
                    // printf("Arg was %s\n", arg); fflush(stdout);
                }
                if (error) {
                    printf("No such file or directory\n"); fflush(stdout);
                }
                /*
                // Uncomment for testing:
                char* buf = NULL;
                buf = getcwd(buf, 0);
                printf("Debug: Current working directory = %s\n", buf);
                    fflush(stdout);
                perror(NULL); fflush(stdout);
                free(buf); 
                // */
            }
            else if (strncmp(readBuffer, "status", 6) == 0) {
                printf("%d\n", status);
            }
            else {
                expandedCommand = expandPids(readBuffer);

                char** argPtrs = calloc(514, sizeof(char*));
                int currArg = 0;
                char* saveptr; // Pointer used by strtok_r() to save its spot
                char* args = strtok_r(expandedCommand, " \n", &saveptr);
                do {
                    argPtrs[currArg] = args;
                    // printf("currArg = %d; argPtrs[currArg] = %s\n", currArg, argPtrs[currArg]);
                    currArg++;
                    args = strtok_r(NULL, " \n", &saveptr);
                }
                while (args);
                argPtrs[currArg] = NULL;
                // determine redirection
                // determine foreground/background

                // I referenced lecture 3.1 and Exploration: Process API -
                // Monitoring Child Processes for this code

                int redirectIn = 0; // 1 if input should be redirected
                int redirectOut = 0; // 1 if output should be redirected
                int bg = 0; // 1 if command should be executed in background
                int fdIn = -1;
                int fdOut = -1;


                ///*

                pid_t childPid = -5;
                int childExitInfo = -5;


                childPid = fork();

                if (childPid == -1) {
                    perror("Unable to fork! ");
                    status = 1;
                }
                else if (childPid == 0) {
                    // I am the child universe
                    // printf("me child\n"); fflush(stdout);
                    // printf("PATH: %s\n", getenv("PATH"));
                    execvp(expandedCommand, argPtrs);
                    perror("execvp() failed: ");
                    exit(1);
                }

                // I am the parent universe
                childPid = waitpid(childPid, &childExitInfo, 0);
                if (WIFEXITED(childExitInfo)) {
                    // printf("Child %d exited normally with status %d\n", childPid, WEXITSTATUS(childExitInfo)); fflush(stdout);
                    status = WEXITSTATUS(childExitInfo);
                }
                // printf("parent waiting done\n");





                // */
                /*
                struct sigaction SIGINT_action = {0}, ignore_action = {0};

                SIGINT_action.sa_handler = catchSIGINT;
                sigfillset(&SIGINT_action.sa_mask);
                SIGINT_action.sa_flags = 0;

                ignore_action.sa_handler = SIG_IGN;

                sigaction(SIGINT, &SIGINT_action, NULL);
                sigaction(SIGTERM, &ignore_action, NULL);
                sigaction(SIGHUP, &ignore_action, NULL);
                sigaction(SIGQUIT, &ignore_action, NULL);

                printf("SIGTERM, SIGHUP, and SIGQUIT are disabled.\n"); fflush();
                // */
            }
        }
        memset(readBuffer, '\0', strlen(readBuffer) - 1); // Clear the reading buffer
    }
    return 1;  // If it got down here, there was a problem...
}
