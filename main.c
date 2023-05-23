#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

int status = 0; // Status value for the status command

void catchSIGINT(int signo) {

    char* message = "^C";
    write(STDOUT_FILENO, message, 2);
    /*raise(SIGUSR2);
    sleep(5);*/
    exit(0);
}

// Clean up and exit with the given error status.
void exitShell() {

    exit(0);
}

// Print the ":" prompt.
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
    free(readCommand);
    return expandedCommand;
}

int main(int argc, char* argv[]) {
    char* readBuffer = NULL;         // Command entered by user
    size_t len = -1;                 // Length of line entered by user
    char* expandedCommand = NULL;    // Expanded version of current command

    pid_t bgProcs[257];   // Array of background processes still running
    int bgProcInfos[257]; // Array of exit information integers for the
                          // background processes
    int maxBgProc = -1;   // Index of last background process

    while (1) {
        // Check for finished background processes to reap ====================

        for (int i = 0; i <= maxBgProc; i++) {
            // If the child process has not terminated, waitpid() will return 0;
            // otherwise, it will clean up that process.
            if (waitpid(bgProcs[i], &bgProcInfos[i], WNOHANG) == 0) {
                continue;
            }
            // Print exit/termination status
            if (WIFEXITED(bgProcInfos[i])) {
                printf("background pid %d is done: ", bgProcs[i]);
                printf("exit value %d\n", WEXITSTATUS(bgProcInfos[i])); fflush(stdout);
            }
            else {
                printf("background pid %d is done: ", bgProcs[i]);
                printf("terminated by signal %d\n", WTERMSIG(bgProcInfos[i])); fflush(stdout);
            }
            // Remove this entry from the record of background processes still
            // running by replacing it with the very top array item
            bgProcs[i] = bgProcs[maxBgProc];
            bgProcInfos[i] = bgProcInfos[maxBgProc];
            maxBgProc--;
        }


        // Prompt and interpret input =========================================

        prompt();
        getline(&readBuffer, &len, stdin);

        // Don't do anything if empty line (only '\n' entered) or line starts with '#'
        if (strlen(readBuffer) != 1 && readBuffer[0] != '#') {
            expandedCommand = expandPids(readBuffer);
            // printf("Expanded command: %s\n", expandedCommand); fflush(stdout);

            if (strncmp(readBuffer, "exit", 4) == 0) {
                exitShell(0);
            }
            else if (strncmp(expandedCommand, "cd", 2) == 0) {
                int error = 0; // non-zero if chdir was unsuccessful
                char* arg = expandedCommand + 2; // pointer to spot after "cd", which
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
                    perror("cd"); fflush(stdout);
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
                if (status == 0 || status == 1) {
                    printf("exit value %d\n", status);
                }
                else {
                    printf("terminated by signal %d\n", status);
                }
            }
            else {
                int bg = 0; // 1 if command should be executed in background
                int fdIn = 0; // File descriptor to direct input to
                int fdOut = 1; // File descriptor to direct output to


                // Put arguments into an array for exec() =====================

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


                // Determine whether backgrounding needs to happen ============

                if (strcmp(argPtrs[currArg - 1], "&") == 0) {
                    bg = 1;
                    argPtrs[currArg - 1] = NULL; // Terminate args here for exec()
                    currArg--;
                }

                // Create child ===============================================

                // I referenced lecture 3.1 and Exploration: Process API -
                // Monitoring Child Processes for this code

                pid_t childPid = -5;
                int childExitInfo = -5;


                childPid = fork();

                if (childPid == -1) {
                    perror("Unable to fork"); fflush(stdout);
                    status = 1;
                }
                else if (childPid == 0) {
                    // Child

                    
                    // Determine whether input/output need to be redirected ===
                    
                    // Only look at the last four arguments (not including & if
                    // it was there), because those are the only places possible
                    // to find < or >. Skip the very last argument, which would
                    // be a file name if redirection is to happen. Also stop if
                    // 0 is reached (that's the position of the command itself).
                    for (int i = currArg - 2; i > 0 && i >= currArg - 4; i--) {
                        if (strcmp(argPtrs[i], "<") == 0) {
                            fdIn = open(argPtrs[i+1], O_RDONLY);
                            if (fdIn == -1) {
                                perror("bash"); fflush(stdout);
                                exit(1); // Exit child and report status
                            }
                            // From Exploration: Processes and I/O
                            fcntl(fdIn, F_SETFD, FD_CLOEXEC);
                            // Now whenever this process or one of its child
                            // processes calls exec, the file descriptor fd will
                            // be closed in that process
                            argPtrs[i] = NULL; // truncate arguments list
                        }
                        else if (strcmp(argPtrs[i], ">") == 0) {
                            fdOut = open(argPtrs[i+1],
                                    O_WRONLY | O_CREAT | O_TRUNC, 0640);
                            if (fdOut == -1) {
                                perror("bash"); fflush(stdout);
                                exit(1); // Exit child and report status
                            }
                            fcntl(fdOut, F_SETFD, FD_CLOEXEC);
                            argPtrs[i] = NULL; // truncate arguments list
                        }
                    }

                    // Redirect input and output to /dev/null if running in
                    // background without redirection to file
                    if (bg == 1) {
                        if (fdIn == 0) {
                            fdIn = open("/dev/null", O_RDONLY);
                            if (fdIn == -1) {
                                perror("bash"); fflush(stdout);
                                exit(1);
                            }
                            fcntl(fdIn, F_SETFD, FD_CLOEXEC);
                        }
                        if (fdOut == 1) {
                            fdOut = open("/dev/null", O_WRONLY);
                            if (fdOut == -1) {
                                perror("bash"); fflush(stdout);
                                exit(1);
                            }
                            fcntl(fdOut, F_SETFD, FD_CLOEXEC);
                        }
                    }
                    dup2(fdIn, 0);
                    dup2(fdOut, 1);
                    execvp(expandedCommand, argPtrs);
                    // Error out if it gets down here
                    perror("bash"); fflush(stdout);
                    exit(1);
                }

                // Parent
                if (bg == 0) {
                    childPid = waitpid(childPid, &childExitInfo, 0);
                    if (WIFEXITED(childExitInfo)) {
                        status = WEXITSTATUS(childExitInfo);
                    }
                    else {
                        status = WTERMSIG(childExitInfo);
                    }
                }
                else {
                    maxBgProc++;
                    waitpid(childPid, &bgProcInfos[maxBgProc], WNOHANG);
                    printf("background pid is %d\n", childPid); fflush(stdout);
                    bgProcs[maxBgProc] = childPid;
                }





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
        free(readBuffer);
        readBuffer = NULL;
        len = -1;
    }
    return 1;  // If it got down here, there was a problem...
}
