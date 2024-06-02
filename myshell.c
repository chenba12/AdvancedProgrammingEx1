#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <errno.h>
#include "myshell.h"
#include <signal.h>
#include <ctype.h>
#include <termios.h>

#define MAX_ARGS 1024
#define BUFFER_SIZE 1024
#define INITIAL_VAR_CAPACITY 10
#define MAX_HISTORY_SIZE 100

char history[MAX_HISTORY_SIZE][BUFFER_SIZE];
int history_index = 0;
int current_history_pos = 0;

char prompt[BUFFER_SIZE] = "hello:";


void executeIfCommand(char **args, char *outfile, char *infile, int append, int stderr_redirect, int background,
                      int *last_exit_status) {
    int contains_pipe = 0;
    for (int i = 0; args[i] != NULL; ++i) {
        if (strchr(args[i], '|') != NULL) {
            contains_pipe = 1;
            break;
        }
    }
    if (contains_pipe) {
        executePipeline(args[1], last_exit_status, 1);
    } else {
        executeExternalCommand(&args[1], outfile, infile, append, stderr_redirect, background, last_exit_status, 1);
    }

}

int main() {
    int last_exit_status = 0;
    char command[BUFFER_SIZE];
    char *args[MAX_ARGS] = {NULL};
    char *outfile = NULL;
    char *infile = NULL;
    int append = 0;
    int stderr_redirect = 0;
    int background = 0;
    char last_command[BUFFER_SIZE] = "";
    int inside_if = 0;
    int then_block = 0;
    int else_block = 0;
    char then_command[BUFFER_SIZE] = "";
    char else_command[BUFFER_SIZE] = "";

    setupSignalHandling();
    VariableArray var_array;
    initVariableArray(&var_array);


    while (1) {
        printPrompt();
        int index = readInput(command);  // Use readInput instead of fgets
        if (index == 0) continue;  // Handle empty input

        command[index] = '\0';

        addToHistory(command);

        if (strcmp(command, "!!") != 0) {
            strcpy(last_command, command);
        }
        if (strcmp(command, "!!") == 0) {
            if (strlen(last_command) == 0) {
                printf("No command in history.\n");
                continue;
            }
            printf("%s\n", last_command);
            strcpy(command, last_command);
            continue;
        }

        // Check for variable assignment
        if (command[0] == '$' && strstr(command, " = ") != NULL) {
            char *name = strtok(command, " = ");
            char *value = strtok(NULL, " = ");
            if (name && value) {
                setVariable(&var_array, name + 1, value); // name + 1 to skip the $
            }
            continue;
        }
        substituteVariables(command, &var_array);
        if (strchr(command, '|') != NULL && strstr(command, "if") == NULL && inside_if == 0) {
            executePipeline(command, &last_exit_status, 0);
        } else {
            int arg_count = parseInput(command, args, &outfile, &infile, &append, &stderr_redirect, &background);
            if (arg_count == 0) continue;

            if (strcmp(args[0], "if") == 0) {
                inside_if = 1;
                then_block = 0;
                else_block = 0;
                then_command[0] = '\0';
                else_command[0] = '\0';
                executeIfCommand(args, outfile, infile, append, stderr_redirect, background, &last_exit_status);
                continue;
            }

            if (inside_if) {
                if (strcmp(args[0], "then") == 0) {
                    then_block = 1;
                    else_block = 0;
                    continue;
                } else if (strcmp(args[0], "else") == 0) {
                    else_block = 1;
                    then_block = 0;
                    continue;
                } else if (strcmp(args[0], "fi") == 0) {
                    inside_if = 0;
                    then_block = 0;
                    else_block = 0;

                    if (last_exit_status == 0) {
                        if (strchr(then_command, '|') != NULL) {
                            executePipeline(then_command, &last_exit_status, 0);
                        } else {
                            int len = strlen(then_command);
                            char *copy = malloc(len + 1);
                            strcpy(copy, then_command);
                            char *line = strtok(then_command, " ");
                            int count = 0;
                            while (line != NULL) {
                                count++;
                                line = strtok(NULL, " ");
                            }
                            char *temp[count];
                            line = strtok(copy, " ");
                            for (int i = 0; i < count - 1; ++i) {
                                temp[i] = line;
                                line = strtok(NULL, " ");
                            }
                            temp[count - 1] = NULL;
                            parseInput(command, args, &outfile, &infile, &append, &stderr_redirect, &background);
                            executeExternalCommand(temp, outfile, infile, append, stderr_redirect, background,
                                                   &last_exit_status, 0);
                            free(copy);
                        }
                    } else {
                        if (strchr(else_command, '|') != NULL) {
                            executePipeline(else_command, &last_exit_status, 0);
                        } else {
                            int len = strlen(else_command);
                            char *copy = malloc(len + 1);
                            strcpy(copy, else_command);
                            char *line = strtok(else_command, " ");
                            int count = 0;
                            while (line != NULL) {
                                count++;
                                line = strtok(NULL, " ");
                            }
                            char *temp[count];
                            line = strtok(copy, " ");
                            for (int i = 0; i < count - 1; ++i) {
                                temp[i] = line;
                                line = strtok(NULL, " ");
                            }
                            temp[count - 1] = NULL;

                            parseInput(command, args, &outfile, &infile, &append, &stderr_redirect, &background);
                            executeExternalCommand(temp, outfile, infile, append, stderr_redirect, background,
                                                   &last_exit_status, 0);
                            free(copy);
                        }
                    }
                    continue;
                } else {
                    char temp_command[BUFFER_SIZE] = "";
                    for (int i = 0; args[i] != NULL; i++) {
                        strcat(temp_command, args[i]);
                        strcat(temp_command, " ");
                    }
                    if (then_block) {
                        strcat(then_command, temp_command);
                        strcat(then_command, "\n");

                    } else if (else_block) {
                        strcat(else_command, temp_command);
                        strcat(else_command, "\n");
                    }
                    continue;
                }
            }

            if (strcmp(args[0], "prompt") == 0 && args[1] && strcmp(args[1], "=") == 0 && args[2]) {
                changePrompt(args);
                continue;
            } else if (strcmp(args[0], "echo") == 0) {
                if (args[1] != NULL && strcmp(args[1], "$?") == 0) {
                    printf("%d\n", last_exit_status);
                } else {
                    executeEcho(args);
                }
            } else if (strcmp(args[0], "cd") == 0) {
                executeCd(args);
            } else if (strcmp(args[0], "read") == 0) {
                executeReadCommand(args, &var_array);
            } else if (strcmp(args[0], "quit") == 0) {
                executeQuit();
            } else {

                executeExternalCommand(args, outfile, infile, append, stderr_redirect, background, &last_exit_status,
                                       0);
            }
        }
    }
    return 0;
}

void setupSignalHandling() {
    struct sigaction sa;
    sa.sa_handler = sigintHandler;
    sa.sa_flags = SA_RESTART;
    sigemptyset(&sa.sa_mask);
    if (sigaction(SIGINT, &sa, NULL) == -1) {
        perror("sigaction");
        exit(EXIT_FAILURE);
    }
}

void changePrompt(char **args) {
    // Clear the current prompt
    memset(prompt, 0, BUFFER_SIZE);

    if (strlen(args[2]) + 1 < BUFFER_SIZE) {
        sprintf(prompt, "%s:", args[2]);
    } else {
        strncpy(prompt, args[2], BUFFER_SIZE - 3);
        strcat(prompt, ": ");
    }
}

void executeEcho(char **args) {
    if (args == NULL || args[0] == NULL) {
        return;
    }
    if (args[1] == NULL) {
        printf("\n");
    } else {
        for (int i = 1; args[i] != NULL; i++) {
            printf("%s", args[i]);
            if (args[i + 1] != NULL) {
                printf(" ");
            }
        }
        printf("\n");
    }
}

void executeCd(char **args) {
    if (args[1] == NULL) {
        fprintf(stderr, "cd: expected argument to \"cd\"\n");
    } else {
        if (chdir(args[1]) != 0) {
            perror("cd");
        }
    }
}

int parseInput(char *input, char **args, char **outfile, char **infile, int *append, int *stderr_redirect,
               int *background) {
    int i = 0;
    *outfile = NULL;
    *infile = NULL;
    *append = 0;
    *stderr_redirect = 0;
    *background = 0;

    // Tokenize the input
    char *token = strtok(input, " ");
    while (token != NULL) {
        if (strcmp(token, ">") == 0) {
            token = strtok(NULL, " ");
            if (token != NULL) {
                *outfile = token;
                *append = 0;
            }
        } else if (strcmp(token, ">>") == 0) {
            token = strtok(NULL, " ");
            if (token != NULL) {
                *outfile = token;
                *append = 1;
            }
        } else if (strcmp(token, "2>") == 0) {
            token = strtok(NULL, " ");
            if (token != NULL) {
                *outfile = token;
                *stderr_redirect = 1;
            }
        } else if (strcmp(token, "<") == 0) {
            token = strtok(NULL, " ");
            if (token != NULL) {
                *infile = token;
            }
        } else if (strcmp(token, "&") == 0) {
            *background = 1;
        } else {
            args[i++] = token;
        }
        token = strtok(NULL, " ");
    }
    args[i] = NULL;
    return i;
}


void executeQuit() {
    exit(0);
}

void sigintHandler(int sig) {
    write(STDOUT_FILENO, "You typed Control-C!\n", 21);
    write(STDOUT_FILENO, prompt, strlen(prompt));
}

void executeExternalCommand(char **args, char *outfile, char *infile, int append, int stderr_redirect, int background,
                            int *last_exit_status, int silent) {
    pid_t pid = fork();
    if (pid == 0) {  // Child process
        if (silent) {
            int fd_null = open("/dev/null", O_WRONLY);
            if (fd_null == -1) {
                perror("open");
                exit(EXIT_FAILURE);
            }
            dup2(fd_null, STDOUT_FILENO);
            dup2(fd_null, STDERR_FILENO);
            close(fd_null);
        }

        if (outfile != NULL) {
            int fd;
            if (append) {
                fd = open(outfile, O_WRONLY | O_CREAT | O_APPEND, 0644);
            } else {
                fd = open(outfile, O_WRONLY | O_CREAT | O_TRUNC, 0644);
            }
            if (fd == -1) {
                perror("open");
                exit(EXIT_FAILURE);
            }
            if (stderr_redirect) {
                dup2(fd, STDERR_FILENO);
            } else {
                dup2(fd, STDOUT_FILENO);
            }
            close(fd);
        }

        if (infile != NULL) {
            int fd = open(infile, O_RDONLY);
            if (fd == -1) {
                perror("open");
                exit(EXIT_FAILURE);
            }
            dup2(fd, STDIN_FILENO);
            close(fd);
        }

        if (execvp(args[0], args) == -1) {
            perror("execvp");
            exit(EXIT_FAILURE);
        }
    } else if (pid > 0) {  // Parent process
        if (!background) {
            waitpid(pid, last_exit_status, 0);
        }
    } else {
        perror("fork");
    }
}


void executePipeline(char *command, int *last_exit_status, int silent) {
    char *commands[BUFFER_SIZE];
    int num_pipes = 0;
    int background = 0;

    commands[num_pipes++] = strtok(command, "|");
    while ((commands[num_pipes] = strtok(NULL, "|")) != NULL) {
        num_pipes++;
    }

    int *pipefds = (int *) malloc(2 * (num_pipes - 1) * sizeof(int));
    if (pipefds == NULL) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }

    for (int i = 0; i < num_pipes - 1; i++) {
        if (pipe(pipefds + i * 2) < 0) {
            perror("pipe");
            free(pipefds);
            exit(EXIT_FAILURE);
        }
    }

    int pid;
    int j = 0;
    int fd[2];

    if (silent) {
        if (pipe(fd) == -1) {
            perror("pipe");
            exit(EXIT_FAILURE);
        }
    }

    for (int i = 0; i < num_pipes; i++) {
        pid = fork();
        if (pid == 0) {
            // If not the first command, get input from the previous pipe
            if (i != 0) {
                if (dup2(pipefds[j - 2], 0) < 0) {
                    perror("dup2");
                    free(pipefds);
                    exit(EXIT_FAILURE);
                }
            }
            // If not the last command, output to the next pipe
            if (i != num_pipes - 1) {
                if (dup2(pipefds[j + 1], 1) < 0) {
                    perror("dup2");
                    free(pipefds);
                    exit(EXIT_FAILURE);
                }
            } else if (silent) {
                if (dup2(fd[1], 1) == -1) {
                    perror("dup2");
                    exit(EXIT_FAILURE);
                }
                close(fd[0]);
                close(fd[1]);
            }
            // Close all pipe file descriptors
            for (int k = 0; k < 2 * (num_pipes - 1); k++) {
                close(pipefds[k]);
            }

            char *args[MAX_ARGS];
            int append = 0, stderr_redirect = 0;
            char *outfile = NULL;
            char *infile = NULL;
            parseInput(command, args, &outfile, &infile, &append, &stderr_redirect, &background);
            if (execvp(args[0], args) < 0) {
                perror(args[0]);
                free(pipefds);
                exit(EXIT_FAILURE);
            }
        } else if (pid < 0) {
            perror("fork");
            free(pipefds);
            exit(EXIT_FAILURE);
        }
        j += 2;
    }

    // Close all pipe file descriptors in the parent process
    for (int i = 0; i < 2 * (num_pipes - 1); i++) {
        close(pipefds[i]);
    }
    if (silent) {
        close(fd[1]);
        char buffer[BUFFER_SIZE];
        int nbytes;
        while ((nbytes = read(fd[0], buffer, sizeof(buffer) - 1)) > 0) {
            buffer[nbytes] = '\0';
        }
        close(fd[0]);
    }

    // Wait for all child processes
    for (int i = 0; i < num_pipes; i++) {
        waitpid(pid, last_exit_status, 0);
    }

    *last_exit_status = WEXITSTATUS(*last_exit_status);
    free(pipefds);
}

void initVariableArray(VariableArray *array) {
    array->size = 0;
    array->capacity = INITIAL_VAR_CAPACITY;
    array->variables = malloc(sizeof(Variable) * array->capacity);
}

void resizeVariableArray(VariableArray *array) {
    array->capacity *= 2;
    array->variables = realloc(array->variables, sizeof(Variable) * array->capacity);
}

void setVariable(VariableArray *array, const char *name, const char *value) {
    for (int i = 0; i < array->size; i++) {
        if (strcmp(array->variables[i].name, name) == 0) {
            free(array->variables[i].value);
            array->variables[i].value = strdup(value);
            return;
        }
    }
    if (array->size == array->capacity) {
        resizeVariableArray(array);
    }
    array->variables[array->size].name = strdup(name);
    array->variables[array->size].value = strdup(value);
    array->size++;
}

char *getVariable(VariableArray *array, const char *name) {
    for (int i = 0; i < array->size; i++) {
        if (strcmp(array->variables[i].name, name) == 0) {
            return array->variables[i].value;
        }
    }
    return NULL;
}

void substituteVariables(char *command, VariableArray *var_array) {
    char buffer[BUFFER_SIZE];
    char *ptr = command;
    char *buf_ptr = buffer;

    while (*ptr) {
        if (*ptr == '$') {
            ptr++;
            char var_name[BUFFER_SIZE];
            char *var_ptr = var_name;

            while (*ptr && (isalnum(*ptr) || *ptr == '_')) {
                *var_ptr++ = *ptr++;
            }
            *var_ptr = '\0';

            char *value = getVariable(var_array, var_name);
            if (value) {
                while (*value) {
                    *buf_ptr++ = *value++;
                }
            } else {
                *buf_ptr++ = '$';
                char *var_ptr2 = var_name;
                while (*var_ptr2) {
                    *buf_ptr++ = *var_ptr2++;
                }
            }
        } else {
            *buf_ptr++ = *ptr++;
        }
    }
    *buf_ptr = '\0';
    strcpy(command, buffer);
}

void executeReadCommand(char **args, VariableArray *var_array) {
    if (args[1] == NULL) {
        fprintf(stderr, "read: missing variable name\n");
        return;
    }

    char input[BUFFER_SIZE];
    if (fgets(input, sizeof(input), stdin) != NULL) {
        input[strcspn(input, "\n")] = 0;  // Remove newline character
        setVariable(var_array, args[1], input);
    } else {
        fprintf(stderr, "read: failed to read input\n");
    }
}

void printPrompt() {
    printf("\r%s ", prompt);
    fflush(stdout);
}

void addToHistory(const char *command) {
    if (history_index < MAX_HISTORY_SIZE) {
        strcpy(history[history_index++], command);
    } else {
        // If history is full, shift all commands up and add the new command at the end
        for (int i = 1; i < MAX_HISTORY_SIZE; i++) {
            strcpy(history[i - 1], history[i]);
        }
        strcpy(history[MAX_HISTORY_SIZE - 1], command);
    }
    current_history_pos = history_index;
}
void enableRawMode() {
    struct termios raw;
    tcgetattr(STDIN_FILENO, &raw);
    raw.c_lflag &= ~(ECHO | ICANON);
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

void disableRawMode() {
    struct termios raw;
    tcgetattr(STDIN_FILENO, &raw);
    raw.c_lflag |= (ECHO | ICANON);
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

void handleArrowKey(char direction, char *buffer, int *index) {
    if (direction == 'A') {  // Up arrow
        if (current_history_pos > 0) {
            current_history_pos--;
            strcpy(buffer, history[current_history_pos]);
            *index = strlen(buffer);
            // Clear the current line
            printf("\33[2K\r");
            printPrompt();
            printf("%s", buffer);
            fflush(stdout);
        }
    } else if (direction == 'B') {  // Down arrow
        if (current_history_pos < history_index - 1) {
            current_history_pos++;
            strcpy(buffer, history[current_history_pos]);
            *index = strlen(buffer);
            // Clear the current line
            printf("\33[2K\r");
            printPrompt();
            printf("%s", buffer);
            fflush(stdout);
        } else if (current_history_pos == history_index - 1) {
            current_history_pos++;
            buffer[0] = '\0';
            *index = 0;
            // Clear the current line
            printf("\33[2K\r");
            printPrompt();
            fflush(stdout);
        }
    }
}


int readInput(char *buffer) {
    enableRawMode();

    int index = 0;
    current_history_pos = history_index; // Reset current history position

    while (1) {
        fflush(stdout);
        int c = getchar();

        if (c == '\r' || c == '\n') {
            buffer[index] = '\0';
            printf("\n");
            break;
        } else if (c == 127 || c == '\b') {  // Handle backspace
            if (index > 0) {
                buffer[--index] = '\0';
                printf("\b \b");
            }
        } else if (c == 27) {  // Escape sequence
            if (getchar() == '[') {
                c = getchar();
                handleArrowKey(c, buffer, &index);
                continue;
            }
        } else {
            buffer[index++] = c;
            printf("%c", c);
        }
        fflush(stdout);
    }

    disableRawMode();
    return index;
}