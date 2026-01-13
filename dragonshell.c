#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <limits.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <fcntl.h>
#include "jobstracker.h"

#define LINE_LENGTH 100 // max input line length we read
#define MAX_ARGS    5 // command + up to 5 args total
#define MAX_LENGTH  20

extern char **environ;

/**
 * @brief Tokenize a C string 
 * 
 * @param str - The C string to tokenize 
 * @param delim - The C string containing delimiter character(s) 
 * @param argv - A char* array that will contain the tokenized strings
 * Make sure that you allocate enough space for the array.
 */
void tokenize(char* str, const char* delim, char ** argv) {
  size_t i = 0;
  char* token;
  token = strtok(str, delim);
  while (token && i < (1 + MAX_ARGS)) { // 1 cmd + MAX_ARGS
    argv[i++] = token;

    token = strtok(NULL, delim);
  }
  argv[i] = NULL;
}

void print_prompt(void) {
  fputs("dragonshell > ", stdout);
  fflush(stdout);
}

// In the shell process, ignore Ctrl-C and Ctrl-Z
void setup_shell_signals(void) {
  struct sigaction sa_ignore;
  sa_ignore.sa_flags = 0;
  sigemptyset(&sa_ignore.sa_mask);
  sa_ignore.sa_handler = SIG_IGN;
  sigaction(SIGINT,  &sa_ignore, NULL);
  sigaction(SIGTSTP, &sa_ignore, NULL);
}


// Reap any finished children without blocking the shell
void reap_finished_jobs(void) {
  int status;
  pid_t pid;

  while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
    jobs_remove(pid); // keep job list clean
  }
}

// Remove " characters from each argument
void strip_double_quotes(char **av) {
  for (size_t i = 0; av && av[i]; ++i) { 
    char *s = av[i], *w = s;
    for (; *s; ++s) if (*s != '"') *w++ = *s; // copy if not "
    *w = '\0';
  }
}

// In child processes, restore default Ctrl-C and Ctrl-Z handling
void default_handler(void) {
  struct sigaction sa_dfl;
  sa_dfl.sa_flags = 0;
  sigemptyset(&sa_dfl.sa_mask);
  sa_dfl.sa_handler = SIG_DFL; 
  sigaction(SIGINT,  &sa_dfl, NULL); 
  sigaction(SIGTSTP, &sa_dfl, NULL); 
}


// Handle a pipe command if present. Returns 1 if a pipe was handled, 0 otherwise.
int handle_pipe (char **args, size_t ac, char *cmdstr) {
  int pipe_pos = -1;

  for (size_t i = 0; i < ac; ++i) {
    if (strcmp(args[i], "|") == 0) {
      pipe_pos = (int)i;
      break;
    }
  }if (pipe_pos >= 0) {
    // Split args into left and right of pipe
    args[pipe_pos] = NULL;
    char **left_argv = args;
    char **right_argv = &args[pipe_pos+1];

    if (!left_argv[0] || !right_argv[0]) {
      fputs("dragonshell: Command not found\n", stdout);
      return 1;
    }

    int fd[2]; // file descriptors for pipe

    if (pipe(fd) < 0) {
      fputs("dragonshell: pipe failed\n", stdout);
      return 1;
    }

    pid_t left_pid = fork();

    if (left_pid == 0) {
      // Left child
      default_handler();

      close(fd[0]);              
      dup2(fd[1], STDOUT_FILENO); 
      close(fd[1]);

      strip_double_quotes(left_argv);

      const char *prog = left_argv[0];
      char pathbuf[PATH_MAX];

      if (strchr(prog, '/') == NULL) {
        snprintf(pathbuf, sizeof(pathbuf), "./%s", prog);
        execve(pathbuf, left_argv, environ);
      } else {
        execve(prog, left_argv, environ);
      }
      fputs("dragonshell: Command not found\n", stdout);
      _exit(1);
    }

    pid_t right_pid = fork();

    if (right_pid == 0) {
      // Right child
      default_handler();

      close(fd[1]);
      dup2(fd[0], STDIN_FILENO);
      close(fd[0]);

      strip_double_quotes(right_argv);

      const char *prog = right_argv[0];
      char pathbuf[PATH_MAX];

      if (strchr(prog, '/') == NULL) {
        snprintf(pathbuf, sizeof(pathbuf), "./%s", prog);
        execve(pathbuf, right_argv, environ);
      } else {
        execve(prog, right_argv, environ);
      }
      fputs("dragonshell: Command not found\n", stdout);
      _exit(1);
    }
    // Parent closes both ends of pipe
    close(fd[0]);
    close(fd[1]);

    jobs_add(left_pid, cmdstr, JOB_RUNNING);
    jobs_add(right_pid, cmdstr, JOB_RUNNING);

    int status;

    waitpid(left_pid, &status, 0);
    waitpid(right_pid, &status, 0);

    jobs_remove(left_pid);
    jobs_remove(right_pid);
    return 1;

  }
  return 0; // no pipe handled
}

int main(int argc, char **argv) {
  // print the string prompt without a newline, before beginning to read
  // tokenize the input, run the command(s), and print the result
  // do this in a loop
  fputs("Welcome to Dragon Shell!\n\n", stdout);

  setup_shell_signals();

  char line[LINE_LENGTH + 1]; //raw input line
  char *args[1 + MAX_ARGS + 1]; //tokenized args

  for (;;) {
    reap_finished_jobs();
    print_prompt();

    if (!fgets(line, sizeof(line), stdin)) {
      printf("\n");
      return 0;
    }

    // Make a copy of the line for jobs tracking
    char cmdstr[LINE_LENGTH + 1];
    strncpy(cmdstr, line, sizeof(cmdstr) - 1);
    cmdstr[sizeof(cmdstr) - 1] = '\0';
    size_t cmdlen = strlen(cmdstr);

    if (cmdlen && cmdstr[cmdlen - 1] == '\n') cmdstr[--cmdlen] = '\0';

    for (size_t i = 0; i < (1 + MAX_ARGS+ 1); ++i){
      args[i] = NULL;
    }
    tokenize(line, " \t\n", args);

    // Count args
    size_t ac = 0;
    while (ac < (1 + MAX_ARGS + 1) && args[ac] != NULL) ++ac;
    if (ac == 0) continue;

    // Check for background execution with &
    int is_bg = 0;

    if (ac > 0 && strcmp(args[ac - 1], "&") == 0) {
      is_bg = 1;
      args[ac - 1] = NULL; // remove & from args
      --ac;

      if (ac == 0) continue;
    }

    // Handle built-in commands: cd, pwd, exit, jobs
    if (strcmp(args[0], "pwd") == 0) {
      char cwd[PATH_MAX];

      if (getcwd(cwd, sizeof(cwd)) != NULL) {
        fputs(cwd, stdout);
        fputc('\n', stdout);
      }

      continue;
    }

    if (strcmp(args[0], "cd") == 0) {
      if (ac <= 1 || args[1] == NULL) {
        fputs("dragonshell: Expected argument to \"cd\"\n", stdout);
      } else {
        if (chdir(args[1]) != 0) {
          fputs("dragonshell: No such file or directory\n", stdout);
        }
      }

      continue;
    }

    if (strcmp(args[0], "jobs") == 0) {
      jobs_print();
      continue;
    }

    if (strcmp(args[0], "exit") == 0) {
      reap_finished_jobs();

      pid_t pids[256];
      char  states[256];

      int n = jobs_collect(pids, states, 256);

      for (int i = 0; i < n; ++i){
        (void)kill(pids[i], SIGTERM);
      }

      for (int i = 0; i < n; ++i){
        (void)kill(pids[i], SIGCONT);
      }
      for (int i = 0; i < n; ++i) {
        int status;
        (void)waitpid(pids[i],&status, 0);
        jobs_remove(pids[i]);
      }

      jobs_clear(); // free any remaining job nodes
      return 0;
    }

    // Handle pipe if present
    if (handle_pipe(args, ac, cmdstr)) {
      continue; 
    }

    // No pipe; handle redirection and normal command execution
    char *infile = NULL;
    char *outfile = NULL;

    char *execv_args[1 + MAX_ARGS + 1];
    size_t exec_i = 0; // execv_args index

    // Parse args for < and >
    for (size_t i = 0; i < ac && exec_i < (1 + MAX_ARGS); ) {
      if (strcmp(args[i], "<") == 0 && i + 1 < ac) {
        infile = args[i + 1];
        i += 2;
        continue;
      }
      if (strcmp(args[i], ">") == 0 && i + 1 < ac) {
        outfile = args[i + 1];
        i += 2;
        continue;
      }
      execv_args[exec_i++] = args[i++]; // normal arg
    }

    execv_args[exec_i] = NULL;
    if (exec_i == 0) continue;

    // Fork and execute command
    pid_t pid = fork();

    if (pid == 0) {
      // child
      default_handler();
      // Redirect input/output to dev/null if background and no file specified
      if (is_bg && !outfile) {
        int devnull = open("/dev/null", O_WRONLY);
        if (devnull >= 0) {
          dup2(devnull, STDOUT_FILENO);
          close(devnull);
        }
      }

      // Handle input/output redirection if requested
      if (infile) {
        int fd = open(infile, O_RDONLY);

        if (fd < 0) _exit(1); 
        dup2(fd, STDIN_FILENO);
        close(fd);
      }

      if (outfile) {
        int fd_out = open(outfile, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (fd_out < 0) _exit(1);

        dup2(fd_out, STDOUT_FILENO);
        close(fd_out);
      }

      strip_double_quotes(execv_args);

      const char *prog = execv_args[0];
      char pathbuf[PATH_MAX];

      if (strchr(prog, '/') == 0) {

        if (snprintf(pathbuf, sizeof(pathbuf), "./%s", prog) >= (int)sizeof(pathbuf)) {
          fputs("dragonshell: Command not found\n", stdout);
          _exit(1);
        }

        execve(pathbuf, execv_args, environ);
      } else {
        execve(prog, execv_args, environ);
      }
      fputs("dragonshell: Command not found\n", stdout);
      _exit(1);
    } else if (pid > 0) { // parent
      if (is_bg){
        jobs_add(pid, cmdstr, JOB_RUNNING);
        printf("PID %d is sent to background\n", (int)pid);
        continue;
      }else{ // foreground wait
        jobs_add(pid, cmdstr, JOB_RUNNING);

        for (;;) {
          int status;
          pid_t w = waitpid(pid, &status, WUNTRACED);
          if (w == -1) break;

          if (WIFSTOPPED(status)) { // stopped
            jobs_update(pid, JOB_SUSPENDED);
            break;
          }

          if (WIFEXITED(status) || WIFSIGNALED(status)) { // exited or killed
            jobs_remove(pid);
            break;
          }
        }
      }
    } else {
      fputs("dragonshell: fork failed\n", stdout);
    }
  }
  return 0;
}