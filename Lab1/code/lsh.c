/*
 * Main source file for the lsh shell program.
 *
 * You are free to add functions to this file.
 * If you want to add functions in separate files,
 * you will need to modify CMakeLists.txt to compile
 * your additional files.
 *
 * Add appropriate comments to make your code
 * easier for us to grade.
 *
 * Using assert statements is a good way to catch errors early and make debugging easier.
 * Think of them as mini self-checks that ensure your program behaves as expected.
 * By setting up these guardrails, you're creating a more robust and maintainable solution.
 * So go ahead, sprinkle some asserts in your code; they're your friends in disguise!
 *
 * All the best!
 */
#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <signal.h>
#include <errno.h>
#include <unistd.h>
#include "parse.h"
#include <fcntl.h>
#include <sys/wait.h>


static void print_cmd(Command *cmd);
static void print_pgm(Pgm *p);
void stripwhite(char *);
int execute_Command(Command *cmd);
int run_Forks(Pgm *pgm, int fdin, Command *cmd);
void sigchld_handler(int sig);

int main(void)
{
  struct sigaction sa;
  memset(&sa, 0, sizeof sa);
  sa.sa_handler = sigchld_handler;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = SA_RESTART;
  sigaction(SIGCHLD, &sa, NULL);

  for (;;)
  {
    char *line;
    line = readline("> ");
    if (line == NULL) // Check if EOF (Ctrl+D) is reached
    {
      break; // Exit the loop if EOF is reached (Ctrl+D)
    }

    // Remove leading and trailing whitespace from the line
    stripwhite(line);

    // If the stripped line is not blank
    if (*line)
    {
      add_history(line);

      Command cmd;
      if (parse(line, &cmd) == 1)
      {
        if(execute_Command(&cmd) == -1){
          printf("Command ERROR\n");
        }

        print_cmd(&cmd);
      }
      else
      {
        printf("Parse ERROR\n");
      }
    }

    // Free the input buffer
    free(line);
  }

  return 0;
}

//Exec command, return -1 on failure
int execute_Command(Command *cmd) {
  int background = 0;

  if ( cmd == NULL || cmd->pgm == NULL || cmd -> pgm->pgmlist == NULL){
    return -1;
  }
  if (cmd->background){
    background = 1;
  }
  char **args = cmd->pgm->pgmlist;
  if (strcmp(args[0], "exit") == 0) { // Check if the command is "exit"
    exit(0);
  }
  if (strcmp(args[0], "cd") == 0) { // Check if the command is "cd"
    if (chdir(args[1]) == 0 ) { // Change the current working directory
      return 1;
    }
  }

  if (cmd->pgm->next != NULL) { // Check if there is a next command in the pipeline
    if (run_Forks(cmd->pgm, STDIN_FILENO, cmd) != 1) {
      printf("Error executing pipeline\n");
      return -1;
    }
    return 1;
  } else{ // Single command execution
    signal(SIGINT, SIG_IGN);
    pid_t pid = fork();
    if(pid < 0){
      printf("Error forking");
      return -1;
    }
    
    if (pid == 0){ // Child process
      if (background == 1) {
        signal(SIGINT, SIG_IGN);
      } else {
        signal(SIGINT, SIG_DFL);
      }

      if(cmd-> rstdin != NULL){ // Check if input redirection is specified
        int in = open(cmd->rstdin, O_RDONLY); // Open the input file for reading
        
        if (in < 0) {
          perror("Error opening input file"); // Print an error message if the file cannot be opened
          exit(1); // Exit the child process with an error code
        }
        
        dup2(in, STDIN_FILENO); // Redirect standard input to the opened file
        close(in); // Close the file descriptor after redirection
        
      }
      
      // stdout redirection
      if(cmd -> rstdout != NULL){
        int out = open(cmd->rstdout, O_WRONLY | O_CREAT, 0644); // Open the output file for writing (create if it doesn't exist)
      
        if(out < 0){
          perror("Error opening output file"); // Print an error message if the file cannot be opened
          exit(1); // Exit the child process with an error code
        }
        dup2(out, STDOUT_FILENO); // Redirect standard output to the opened file
        close(out); // Close the file descriptor after redirection
      }

      if(execvp(args[0], args) == -1) { // Execute the command with the provided arguments
        perror("Error executing command"); // Print an error message if the command execution fails
        exit(1); // Exit the child process with an error code
      }
    }
    if(background == 1){
      printf("Process running in background with PID: %d\n", pid); // Print the PID of the background process
    } else {
      while (waitpid(pid, NULL, 0) < 0) {
        if (errno == ECHILD || errno != EINTR) break;
      } // Wait for the child process to finish if not running in background
    } 
    return 1; // Return 1 on success, or an error code on failure
  }
}

int run_Forks(Pgm *pgm, int fdin, Command *cmd) {
  if (pgm == NULL) {
    return -1;
  }
  if (pgm->next != NULL) {
    int fd[2];
    if (pipe(fd) < 0) {
      perror("pipe");
      return -1;
    }

    signal(SIGINT, SIG_IGN);

    run_Forks(pgm->next, fd[1], cmd); // Recursively call run_Forks for the next command in the pipeline, passing the write end of the pipe (fd[1]) as the input for the next command
    close(fd[1]); // Close the write end of the pipe in the parent process, as it is no longer needed

    pid_t pid = fork(); // Fork a new process for the current command in the pipeline
    if (pid < 0) { // Check for fork error
      perror("Error forking");
      close(fd[0]);
      return -1;
    }

    if (pid == 0) { // Child process
      if (cmd->background) {
        signal(SIGINT, SIG_IGN);
      } else {
        signal(SIGINT, SIG_DFL); 
      }
      dup2(fd[0], STDIN_FILENO);
      close(fd[0]);

      if (fdin != STDIN_FILENO) {
        dup2(fdin, STDOUT_FILENO);
        close(fdin);
      }

      if (cmd->rstdout != NULL) {
        int outfd = open(cmd->rstdout, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (outfd < 0) {
          perror("Error opening output file");
          exit(1);
        }
        dup2(outfd, STDOUT_FILENO);
        close(outfd);
      }

      execvp(pgm->pgmlist[0], pgm->pgmlist);
      perror("execvp");
      exit(1);
    }

    close(fd[0]);
    if (fdin != STDIN_FILENO) {
      close(fdin);
    }
  if (!cmd->background) {
      while (waitpid(pid, NULL, 0) < 0) {
        if (errno == ECHILD || errno != EINTR) break;
      }
    }
    return 1;

  } else { // Last command in the pipeline
    signal(SIGINT, SIG_IGN);
    pid_t pid = fork();

    if (pid < 0) {
      perror("Error forking");
      if (fdin != STDIN_FILENO) {
        close(fdin);
      }
      return -1;
    }

    if (pid == 0) { // Child process
      if (cmd->background) {
        signal(SIGINT, SIG_IGN);
      } else {
        signal(SIGINT, SIG_DFL); 
      }

      if (fdin != STDIN_FILENO) { // If the input file descriptor is not standard input, redirect it to standard output
        dup2(fdin, STDOUT_FILENO);
        close(fdin);
      }

      if (cmd->rstdin != NULL) { // If there is an input redirection specified, open the input file and redirect standard input to it
        int infd = open(cmd->rstdin, O_RDONLY);
        if (infd < 0) {
          perror("Error opening input file");
          exit(1);
        }
        dup2(infd, STDIN_FILENO);
        close(infd);
      }

      execvp(pgm->pgmlist[0], pgm->pgmlist); // Execute the command with the provided arguments
      perror("execvp");
      exit(1);
    }

    if (fdin != STDIN_FILENO) { // Close the input file descriptor in the parent process if it is not standard input
      close(fdin);
    }
    if (!cmd->background) {
      while (waitpid(pid, NULL, 0) < 0) {
        if (errno == ECHILD || errno != EINTR) break;
      }
    }
    return 1;
  }
}

void sigchld_handler(int sig) { // Signal handler for SIGCHLD to reap zombie processes
  int saved_errno = errno;
  while (waitpid(-1, NULL, WNOHANG) > 0) { }
  errno = saved_errno;
}


/*
 * Print a Command structure as returned by parse on stdout.
 *
 * Helper function, no need to change. Might be useful to study as inspiration.
 */
static void print_cmd(Command *cmd_list)
{
  printf("------------------------------\n");
  printf("Parse OK\n");
  printf("stdin:      %s\n", cmd_list->rstdin ? cmd_list->rstdin : "<none>");
  printf("stdout:     %s\n", cmd_list->rstdout ? cmd_list->rstdout : "<none>");
  printf("background: %s\n", cmd_list->background ? "true" : "false");
  printf("Pgms:\n");
  print_pgm(cmd_list->pgm);
  printf("------------------------------\n");
}

/* Print a linked list of Pgm structures.
 *
 * Helper function, no need to change. It may be useful to study for inspiration.
 */
static void print_pgm(Pgm *p)
{
  if (p == NULL)
  {
    return;
  }
  else
  {
    char **pl = p->pgmlist;

    /* The list is stored in reverse order, so print
     * it in reverse to restore the original order.
     */
    print_pgm(p->next);
    printf("            * [ ");
    while (*pl)
    {
      printf("%s ", *pl++);
    }
    printf("]\n");
  }
}


/* Strip whitespace from the start and end of a string.
 *
 * Helper function, no need to change.
 */
void stripwhite(char *string)
{
  size_t i = 0;

  while (isspace(string[i]))
  {
    i++;
  }

  if (i)
  {
    memmove(string, string + i, strlen(string + i) + 1);
  }

  i = strlen(string) - 1;
  while (i > 0 && isspace(string[i]))
  {
    i--;
  }

  string[++i] = '\0';
}


