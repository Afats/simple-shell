// cowrie.c a simple shell


// Mustafa Dohadwalla, z5232937


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <fcntl.h>
#include <unistd.h>
#include <spawn.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>


// PUT EXTRA `#include'S HERE


#define MAX_LINE_CHARS 1024
#define INTERACTIVE_PROMPT "cowrie> "
#define DEFAULT_PATH "/bin:/usr/bin"
#define WORD_SEPARATORS " \t\r\n"
#define DEFAULT_HISTORY_SHOWN 10

// These characters are always returned as single words
#define SPECIAL_CHARS "!><|"


// PUT EXTRA `#define'S HERE


static void execute_command(char **words, char **path, char **environment);
static void do_exit(char **words);
static int is_executable(char *pathname);
static char **tokenize(char *s, char *separators, char *special_chars);
static void free_tokens(char **tokens);
static FILE *record_cowrie(char **words);
static void rp_history();
static char* rp_history_pull(char *program);
static int line_check(char *program);


// PUT EXTRA FUNCTION PROTOTYPES HERE


int main(void) {
    //ensure stdout is line-buffered during autotesting
    setlinebuf(stdout);

    // Environment variables are pointed to by `environ', an array of
    // strings terminated by a NULL value -- something like:
    //     { "VAR1=value", "VAR2=value", NULL }
    extern char **environ;

    // grab the `PATH' environment variable;
    // if it isn't set, use the default path defined above
    char *pathp;
    if ((pathp = getenv("PATH")) == NULL) {
        pathp = DEFAULT_PATH;
    }
    char **path = tokenize(pathp, ":", "");

    char *prompt = NULL;
    // if stdout is a terminal, print a prompt before reading a line of input
    if (isatty(1)) {
        prompt = INTERACTIVE_PROMPT;
    }

    // main loop: print prompt, read line, execute command
    while (1) {
        if (prompt) {
            fputs(prompt, stdout);
        }

        char line[MAX_LINE_CHARS];
        if (fgets(line, MAX_LINE_CHARS, stdin) == NULL) {
            break;
        }

        char **command_words = tokenize(line, WORD_SEPARATORS, SPECIAL_CHARS);
        execute_command(command_words, path, environ);
        free_tokens(command_words);

    }

    free_tokens(path);
    return 0;
}


//
// Execute a command, and wait until it finishes.
//
//  * `words': a NULL-terminated array of words from the input command line
//  * `path': a NULL-terminated array of directories to search in;
//  * `environment': a NULL-terminated array of environment variables.
//
static void execute_command(char **words, char **path, char **environment) {
    assert(words != NULL);
    assert(path != NULL);
    assert(environment != NULL);

    char *program = words[0];

    if (program == NULL) {
        // nothing to do
        return;
    }   

    // history file open to write commands in 
    // no ! in input
    if ((strchr(program, '!') != 0))  {
        
        FILE * fp;
        fp = record_cowrie(words);
        fclose(fp);
        return;
    }


    if (strcmp(program, "exit") == 0) {
        do_exit(words);
        // do_exit will only return if there is an error
        return;
    }

    // ==== SUBSET 0 ====

    // cd
    if (strcmp(program, "cd") == 0) {
       
        if (words[1] == NULL) {
            //printf("Entered here too!\n");
            if (chdir(getenv("HOME")) != 0) {
                perror("cd");
                return;
            }
            //return;
        }

        else {
            //printf("Entered here three!\n");
            if (chdir(words[1]) != 0) {
                fprintf(stderr, "cd: %s: No such file or directory\n", words[1]);
                return;
            }
        }

    }

    // pwd
    if (strcmp(program, "pwd") == 0) {
        char dir[1000];

        if (getcwd(dir, sizeof(dir)) == NULL) {
            perror("pwd");
            return;
        }

        else {
            printf("current directory is '%s'\n", dir);
            return;
        }

    }

    // ==== SUBSET 1 ==== 

    // create custom path to execute 
    if (strrchr(program, '/') == NULL && (strcmp(program, "cd") != 0 && strcmp(program, "pwd") != 0) && strcmp(program, "history") != 0) {

        int i = 0;
        while (path[i] != NULL) {
            
            char CStr[1000];
            snprintf(CStr, 1000 , "%s/%s", path[i], program);
            //puts(CStr);

            if (is_executable(CStr)) {
                pid_t pid;

                if (posix_spawn(&pid, CStr, NULL, NULL, words, environment) != 0) {
                    perror("spawn");
                    return;
                }

                int exit_status;
                if (waitpid(pid, &exit_status, 0) == -1) {
                    perror("waitpid");
                    return;
                }

                printf("%s exit status = %d\n", CStr, WEXITSTATUS(exit_status));
                return;
            } 

            i++;
        }

        // command not found
        if (path[i] == NULL) {

            fprintf(stderr, "%s: command not found\n", program);
            return;
        }
    }

    // path name is part of argument, no need to create custom string
    else if (strrchr(program, '/') != NULL) {
        
        int done = 0;
        if (is_executable(program)) {
            
            done = 1;
            pid_t pid;

            if (posix_spawn(&pid, program, NULL, NULL, words, environment) != 0) {
                perror("spawn");
                return;
            }

            int exit_status;
            if (waitpid(pid, &exit_status, 0) == -1) {
                perror("waitpid");
                return;
            }

            printf("%s exit status = %d\n", program, WEXITSTATUS(exit_status));
            return;
        } 

        if (done == 0) {
            
            fprintf(stderr, "%s: command not found\n", program);
            return;
        }
    }

    // ==== SUBSET 2 ====

    // cheeck : history might be pulling details from the wrong file
    if (strcmp(program, "history") == 0) {
        rp_history();
    }

    if(strchr(program, '!') != NULL) {
        rp_history_pull(program); 
    }

}


// PUT EXTRA FUNCTIONS HERE

// history file open to write commands in 
// no ! in input (check for history as well)
static FILE *record_cowrie(char **words) {
        
        FILE *fp;

        // open the file for writing
        char buff[1000];
        char *val = getenv("HOME");
        char *file = "/.cowrie_history";
        snprintf(buff, sizeof(buff), "%s%s", val, file);
        fp = fopen(buff,"a+");

        /*if(fp == NULL) {
            printf("Error!");   
            exit(1);             
        }*/

        // write to file
        for (int i = 0; words[i] != NULL; i++) {
            fprintf(fp, "%s ", words[i]);
        }

        char newline[2] = "\n";
        fprintf(fp, "%s", newline);

        return fp;
}

// print hisotry file
static void rp_history() {
    
    FILE *fp;
    // open the file for writing
    char buff[1000];
    char *val = getenv("HOME");
    char *file = "/.cowrie_history";
    snprintf(buff, sizeof(buff), "%s%s", val, file);
    fp = fopen (buff,"r");

    if (fp == NULL) {
        printf("Empty history file!\n");   
        exit(1);             
    }

    int c;
    int i = 0;
    c = fgetc(fp);
    printf("%d: ", i);
    while (c != EOF) {

        if (c == '\n') {
            printf("%c", c);
            i++;
            printf("%d: ", i);
        }
        
        else {
            printf("%c", c);
        }

        c = fgetc(fp);
    }

    fclose(fp);
    return;
}

// get line number and check if input is valid
static int line_check(char *program) {

    int rVal = -1;
    for (int line = 0; line <1000; line++) {

        char cline[1000];
        char c = snprintf(cline, 1000, "%d", line);

        char *ret = strchr(program, c);
        if (atoi(ret) >= 0) {
            rVal = atoi(ret);
            return rVal;
        }
    }

    return rVal;
}
// pull specific instruction from history
static char* rp_history_pull(char *program) {

    char *statement = malloc(sizeof(char) * 1000);

    if (line_check(program) >= 0) {

        FILE *fp;
        // open the file for writing
        char buff[1000];
        char *val = getenv("HOME");
        char *file = "/.cowrie_history";
        snprintf(buff, sizeof(buff), "%s%s", val, file);
        fp = fopen (buff,"r");

        if (fp == NULL) {
            printf("Empty history file!\n");   
            exit(1);             
        }
        
        int i = 0;

        // print statement
        while (fgets (statement, 1000, fp) != NULL) {
            
            //print statement from history
            if (i == line_check(program)) {
                printf("%s", statement);
            }

            i++; 
        }

        fclose(fp);
    }

    return statement;

}

//
// Implement the `exit' shell built-in, which exits the shell.
//
// Synopsis: exit [exit-status]
// Examples:
//     % exit
//     % exit 1
//
static void do_exit(char **words) {
    int exit_status = 0;

    if (words[1] != NULL) {
        if (words[2] != NULL) {
            fprintf(stderr, "exit: too many arguments\n");
        } else {
            char *endptr;
            exit_status = (int)strtol(words[1], &endptr, 10);
            if (*endptr != '\0') {
                fprintf(stderr, "exit: %s: numeric argument required\n",
                        words[1]);
            }
        }
    }

    exit(exit_status);
}


//
// Check whether this process can execute a file.
// Use this function when searching through the directories
// in the path for an executable file
//
static int is_executable(char *pathname) {
    struct stat s;
    return
        // does the file exist?
        stat(pathname, &s) == 0 &&
        // is the file a regular file?
        S_ISREG(s.st_mode) &&
        // can we execute it?
        faccessat(AT_FDCWD, pathname, X_OK, AT_EACCESS) == 0;
}


//
// Split a string 's' into pieces by any one of a set of separators.
//
// Returns an array of strings, with the last element being `NULL';
// The array itself, and the strings, are allocated with `malloc(3)';
// the provided `free_token' function can deallocate this.
//
static char **tokenize(char *s, char *separators, char *special_chars) {
    size_t n_tokens = 0;
    // malloc array guaranteed to be big enough
    char **tokens = malloc((strlen(s) + 1) * sizeof *tokens);


    while (*s != '\0') {
        // We are pointing at zero or more of any of the separators.
        // Skip leading instances of the separators.
        s += strspn(s, separators);

        // Now, `s' points at one or more characters we want to keep.
        // The number of non-separator characters is the token length.
        //
        // Trailing separators after the last token mean that, at this
        // point, we are looking at the end of the string, so:
        if (*s == '\0') {
            break;
        }

        size_t token_length = strcspn(s, separators);
        size_t token_length_without_special_chars = strcspn(s, special_chars);
        if (token_length_without_special_chars == 0) {
            token_length_without_special_chars = 1;
        }
        if (token_length_without_special_chars < token_length) {
            token_length = token_length_without_special_chars;
        }
        char *token = strndup(s, token_length);
        assert(token != NULL);
        s += token_length;

        // Add this token.
        tokens[n_tokens] = token;
        n_tokens++;
    }

    tokens[n_tokens] = NULL;
    // shrink array to correct size
    tokens = realloc(tokens, (n_tokens + 1) * sizeof *tokens);

    return tokens;
}


//
// Free an array of strings as returned by `tokenize'.
//
static void free_tokens(char **tokens) {
    for (int i = 0; tokens[i] != NULL; i++) {
        free(tokens[i]);
    }
    free(tokens);
}
