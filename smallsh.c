// include all the header files for commands used
#define  _GNU_SOURCE
#include <dirent.h>
// header file for input/output system calls
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
// header file for file/directory statistics
#include <sys/stat.h>
// header file for wait system calls for clearing child processes
#include <sys/wait.h> 
#include <time.h>
#include <unistd.h>
// header file for signal-related system calls & modifcations
#include <signal.h>
#include <fcntl.h>
#include <stdbool.h>
#include <sys/types.h>

// global vars for user input, argument list, input & output modes & files
char words_array[517][400];
char args_array[512][400];
int words_array_length = 0;
int args_array_length = 0;
int input_redirect = 0;
int output_redirect = 0;
char input_filepath[400];
char output_filepath[400];
// global vars for foreground mode, background command, status, array of background PIDs 
int foreground_mode = 0;
int background_command = 0;
char status[50];
int background_PID_array[9999];
int background_PID_array_length = 0;
// global vars to indicate location of prompt & whether control+z has toggled foreground mode
int foreground_enter_message = 0;
int foreground_exit_message = 0;
int at_prompt = 0;

/*
/ set default message of status
*/
void initializeGlobalVars() {

    memset(status, '\0', 50);
    strcpy(status, "exit value 0"); 
}

/*
/ convert user input separated by space into an array of individual words
*/
void inputToTokens(char *user_input) {
    
    int index = 0;
    // set " " and "\n" as delimiters
    char *current_word = strtok(user_input, " \n");
    memset(words_array, '\0', 517*400);
    words_array_length = 0;
    // iterate through each word returned by strtok
    while (current_word != NULL) {
        strcpy(words_array[index], current_word);
        index++;
        // generate next word
        current_word = strtok(NULL, " \n");
    }
    words_array_length = index; 
}

/*
/ expand all instances of "&&" in the array of words into the shell's PID
*/
void expandPID() {

    // retrieve shell's PID
    char pid_str[50];
    memset(pid_str, '\0', 50);
    sprintf(pid_str, "%d", getpid());

    int index = 0;

    // interate through array of words entered by user & rebuild entire word if any instances of "$$" detected
    while(index < words_array_length) {

        // create temp string variable to store current word to be rebuilt
        char current_word[500];
        memset(current_word, '\0', 500);
        int current_word_length = strlen(words_array[index]);
        int current_char = 0;

        // iterate through every character in the current word
        while(current_char < current_word_length) {
            // if the current & next words are "&&", replace them with the shell's PID & append to the rebuilt word
            if (current_char + 1 < current_word_length && strncmp("$$", words_array[index] + current_char, 2) == 0) {
                strcat(current_word, pid_str);
                current_char = current_char + 2;
            }
            // else, append current character as is to the rebuilt word
            else {
                strncat(current_word, words_array[index] + current_char, 1);
                current_char++;
            }
        }

        // replace the original word in the array with the rebuilt word
        memset(words_array[index], '\0', 400);
        strcpy(words_array[index], current_word);
        index++;
    }
}

/*
/ retrieve arguments in list of words & store them into a list of just arguments
*/
void getArgs() {

    int index = 0;
    memset(args_array, '\0', 512*400);
    args_array_length = 0;

    // copy words into list of arguments until a special symbol is encountered or 512 arguments have been copied
    while(true) {
        // if max number of arguments (512) have been copied
        if (args_array_length >= 512) {
            break;
        }
        // if "<" or ">" have been detected
        if (strcmp(words_array[index], "<") == 0 || strcmp(words_array[index], ">") == 0) {
            break;
        }
        // if this is the last word and it is "&"
        if (index + 1 == words_array_length && strcmp(words_array[index], "&") == 0) {
            break;
        }
        if (index >= words_array_length) {
            break;
        }
        // copy current word to array of arguments
        strcpy(args_array[index], words_array[index]);
        index++;
        args_array_length++;
    }
}

/*
/ parse the words after the special symbols & set modes accordingly
*/
void setModes() {

    // reset the mode global vars from the previous input
    int index = args_array_length;
    background_command = 0;
    input_redirect = 0;
    output_redirect = 0;
    memset(input_filepath, '\0', 400);
    memset(output_filepath, '\0', 400);

    // iterate through array of words starting at the 1st special symbol
    while(index < words_array_length) {
        // if current symbol is ">" and is followed by another word, store output redirection file and set output redirection
        if (strcmp(words_array[index], ">") == 0 && output_redirect == 0 && index + 1 < words_array_length) {
            strcpy(output_filepath, words_array[index+1]);
            output_redirect = 1;
            index++;
        }
        // if curent symbol is "<" and is followed by another word, store input redirection file and set input redirection
        else if (strcmp(words_array[index], "<") == 0 && input_redirect == 0 && index + 1 < words_array_length) {
            strcpy(input_filepath, words_array[index+1]);
            input_redirect = 1;
            index++;
        }
        // if current symbol is "&" and is the last word in the user input, set background command
        else if (strcmp(words_array[index], "&") == 0 && index == words_array_length - 1) {
            background_command = 1;
        }
        index++;
    }
}

/*
/ validates format of input if "exit" command detected
/ terminates all background commands through the SGKILL signal & terminates the shell
*/
int exitCommand() {

    // if exit is the only argument
    if (strcmp(args_array[1], "") == 0) {
        // iterate through array of background PIDs & terminate each PID
        for (int i = 0; i < background_PID_array_length; i++) {
            kill(background_PID_array[i], SIGKILL);
        }
        // return 1 to indicate OK for the main function to terminate the shell process
        return 1;
    }
    // if exit is not the only argument, indicate too many arguments
    else {
        printf("too many arguments\n");
        fflush(stdout);
        return 0;
    }
}

/*
/ validates format of input if "cd" command detected
*/
void cdCommand() {

    // if number of arguments <= 2
    if (args_array_length <= 2) {
        // if "cd" is the only arguemnt
        if (args_array_length == 1) {
            // cd to HOME directory
            if (chdir(getenv("HOME")) != 0) {
                perror("cd");
            }
        }
        // if followed by 2nd argument
        else {
            // cd to 2nd argument
            if (chdir(args_array[1]) != 0) {
                perror("cd");
            }
        }
    }
    // if numbe of arguments > 2, indicate too many arguments
    else {
        printf("too many arguments\n");
        fflush(stdout);
    }    
}

/*
/ validates format of input if "status" command detected
*/
void statusCommand() {

    // if no arguments follow, show status message
    if (strcmp(args_array[1], "") == 0) {
        printf("%s\n", status);
    }
    // if an argument follows, indicate too many arguments
    else {
        printf("too many arguments\n");
        fflush(stdout);
    }
}

/*
/ create NULL-terminated array of strings to be used with exec() with provided empty array
*/
void argsToExecArray(char *exec_array[]) {

    // iterate through array of arguments
    for (int i = 0; i < args_array_length; i++) {
        // copy & append current argument to array
        exec_array[i] = malloc(strlen(args_array[i] + 1));
        strcpy(exec_array[i], args_array[i]);
    }
    // assign last index to NULL
    exec_array[args_array_length] = NULL;
}

/*
/ set termination status of last terminated foreground process based on termination number provided
*/
void setStatus(int child_term_status) {

    // create temp var for rebuilt status string
    char status_string[50];
    memset(status_string, '\0', 50);

    // if provided termination number indicates that it terminated normally
    if (WIFEXITED(child_term_status) == 1) {
        strcpy(status_string, "exit value ");
        char status_number[10];
        memset(status_number, '\0', 10);
        // retrieve normal temrination number 
        sprintf(status_number, "%d", WEXITSTATUS(child_term_status));
        strcat(status_string, status_number);
        memset(status, '\0', 50);
        // copy complete normal termination message & number to temp var
        strcpy(status, status_string);
    }   
    // if provided terminaition number indicates that it was terminated by a signal
    else {
        strcpy(status_string, "terminated by signal ");
        char signal_number[10];
        memset(signal_number, '\0', 10);
        // retrieve signal termination number
        sprintf(signal_number, "%d", WTERMSIG(child_term_status));
        strcat(status_string, signal_number);
        memset(status, '\0', 50);
        // copy complete signal termination message & number to temp var
        strcpy(status, status_string);
        // display message right after termination
        printf("%s\n", status);
        fflush(stdout);
    }                            
}

/*
/ redirect input & output file descriptor to specified files if redirection specified or "/dev/null" if background command
*/
void fileRedirections() {

    // if input redirect was specified
    if (input_redirect == 1) {
        // open specifed input file
        int sourceFD = open(input_filepath, O_RDONLY);
        if (sourceFD == -1) { 
            printf("cannot open %s for input\n", input_filepath);
            fflush(stdout);
            exit(1); 
        };
        // redirect standard input file descriptor to specified file 
        int input_redirect_result = dup2(sourceFD, 0);
        if (input_redirect_result == -1) { 
            perror("source dup2()"); 
            exit(1); 
        }
    }
    // if background command specified
    else if (background_command && foreground_mode == 0) {
        // open "/dev/null" 
        int sourceFD = open("/dev/null", O_RDONLY);
        if (sourceFD == -1) { 
            printf("cannot open %s for input\n", "/dev/null");
            fflush(stdout);
            exit(1); 
        };   
        // redirect standard input file descriptor to "/dev/null"
        int input_redirect_result = dup2(sourceFD, 0);
        if (input_redirect_result == -1) { 
            perror("source dup2()"); 
            exit(1); 
        }
    }
    // if output redirect was specified
    if (output_redirect == 1) {
        // open specified output file
        int targetFD = open(output_filepath, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (targetFD == -1) { 
            printf("cannot open %s for output\n", output_filepath);
            fflush(stdout);
            exit(1); 
        }  
        // redirect standard output file descriptor to specified file 
        int output_red_result = dup2(targetFD, 1);
        if (output_red_result == -1) { 
            perror("target dup2()"); 
            exit(1); 
        }
    }
    // if background command specified
    else if (background_command && foreground_mode == 0) {
        // open "/dev/null"
        int targetFD = open("/dev/null", O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (targetFD == -1) { 
            printf("cannot open %s for output\n", "/dev/null");
            fflush(stdout);
            exit(1); 
        }  
        // redirect standard output file descriptor to "/dev/null"
        int output_red_result = dup2(targetFD, 1);
        if (output_red_result == -1) { 
            perror("target dup2()"); 
            exit(1); 
        }
    }
}

/* 
/ append provided process ID to array of background PIDs
*/
void addToBackgroundPIDs(int child_ID) {
    
    background_PID_array[background_PID_array_length] = child_ID;
    background_PID_array_length++;
}

/*
/ delete PID in the provided index from the array of background PIDs
*/
void deleteFromBackgroundPIDs(int index_to_delete) {

    int index = index_to_delete;
    // iterate through all PIDs starting @ index to be deleted
    while (index < background_PID_array_length - 1) {
        // replace the PID with the next one
        background_PID_array[index] = background_PID_array[index + 1];
        index++;
    }
    background_PID_array_length--;
}

/*
/ call wait() on each PID in array of background PIDs
*/
void waitBackgroundPIDs() {

    int index = 0;
    // iterate through array of background PIDs
    while (index < background_PID_array_length) {
        int background_child_status;
        // call waitpid without blocking on current PID
        int background_child_PID = waitpid(background_PID_array[index], &background_child_status, WNOHANG);
        // if child process of current PID was terminated
        if (background_child_PID != 0) {
            // if it terminated normally, display message with its termination number
            if (WIFEXITED(background_child_status) == 1) {
                printf("background pid %d is done: exit value %d\n", background_child_PID, WEXITSTATUS(background_child_status));
                fflush(stdout);
            }
            else {
            // if it was terminated by a signal, display message with its signal number
                printf("background pid %d is done: terminated by signal %d\n", background_child_PID, WTERMSIG(background_child_status));
                fflush(stdout);
            }
            // delete current PID from array of background PIDs
            deleteFromBackgroundPIDs(index);
        }
        // if child process of current PID hasn't terminated yet, iterate to the next PID
        else {
            index++;
        }
    }
}

/*
/ set current process to ignore the SIGINT signal
*/
void setIgnoreSIGINT() {
    struct sigaction ignore_SIGINT = {0};
    // set the handler function to the ignore macro
    ignore_SIGINT.sa_handler = SIG_IGN;
    sigaction(SIGINT, &ignore_SIGINT, NULL);
}

/*
/ set current process to terminate if sent the SIGINT signal
*/
void setDefaultSIGINT() {
    struct sigaction default_SIGINT = {0};
    // set the handler function to the default macro
    default_SIGINT.sa_handler = SIG_DFL;
    sigaction(SIGINT, &default_SIGINT, NULL);
}

/*
/ set current process to ignore the SIGTSTP signal
*/
void setIgnoreSIGTSTP() {
    struct sigaction ignore_SIGTSTP = {0};
    // set the handler function to the ignore macro
    ignore_SIGTSTP.sa_handler = SIG_IGN;
    sigaction(SIGTSTP, &ignore_SIGTSTP, NULL);
}

/*
/ set signal handler for current process to toggle foreground mode if sent SIGTSTP signal
*/
void handlerSIGTSTP(int signo){
    // if foreground mode is off
    if (foreground_mode == 0) {
        // turn it on
        foreground_mode = 1;
        // if shell is @ prompt, print enter message
        if (at_prompt == 1) {
        char* enter_message = "\nEntering foreground-only mode (& is now ignored)\n: ";
        write(STDOUT_FILENO, enter_message, 52);
        }
        // if shell not @ prompt, set reminder to display enter message when arrive @ prompt next time
        else {
            foreground_enter_message = 1;
            foreground_exit_message = 0; 
        }
    }
    // if foreground mode is on
    else {
        // turn it off
        foreground_mode = 0;
        // if shell is @ prompt, print exit message
        if (at_prompt == 1) {
        char* exit_message = "\nExiting foreground-only mode\n: ";
        write(STDOUT_FILENO, exit_message, 32);
        }
        // if shell not @ prompt, set reminder to display exit message when arriv @ prompt next time
        else {
            foreground_exit_message = 1;
            foreground_enter_message = 0; 
        }
    }
}

/*
/ display enter or exit foreground mode message if necessary
*/
void foregroundModeMessage() {
    // if reminder is set to display enter message
    if (foreground_enter_message) {
        // display enter message
        printf("Entering foreground-only mode (& is now ignored)\n");
        fflush(stdout);
        // turn off reminder
        foreground_enter_message = 0;
    }
    // if reminder is set to display exit message
    if (foreground_exit_message) {
        // display exit message
        printf("Exiting foreground-only mode\n");
        fflush(stdout);
        // turn off reminder
        foreground_exit_message = 0;
    }
}

/*
/ set current process to the handlerSIGTSTP handler function described previously
*/
void setCustomSIGTSTP() {
    struct sigaction custom_SIGTSTP = {0};
    custom_SIGTSTP.sa_handler = handlerSIGTSTP;
    // if a function call was aborted in the process of executing the handler function, restart the function call
    custom_SIGTSTP.sa_flags |= SA_RESTART;
    sigaction(SIGTSTP, &custom_SIGTSTP, NULL);
}

/*
/ main function containing the actual smallsh shell
*/
int main() {

    // assign initial values to global vars
    initializeGlobalVars();
    // ignore the SIGINT signal
    setIgnoreSIGINT();
    // set the SIGTSTP signal to toggle foreground mode
    setCustomSIGTSTP();

    // infinite loop representing the shell's interface
    while(true) {

        // create string var to hold user input
        char *user_input = malloc(2048);
        memset(user_input, '\0', 2048);
        size_t max_input_size = 2048;

        printf(": ");
        fflush(stdout);
        at_prompt = 1;
        // prompt user for input
        getline(&user_input, &max_input_size, stdin);
        at_prompt = 0;
        // convert input to words separated by space
        inputToTokens(user_input);
        // expand instances of "$$" to the shell process's PID
        expandPID();
        // store arguments to global array
        getArgs();
        // set file redirection statuses, foreground mode status, background command status
        setModes();
        free(user_input);

        // if 1st argument is "exit"
        if (strcmp(args_array[0], "exit") == 0) {
            // validate command & terminate all background processes 
            if (exitCommand()) {
                // terminate shell if argument valid
                return 0;
            }
        }
        // if 1st argument is "cd"
        else if (strcmp(args_array[0], "cd") == 0) {
            // validate command & cd to desired directory if argument(s) valid
            cdCommand();
        }
        // if 1st argument is "status"
        else if (strcmp(args_array[0], "status") == 0) {
            // validate command & display status if argument valid
            statusCommand();
        }
        // if nothing entered or input starts with "#", do nothing
        else if (words_array_length == 0 || strncmp(words_array[0], "#", 1) == 0) {
        }
        // if a non-built in command is entered
        else {
            // If fork is successful, child_ID == 0 in the child process, child_ID == child's PID in the shell
            pid_t child_ID = fork();
            switch (child_ID){
                // if fork failed, display error message & move on to next prompt
                case -1: {
                    perror("fork() failed!");
                    break;
                }
                // if child_ID == 0, the child process will run the following code
                case 0: {
                    // if foreground non-built in command entered
                    if (background_command == 0 || foreground_mode == 1) {
                        // set the behavior back to default if SIGINT signal received
                        setDefaultSIGINT();
                    }
                    // set all child processes to ignore the SIGTSTP signal
                    setIgnoreSIGTSTP();
                    // redirect input &/ output file descriptor(s) to corresponding files if necessary
                    fileRedirections();
                    char *exec_array[args_array_length];
                    // create NULL-terminated string array for execvp() call
                    argsToExecArray(exec_array);
                    // replace current child process with a new program to take care of the non-built in command
                    int exec_status = execvp(exec_array[0], exec_array);
                    perror(exec_array[0]);
                    exit(1);
                }
                // if current process is the shell's process
                default: {

                    // if a foreground non built-in process has been executed
                    if (foreground_mode == 1 || background_command == 0) {
                        int child_term_status;
                        // wait for the child process & clear it from the process table
                        waitpid(child_ID, &child_term_status, 0);
                        // set the status according to the termination type & number
                        setStatus(child_term_status);
                    }
                    // if a background non built-in process has been executed
                    else {
                        // display its PID
                        printf("background pid is %d\n", child_ID);
                        fflush(stdout);
                        // append its PID to the array of background PIDs
                        addToBackgroundPIDs(child_ID);
                    }
                    // display enter / exit foreground mode message if necessary
                    foregroundModeMessage();
                }    
            }
        }
        // clear any terminated background child processes & display message about each of them
        waitBackgroundPIDs();
    }
}