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
void sigchld_handler(int signum);

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
      break;
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
          exit(1);
        }
        
        dup2(in, STDIN_FILENO); // Redirect standard input to the opened file
        close(in); // Close the file descriptor after redirection
        
      }
      
      // stdout redirection
      if(cmd -> rstdout != NULL){
        int out = open(cmd->rstdout, O_WRONLY | O_CREAT | O_TRUNC, 0644); // Open the output file for writing (create if it doesn't exist)

        if(out < 0){
          perror("Error opening output file"); // Print an error message if the file cannot be opened
          exit(1);
        }
        dup2(out, STDOUT_FILENO); // Redirect standard output to the opened file
        close(out); // Close the file descriptor after redirection
      }

      if(execvp(args[0], args) == -1) { // Execute the command with the provided arguments
        perror("Error executing command");
        exit(1);
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
    if (pgm == NULL || cmd == NULL) {
        return -1;
    }

    // Count children
    int num_children = 0;
    for (Pgm *p = pgm; p != NULL; p = p->next) {
        num_children++;
    }

    pid_t *pids = malloc(num_children * sizeof(*pids));
    if (pids == NULL) {
        perror("malloc");
        return -1;
    }

    // Masking children to show they are foreground and should not be reaped
    sigset_t blockmask, oldmask;
    sigemptyset(&blockmask);
    sigaddset(&blockmask, SIGCHLD);

    if (sigprocmask(SIG_BLOCK, &blockmask, &oldmask) == -1) {
        perror("sigprocmask");
        free(pids);
        return -1;
    }

    signal(SIGINT, SIG_IGN);

    int child_count = 0;
    int success = 1;

    // Changed the concurrency of the function to a loop
    for (Pgm *current = pgm; current != NULL;
         current = current->next) {

        int fd[2] = {-1, -1};

        // Create a pipe if another command exists
        if (current->next != NULL) {
            if (pipe(fd) == -1) {
                perror("pipe");
                success = 0;
                break;
            }
        }

        pid_t pid = fork();

        if (pid < 0) {
            perror("fork");

            if (fd[0] != -1) close(fd[0]);
            if (fd[1] != -1) close(fd[1]);

            success = 0;
            break;
        }

        if (pid == 0) {
            // CHILD

            if (sigprocmask(SIG_SETMASK, &oldmask, NULL) == -1) {
                perror("sigprocmask");
                exit(1);
            }

            if (cmd->background) {
                signal(SIGINT, SIG_IGN);
            } else {
                signal(SIGINT, SIG_DFL);
            }

            // Receive input from the next command
            if (fd[0] != -1) {
                if (dup2(fd[0], STDIN_FILENO) == -1) {
                    perror("dup2 stdin");
                    exit(1);
                }
            }

            // Send output to the previous command
            if (fdin != STDIN_FILENO) {
                if (dup2(fdin, STDOUT_FILENO) == -1) {
                    perror("dup2 stdout");
                    exit(1);
                }
            }

            // Close unused fds
            if (fd[0] != -1) close(fd[0]);
            if (fd[1] != -1) close(fd[1]);
            if (fdin != STDIN_FILENO) close(fdin);

            // Input redirection
            if (current->next == NULL && cmd->rstdin != NULL) {
                int in = open(cmd->rstdin, O_RDONLY);

                if (in == -1) {
                    perror("open input");
                    exit(1);
                }

                if (dup2(in, STDIN_FILENO) == -1) {
                    perror("dup2 input");
                    exit(1);
                }

                close(in);
            }

            // Output redirection
            if (current == pgm && cmd->rstdout != NULL) {
                int out = open(cmd->rstdout,
                               O_WRONLY | O_CREAT | O_TRUNC, 0644);

                if (out == -1) {
                    perror("open output");
                    exit(1);
                }

                if (dup2(out, STDOUT_FILENO) == -1) {
                    perror("dup2 output");
                    exit(1);
                }

                close(out);
            }

            execvp(current->pgmlist[0], current->pgmlist);
            perror("execvp");
            exit(127);
        }

        // PARENT 

        pids[child_count++] = pid;

        // Output pipe is no longer needed
        if (fdin != STDIN_FILENO) {
            close(fdin);
        }

        // Don't read new pipe
        if (fd[0] != -1) {
            close(fd[0]);
        }

        // Save write end 
        fdin = fd[1];
    }

    if (fdin != STDIN_FILENO) {
        close(fdin);
    }

    // Cleanup on failure
    if (!success) {
        for (int i = 0; i < child_count; i++) {
            kill(pids[i], SIGTERM);
        }
    }

    // Wait only after every child has been created
    if (!cmd->background || !success) {
        for (int i = 0; i < child_count; i++) {
            pid_t result;

            do {
                result = waitpid(pids[i], NULL, 0);
            } while (result == -1 && errno == EINTR);

            if (result == -1) {
                perror("waitpid");
                success = 0;
            }
        }
    }

    // Restore signal mask
    if (sigprocmask(SIG_SETMASK, &oldmask, NULL) == -1) {
        perror("sigprocmask");
        success = 0;
    }

    free(pids);

    return success ? 1 : -1;
}

void sigchld_handler(int signum) { // Signal handler for SIGCHLD to reap zombie processes
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


