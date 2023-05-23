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
                // - can hold exit status or termination signal number
int noBg = 0;   // 1 if nothing can be run in the background

// Handler for SIGTSTP signal, which only works on the parent and toggles
// foreground-only mode.
void catchSIGTSTP(int signo) {
    if (noBg) {
        noBg = 0;
        char* message = "\nExiting foreground-only mode\n";
        write(STDOUT_FILENO, message, 30);
        message = ": ";
        write(STDOUT_FILENO, message, 2);
    }
    else {
        noBg = 1;
        char* message = "\nEntering foreground-only mode (& is now ignored)\n";
        write(STDOUT_FILENO, message, 50);
        message = ": ";
        write(STDOUT_FILENO, message, 2);
    }
}

// Print the ":" prompt.
void prompt() {
    printf(": "); fflush(stdout);
}

// Given a string, go through and replace instances of "$$" with the process ID.
char* expandPids(char* command) {
    int pid = getpid();                        // Pid of current process
    char* pidString = calloc(7, sizeof(char)); // 6 is max length of a pid on os1
    int pidDigits = sprintf(pidString, "%d", pid); // number of digits in the pid

    char* readCommand = NULL; // Make a copy of the original command so it doesn't get destroyed
    readCommand = calloc(strlen(command) + 1, 1);
    strcpy(readCommand, command);
    char* expandedCommand = calloc(strlen(readCommand) + 1, 1); // Final expanded command to be returned
    char* currString = readCommand; // Pointer to current token to add to expanded command
    char* pidLocation = strstr(readCommand, "$$"); // Pointer to next spot to replace $$

    if (!pidLocation) {
        return command; // Exit if no $$'s found
    }

    // Iterate through the string to replace $$'s
    while (pidLocation && currString[0] != '\0') {
        pidLocation[0] = '\0'; // Terminate the current token for strcat'ing currString
        pidLocation[1] = '\0'; // Blank out the next $ too

        expandedCommand = realloc(expandedCommand, strlen(expandedCommand) + strlen(currString) + pidDigits + 1);

        // Add current token and the pid to the expanded command
        strcat(expandedCommand, currString);
        strcat(expandedCommand, pidString);

        currString = pidLocation + 2;
        pidLocation = strstr(currString, "$$");
    }
    // Put the end of the command (after the last $$) into the expanded command
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


    // I referenced Lecture 3.3: Signals for this
    // Set up signal handlers for parent process
    struct sigaction SIGTSTP_action = {{0}}, ignore_action = {{0}};

    SIGTSTP_action.sa_handler = catchSIGTSTP;
    sigfillset(&SIGTSTP_action.sa_mask);
    SIGTSTP_action.sa_flags = SA_RESTART;

    ignore_action.sa_handler = SIG_IGN;

    sigaction(SIGINT, &ignore_action, NULL);   // Ignore SIGINT for parent
    sigaction(SIGTSTP, &SIGTSTP_action, NULL); // Register SIGTSTP to function

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

            if (strncmp(readBuffer, "exit", 4) == 0) {
                // Built-in exit command ======================================

                for (int i = 0; i <= maxBgProc; i++) {
                    // If the child process has not terminated, it will be force killed;
                    // otherwise, it will be cleaned up.
                    if (waitpid(bgProcs[i], &bgProcInfos[i], WNOHANG) == 0) {
                        kill(bgProcs[i], SIGKILL); // Force kill!
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
                exit(0); // Exit smallsh!
            }
            else if (strncmp(expandedCommand, "cd", 2) == 0) {
                // Built-in cd command ========================================

                int error = 0;              // non-zero if chdir was unsuccessful
                char* arg = expandedCommand + 2; // pointer to spot after "cd",
                                            // which is ' ' if arguments
                                            // were provided and '\n' if not

                if (arg[0] == '\n') {
                    // No arguments provided, so go to home directory
                    error = chdir(getenv("HOME"));
                }
                else {
                    // An argument exists after the space, so give it to chdir()
                    // Replace the '\n' at the end with a '\0' first
                    strtok(++arg, "\n");
                    error = chdir(arg);
                }
                if (error) {
                    perror("cd"); fflush(stdout);
                }
            }
            else if (strncmp(readBuffer, "status", 6) == 0) {
                // Built-in status command ====================================

                if (status == 0 || status == 1) {
                    printf("exit value %d\n", status);
                }
                else {
                    printf("terminated by signal %d\n", status);
                }
            }
            else {
                // External commands ==========================================

                int bg = 0;    // 1 if command should be executed in background
                int fdIn = 0;  // File descriptor to direct input to
                int fdOut = 1; // File descriptor to direct output to


                // Put arguments into an array for exec() =====================

                // Array of pointers to arguments
                char** argPtrs = calloc(514, sizeof(char*));

                int currArg = 0; // Index of current slot to store an argument
                char* saveptr;   // Pointer used by strtok_r() to save its spot

                // Current argument (token) being investigated
                char* args = strtok_r(expandedCommand, " \n", &saveptr);

                // Iterate through command and add arguments to argPtrs
                do {
                    argPtrs[currArg] = args;
                    currArg++;
                    args = strtok_r(NULL, " \n", &saveptr);
                }
                while (args);

                // Terminate args array with a NULL pointer
                argPtrs[currArg] = NULL;


                // Determine whether backgrounding needs to happen ============

                if (strcmp(argPtrs[currArg - 1], "&") == 0) {
                    if (!noBg) { // Do nothing if foreground-only mode is on
                        bg = 1;
                    }
                    // Terminate args array before the & for exec
                    argPtrs[currArg - 1] = NULL;
                    currArg--;
                }

                // Create child ===============================================

                // I referenced lecture 3.1 and Exploration: Process API -
                // Monitoring Child Processes for this code

                pid_t childPid = -5;
                int childExitInfo = -5;

                childPid = fork();

                // Determine whether we are in the parent or child
                if (childPid == -1) {
                    perror("Unable to fork"); fflush(stdout);
                    status = 1;
                }
                else if (childPid == 0) {
                    // In the child ============================================

                    
                    // Determine whether input/output need to be redirected ====

                    // Only look at the last four arguments (not including & if
                    // it was there), because those are the only places possible
                    // to find < or >. Skip the very last argument, which would
                    // be a file name if redirection is to happen. Also stop if
                    // 0 is reached (that's the position of the command itself).
                    for (int i = currArg - 2; i > 0 && i >= currArg - 4; i--) {
                        // Look for < and open the file specified
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
                        // Look for > and open the file specified
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

                    // Do any redirects necessary
                    dup2(fdIn, 0);
                    dup2(fdOut, 1);

                    // Create signal handler for SIGINT
                    struct sigaction SIGINT_action = {{0}};
                    // Set to default action
                    SIGINT_action.sa_handler = SIG_DFL;

                    sigfillset(&SIGINT_action.sa_mask);
                    SIGINT_action.sa_flags = SA_RESTART;

                    // Set signal handler for SIGINT for child if it's going to
                    // be executed in the foreground
                    if (!bg) {
                        sigaction(SIGINT, &SIGINT_action, NULL);
                    }
                    // Otherwise it will stay ignored

                    // Set child to ignore SIGTSTP
                    sigaction(SIGTSTP, &ignore_action, NULL);

                    // Execute external command
                    execvp(expandedCommand, argPtrs);

                    // Error out if it gets down here
                    perror("bash"); fflush(stdout);
                    exit(1);
                }


                // In the parent ===============================================

                if (bg == 0) {
                    // Do blocking wait for child if it's not a background
                    // process
                    childPid = waitpid(childPid, &childExitInfo, 0);

                    // Set the exit/termination status
                    if (WIFEXITED(childExitInfo)) {
                        status = WEXITSTATUS(childExitInfo);
                    }
                    else {
                        status = WTERMSIG(childExitInfo);
                    }
                }
                else {
                    maxBgProc++;
                    // Do non-blocking wait for child
                    waitpid(childPid, &bgProcInfos[maxBgProc], WNOHANG);
                    printf("background pid is %d\n", childPid); fflush(stdout);
                    // Add child to background processes list
                    bgProcs[maxBgProc] = childPid;
                }
            }
        }
        free(readBuffer);
        readBuffer = NULL;
        len = -1;
    }
    perror(NULL);
    return 1;  // If it got down here, there was a problem...
}
