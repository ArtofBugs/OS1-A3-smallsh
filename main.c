#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <string.h>
#include <math.h>

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

    /*
    char* token = NULL;        // Keep track of current token (section of command
                               // between dollar signs)
    char* saveptr;             // Used by strtok_r to keep track of spot

    token = strtok_r(readCommand, "$", &saveptr);
    if (*saveptr == '\0') {
        return readCommand;
    }
    else if (saveptr[1] == '$') {
        expandedCommand = calloc(strlen(token) + pidDigits + 1, sizeof(char));
        sprintf(expandedCommand, "%s%d", token, pid);
        saveptr++;
    }
    else {
        expandedCommand = calloc(strlen(token) + 2, sizeof(char));
        sprintf(expandedCommand, "%s\$", token);
    }

    while (token = strtok_r(NULL, "$", &saveptr) && token) {
    
    }












    token = strtok_r(readCommand, "$", &saveptr);
    expandedCommand = calloc(strlen(token) + 1, sizeof(char));
    while (token) { // Returns false when there are no more tokens found
        realloc(expandedCommand, strlen(token) + 1, sizeof(char));
        strcat(expandedCommand, token);
        if (saveptr && saveptr[1] == '$') { // Another $ follows
            
        }
          
        sprintf(expandedCommand, "%s%s", expandedCommand, )
    }
    */

    // use strtok or substring to look for $$ and sprintf to rewrite?
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
    char* currWord = NULL;       // Current word in command entered
    char* saveptr;               // Pointer used by strtok_r() to save its spot
    char* expandedCommand = NULL;    // Expanded version of current command

    while (1) {
        prompt();
        getline(&readBuffer, &len, stdin);

        // Don't do anything if empty line (only '\n' entered) or line starts with '#'
        if (len != 1 && readBuffer[0] != '#') {
            expandedCommand = expandPids(readBuffer);
            printf("Expanded command: %s\n", expandedCommand); fflush(stdout);

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
                    printf("Arg was %s\n", arg);
                }
                if (error) {
                    printf("No such file or directory\n"); fflush(stdout);
                }
                // /*
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
                expandPids(readBuffer);
                // get args
                // expand args
                // determine redirection
                // determine foreground/background
                // fork
                // exec on expanded command
                // exit after the exec!!
                
            /*

            pid_t spawnpid = -5;
            
            spawnpid = fork();

            switch (spawnpid) {
                case -1:
                    perror("Unable to fork! ");
                    exitShell(1);
                    break;
                case 0:
                    // Child

                    exec();
                    break;
                default:
                    // Parent
                    break;
            }
            printf(""); fflush();

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
        */
            }
        }
        memset(readBuffer, '\0', strlen(readBuffer) - 1); // Clear the reading buffer
    }
    return 1;  // If it got down here, there was a problem...
}
