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


int parseInput(char *input, char **args, char **outfile, char **infile, int *append, int *stderr_redirect,
               int *background);

void executeQuit();

void executeExternalCommand(char **args, char *outfile, char *infile, int append, int stderr_redirect, int background,
                            int *last_exit_status, int silent);

void changePrompt(char **args);

void executeEcho(char **args);

void executeCd(char **args);

void sigintHandler(int sig);

void executePipeline(char *command, int *last_exit_status, int silent);

void initVariableArray(VariableArray *array);

void resizeVariableArray(VariableArray *array);

void setVariable(VariableArray *array, const char *name, const char *value);

char *getVariable(VariableArray *array, const char *name);

void substituteVariables(char *command, VariableArray *var_array);

void executeReadCommand(char **args, VariableArray *var_array);

void enableRawMode();

void disableRawMode();

void printPrompt();

int readInput(char *buffer);

void addToHistory(const char *command);

void enableRawMode();

void disableRawMode();

void handleArrowKey(char direction, char *buffer, int *index);

void setupSignalHandling();

void executeIfCommand(char **args, char *outfile, char *infile, int append, int stderr_redirect, int background,
                      int *last_exit_status);


#endif //ADVANCEDPROGRAMMINGEX1_MYSHELL_H
