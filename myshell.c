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


void execute_if_command(char **args, int *last_exit_status) {
    execute_pipeline(args[1], last_exit_status, 1);
}

int main() {
    int last_exit_status = 0;
    char command[BUFFER_SIZE];
    char *args[MAX_ARGS] = {NULL};
    char *outfile = NULL;
    int append = 0;
    int stderr_redirect = 0;
    char last_command[BUFFER_SIZE] = "";
    int inside_if = 0;
    int then_block = 0;
    int else_block = 0;
    char then_command[BUFFER_SIZE] = "";
    char else_command[BUFFER_SIZE] = "";

    struct sigaction sa;
    sa.sa_handler = sigint_handler;
    sa.sa_flags = SA_RESTART;
    sigemptyset(&sa.sa_mask);

    VariableArray var_array;
    init_variable_array(&var_array);

    if (sigaction(SIGINT, &sa, NULL) == -1) {
        perror("sigaction");
        exit(EXIT_FAILURE);
    }

    while (1) {
        print_prompt();
        int index = read_input(command);  // Use read_input instead of fgets
        if (index == 0) continue;  // Handle empty input

        command[index] = '\0';

        add_to_history(command);

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
        }

        // Check for variable assignment
        if (command[0] == '$' && strstr(command, " = ") != NULL) {
            char *name = strtok(command, " = ");
            char *value = strtok(NULL, " = ");
            if (name && value) {
                set_variable(&var_array, name + 1, value); // name + 1 to skip the $
            }
            continue;
        }
        substitute_variables(command, &var_array);

        if (strchr(command, '|') != NULL && strstr(command, "if") == NULL) {
            execute_pipeline(command, &last_exit_status, 1);
        } else {
            parse_input(command, args, &append, &stderr_redirect, &outfile);
            if (args[0] == NULL) continue;

            if (strcmp(args[0], "if") == 0) {
                inside_if = 1;
                then_block = 0;
                else_block = 0;
                then_command[0] = '\0';
                else_command[0] = '\0';
                execute_if_command(args, &last_exit_status);
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
                        char *line = strtok(then_command, "\n");
                        while (line != NULL) {
                            parse_input(line, args, &append, &stderr_redirect, &outfile);
                            execute_external_command(args, append, stderr_redirect, outfile, &last_exit_status);
                            line = strtok(NULL, "\n");
                        }
                    } else {
                        char *line = strtok(else_command, "\n");
                        while (line != NULL) {
                            parse_input(line, args, &append, &stderr_redirect, &outfile);
                            execute_external_command(args, append, stderr_redirect, outfile, &last_exit_status);
                            line = strtok(NULL, "\n");
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
                change_prompt(args);
                continue;
            } else if (strcmp(args[0], "echo") == 0) {
                if (args[1] != NULL && strcmp(args[1], "$?") == 0) {
                    printf("%d\n", last_exit_status);
                } else {
                    execute_echo(args);
                }
            } else if (strcmp(args[0], "cd") == 0) {
                execute_cd(args);
            } else if (strcmp(args[0], "read") == 0) {
                execute_read_command(args, &var_array);
            } else if (strcmp(args[0], "quit") == 0) {
                execute_exit();
            } else {
                execute_external_command(args, append, stderr_redirect, outfile, &last_exit_status);
            }
        }
    }
    return 0;
}



void change_prompt(char **args) {
    // Clear the current prompt
    memset(prompt, 0, BUFFER_SIZE);

    if (strlen(args[2]) + 1 < BUFFER_SIZE) {
        sprintf(prompt, "%s:", args[2]);
    } else {
        strncpy(prompt, args[2], BUFFER_SIZE - 3);
        strcat(prompt, ": ");
    }
}

void execute_echo(char **args) {
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

void execute_cd(char **args) {
    if (args[1] == NULL) {
        fprintf(stderr, "cd: expected argument to \"cd\"\n");
    } else {
        if (chdir(args[1]) != 0) {
            perror("cd");
        }
    }
}

void parse_input(char *command, char **args, int *append, int *stderr_redirect, char **outfile) {
    *append = 0;
    *stderr_redirect = 0;
    int i = 0;
    for (char *token = strtok(command, " "); token; token = strtok(NULL, " ")) {
        if (!strcmp(token, "2>") && (i > 0)) {
            *stderr_redirect = 1;
            *outfile = strtok(NULL, " ");
            break;
        } else if (!strcmp(token, ">>") && (i > 0)) {
            *append = 1;
            *outfile = strtok(NULL, " ");
            break;
        }
        args[i++] = token;
    }
    args[i] = NULL;
}

void execute_exit() {
    exit(0);
}

void sigint_handler(int sig) {
    write(STDOUT_FILENO, "You typed Control-C!\n", 21);
    write(STDOUT_FILENO, prompt, strlen(prompt));
}

void execute_external_command(char **args, int append, int stderr_redirect, char *outfile, int *last_exit_status) {
    int fd;
    pid_t pid = fork();
    if (pid == 0) {  // Child process
        // Redirection logic
        if (stderr_redirect) {
            fd = open(outfile, O_WRONLY | O_CREAT | O_TRUNC, 0666);
            if (fd == -1) {
                fprintf(stderr, "Failed to open file %s: %s\n", outfile, strerror(errno));
                exit(EXIT_FAILURE);
            }
            dup2(fd, STDERR_FILENO);
            close(fd);
        }
        if (append) {
            fd = open(outfile, O_WRONLY | O_CREAT | O_APPEND, 0666);
            if (fd == -1) {
                fprintf(stderr, "Failed to open file %s: %s\n", outfile, strerror(errno));
                exit(EXIT_FAILURE);
            }
            dup2(fd, STDOUT_FILENO);
            close(fd);
        }

        if (execvp(args[0], args) == -1) {
            fprintf(stderr, "Failed to execute %s: %s\n", args[0], strerror(errno));
            exit(EXIT_FAILURE);
        }
    } else if (pid > 0) {  // Parent process
        int status;
        waitpid(pid, &status, 0);
        status = WEXITSTATUS(status);
        *last_exit_status = status;

    } else {
        fprintf(stderr, "Failed to fork: %s\n", strerror(errno));
    }
}

void execute_pipeline(char *command, int *last_exit_status, int silent) {
    char *commands[BUFFER_SIZE];
    int num_pipes = 0;

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
            parse_input(commands[i], args, &append, &stderr_redirect, &outfile);
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

void init_variable_array(VariableArray *array) {
    array->size = 0;
    array->capacity = INITIAL_VAR_CAPACITY;
    array->variables = malloc(sizeof(Variable) * array->capacity);
}

void resize_variable_array(VariableArray *array) {
    array->capacity *= 2;
    array->variables = realloc(array->variables, sizeof(Variable) * array->capacity);
}

void set_variable(VariableArray *array, const char *name, const char *value) {
    for (int i = 0; i < array->size; i++) {
        if (strcmp(array->variables[i].name, name) == 0) {
            free(array->variables[i].value);
            array->variables[i].value = strdup(value);
            return;
        }
    }
    if (array->size == array->capacity) {
        resize_variable_array(array);
    }
    array->variables[array->size].name = strdup(name);
    array->variables[array->size].value = strdup(value);
    array->size++;
}

char *get_variable(VariableArray *array, const char *name) {
    for (int i = 0; i < array->size; i++) {
        if (strcmp(array->variables[i].name, name) == 0) {
            return array->variables[i].value;
        }
    }
    return NULL;
}

void substitute_variables(char *command, VariableArray *var_array) {
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

            char *value = get_variable(var_array, var_name);
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

void execute_read_command(char **args, VariableArray *var_array) {
    if (args[1] == NULL) {
        fprintf(stderr, "read: missing variable name\n");
        return;
    }

    char input[BUFFER_SIZE];
    if (fgets(input, sizeof(input), stdin) != NULL) {
        input[strcspn(input, "\n")] = 0;  // Remove newline character
        set_variable(var_array, args[1], input);
    } else {
        fprintf(stderr, "read: failed to read input\n");
    }
}

void print_prompt() {
    printf("\r%s ", prompt);
    fflush(stdout);
}

void add_to_history(const char *command) {
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

void handle_arrow_key(char direction, char *buffer, int *index) {
    if (direction == 'A') {  // Up arrow
        if (current_history_pos > 0) {
            current_history_pos--;
            strcpy(buffer, history[current_history_pos]);
            *index = strlen(buffer);
            // Clear the current line
            printf("\33[2K\r");
            print_prompt();
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
            print_prompt();
            printf("%s", buffer);
            fflush(stdout);
        } else if (current_history_pos == history_index - 1) {
            current_history_pos++;
            buffer[0] = '\0';
            *index = 0;
            // Clear the current line
            printf("\33[2K\r");
            print_prompt();
            fflush(stdout);
        }
    }
}

int read_input(char *buffer) {
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
                handle_arrow_key(c, buffer, &index);
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