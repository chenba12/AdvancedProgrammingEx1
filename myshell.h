#ifndef ADVANCEDPROGRAMMINGEX1_MYSHELL_H
#define ADVANCEDPROGRAMMINGEX1_MYSHELL_H

typedef struct {
    char *name;
    char *value;
} Variable;

typedef struct {
    Variable *variables;
    int size;
    int capacity;
} VariableArray;


void parse_input(char *command, char **args, int *append, int *stderr_redirect, char **outfile);

void execute_exit();

void execute_external_command(char **args, int append, int stderr_redirect, char *outfile, int *last_exit_status);

void change_prompt(char **args);

void execute_echo(char **args);

void execute_cd(char **args);

void sigint_handler(int sig);

void execute_pipeline(char *command, int *last_exit_status, int silent);

void init_variable_array(VariableArray *array);

void resize_variable_array(VariableArray *array);

void set_variable(VariableArray *array, const char *name, const char *value);

char *get_variable(VariableArray *array, const char *name);

void substitute_variables(char *command, VariableArray *var_array);

void execute_read_command(char **args, VariableArray *var_array);

void enableRawMode();

void disableRawMode();

void print_prompt();

int read_input(char *buffer);

void add_to_history(const char *command);

void handle_arrow_key(char direction, char *buffer, int *index);

void setup_signal_handling();

void handle_variable_assignment(char *command, VariableArray *var_array);

void
handle_if_else_fi(char *command, char **args, int *inside_if, int *then_block, int *else_block, int *last_exit_status,
                  char *then_command, char *else_command, int append, int stderr_redirect, char *outfile);

void execute_if_command(char **args, int *last_exit_status);


#endif //ADVANCEDPROGRAMMINGEX1_MYSHELL_H
