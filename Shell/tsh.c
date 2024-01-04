/*
 * tsh - A tiny shell program with job control
 *
 * Shem Snow u1058151
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>

/*
 * csapp.c - Functions for the CS:APP3e book
 *
 * Updated 10/2016 reb:
 *   - Fixed bug in sio_ltoa that didn't cover negative numbers
 *
 * Updated 2/2016 droh:
 *   - Updated open_clientfd and open_listenfd to fail more gracefully
 *
 * Updated 8/2014 droh:
 *   - New versions of open_clientfd and open_listenfd are reentrant and
 *     protocol independent.
 *
 *   - Added protocol-independent inet_ntop and inet_pton functions. The
 *     inet_ntoa and inet_aton functions are obsolete.
 *
 * Updated 7/2014 droh:
 *   - Aded reentrant sio (signal-safe I/O) routines
 *
 * Updated 4/2013 droh:
 *   - rio_readlineb: fixed edge case bug
 *   - rio_readnb: removed redundant EINTR check
 */

ssize_t sio_puts(char s[]);
ssize_t sio_putl(long v);
static size_t sio_strlen(char s[]);
static void sio_ltoa(long v, char s[], int b);
static void sio_reverse(char s[]);

/* Misc manifest constants */
#define MAXLINE 1024   /* max line size */
#define MAXARGS 128    /* max args on a command line */
#define MAXJOBS 16     /* max jobs at any point in time */
#define MAXJID 1 << 16 /* max job ID */

/* Job states */
#define UNDEF 0 /* undefined */
#define FG 1    /* running in foreground */
#define BG 2    /* running in background */
#define ST 3    /* stopped */

/*
 * Jobs states: FG (foreground), BG (background), ST (stopped)
 * Job state transitions and enabling actions:
 *     FG -> ST  : ctrl-z
 *     ST -> FG  : fg command
 *     ST -> BG  : bg command
 *     BG -> FG  : fg command
 * At most 1 job can be in the FG state.
 */

/* Global variables */
extern char **environ;   /* defined in libc */
char prompt[] = "tsh> "; /* command line prompt (DO NOT CHANGE) */
int verbose = 0;         /* if true, print additional output */
int nextjid = 1;         /* next job ID to allocate */
char sbuf[MAXLINE];      /* for composing sprintf messages */

struct job_t
{                        /* The job struct */
  pid_t pid;             /* job PID */
  int jid;               /* job ID [1, 2, ...] */
  int state;             /* UNDEF, BG, FG, or ST */
  char cmdline[MAXLINE]; /* command line */
};
struct job_t jobs[MAXJOBS]; /* The job list */
/* End global variables */

/* Function prototypes */

/* Here are the functions that you will implement */
void eval(char *cmdline);
int builtin_cmd(char **argv);
void do_bg(int jid);
void do_fg(int jid);
void waitfg(pid_t pid);

void sigchld_handler(int sig);
void sigtstp_handler(int sig);
void sigint_handler(int sig);

/* Here are helper routines that we've provided for you */
int parseline(const char *cmdline, char **argv, int cmdnum);
void sigquit_handler(int sig);

void clearjob(struct job_t *job);
void initjobs(struct job_t *jobs);
int maxjid(struct job_t *jobs);
int addjob(struct job_t *jobs, pid_t pid, int state, char *cmdline);
int deletejob(struct job_t *jobs, pid_t pid);
struct job_t *getjobpid(struct job_t *jobs, pid_t pid);
struct job_t *getjobjid(struct job_t *jobs, int jid);
int pid2jid(pid_t pid);
void listjobs(struct job_t *jobs);

void usage(void);
void unix_error(char *msg);
void app_error(char *msg);
typedef void handler_t(int);
handler_t *Signal(int signum, handler_t *handler);

/********************************** Start of Helper Methods and structs I added *********************************/

sigset_t MaskTheseSignals(int sigs[], int num_sigs); // Creates and returns a new mask containing all the signals in the "sig" array.

int ValidateJob(struct job_t *job, int jid); // Displays an error message and returns a boolean if the specified job is NULL

struct job_t *GetFg(); // Returns the foreground process if there is one. Returns NULL otherwise

void PipeTwoCommands(char *argv1[MAXARGS], char *argv2[MAXARGS], sigset_t mask); // Abstraction of piping two commands (a | b)

/********************************** End of Helper Methods and Structs I added *********************************/

/*
 * main - The shell's main routine that repeatedly prompts the user for input then processes their input.
 */
int main(int argc, char **argv)
{
  char c;
  char cmdline[MAXLINE];
  int emit_prompt = 1; /* emit prompt (default) */

  // Redirect stderr to stdout (so that driver will get all output on the pipe connected to stdout)
  dup2(1, 2);

  /* Parse the command line */
  while ((c = getopt(argc, argv, "hvp")) != EOF)
  {
    switch (c)
    {
    case 'h': /* print help message */
      usage();
      break;
    case 'v': /* emit additional diagnostic info */
      verbose = 1;
      break;
    case 'p':          /* don't print a prompt */
      emit_prompt = 0; /* handy for automatic testing */
      break;
    default:
      usage();
    }
  }

  /* Install the signal handlers */

  /* These are the ones you will need to implement */
  Signal(SIGINT, sigint_handler);   /* ctrl-c */
  Signal(SIGTSTP, sigtstp_handler); /* ctrl-z */
  Signal(SIGCHLD, sigchld_handler); /* Terminated or stopped child */

  /* This one provides a clean way to kill the shell */
  Signal(SIGQUIT, sigquit_handler);

  /* Initialize the job list */
  initjobs(jobs);

  /* Execute the shell's read/eval loop */
  while (1)
  {

    /* Read command line */
    if (emit_prompt)
    {
      printf("%s", prompt);
      fflush(stdout);
    }
    if ((fgets(cmdline, MAXLINE, stdin) == NULL) && ferror(stdin))
      app_error("fgets error");
    if (feof(stdin))
    { /* End of file (ctrl-d) */
      fflush(stdout);
      exit(0);
    }

    /* Evaluate the command line */
    eval(cmdline);
    fflush(stdout);
    fflush(stdout);
  }

  exit(0); /* control never reaches here */
}

/*
 * eval - Evaluate the command line that the user has just typed in
 *
 * - If the user has requested a built-in command (quit, jobs, bg or fg) then execute it immediately.
 * - Otherwise, fork a child process and run the job (the user's request) in the context of the child.
 *
 * - If the job (the user's request) is running in the foreground, wait for it to terminate and then return.
 *
 * Note: Each child process must have a unique process group ID so that our background children don't receive SIGINT (SIGTSTP)
 * from the kernel when we type ctrl-c (ctrl-z) at the keyboard.
 *
 * @param *cmdline: A pointer the the start of the user's input.
 */
void eval(char *cmdline)
{ 
  // The two argv for execve()
  char *argv1[MAXARGS];
  char *argv2[MAXARGS];

  // Boolean that indicates if the job should run in the background. If this is false then it should run in the foreground.
  int bg;

  // If the line contains two commands (the pipe operator "|") then 'split' it into two strings
  // by creating a pointer to the pipeline character "|".
  char *cmd2 = strchr(cmdline, '|');

  // Point the second command to its beginning.
  if (cmd2 != NULL && 2 <= (cmd2 - cmdline) && 3 <= strlen(cmd2))
  {
    // Terminate the first command with newline and null character
    cmd2--;         // Now cmd2 points at the null terminator at the end of the first command.
    cmd2[0] = '\n'; // Replace "0" null terminator with "\n"
    cmd2[1] = '\0'; // Replace the "|" after it with the null terminator "\0"

    // Repoint the second command's pointer to its beginning.
    cmd2 += 3;
  }

  // Parse the first command.
  bg = parseline(cmdline, argv1, 1);

  // If the first command is empty then you're done.
  if (argv1[0] == NULL)
    return;
  // If it's built-in then immediately execute it and return
  if (builtin_cmd(argv1))
    return;

  // The main branch should block SIGCHLD signals before it forks the child.
  int interestingSignals[] = {SIGCHLD, SIGINT, SIGTSTP};
  sigset_t mask = MaskTheseSignals(interestingSignals, 3);
  sigprocmask(SIG_BLOCK, &mask, NULL);

  // If there's a second command then parse it as well then execute both commands with a pipe.
  if (cmd2 != NULL) {
    parseline(cmd2, argv2, 2);
    PipeTwoCommands(argv1, argv2, mask);
    return;
  }

  // Else the command is a filepath. Fork and run the program in a child process.

  // Fork the child.
  pid_t pid_filepath_process = fork();

  // If in the child process
  if (!pid_filepath_process)
  {
    // Set the process group ID so any children branched off from here will be killed by the same signals as this process.
    setpgid(0, 0);

    // Unblock incoming signals.
    sigprocmask(SIG_UNBLOCK, &mask, NULL);

    // Run the job in the child process.
    if(execve(argv1[0], argv1, environ) == -1) {
      // Display an error if execve failed
      printf("%s: Command not found\n", argv1[0]);
      exit(1);
    }
  }

  // Else this is the parent process.

  // If the job (the user's request) is running in the foreground, wait for it to terminate and then return.
  if (!bg)
  {
    // Add the user's request to the job list in the foreground
    addjob(jobs, pid_filepath_process, FG, cmdline);

    // Continue receiving the blocked signals
    sigprocmask(SIG_UNBLOCK, &mask, NULL);

    // Wait for the user's request to leave the foreground.
    waitfg(pid_filepath_process);
  }
  // Else the user's request is running in the background.
  else
  {
    // Add the job (user's request) to the job list in the background.
    addjob(jobs, pid_filepath_process, BG, cmdline);

    // Print and flush an indication that the job was added
    printf("[%d] (%d) %s", pid2jid(pid_filepath_process), pid_filepath_process, cmdline);
    fflush(stdout);

    // Continue receiving the blocked signals.
    sigprocmask(SIG_UNBLOCK, &mask, NULL);
  }
}

/*
 * This helper method is just an abstraction for executing the pipe because my eval() was so big and ugly before.
 *
 * @param argv1: The first process
 * @param argv2: The second process
 * @param mask:  An array of signals to unblock
 */
void PipeTwoCommands(char *argv1[MAXARGS], char *argv2[MAXARGS], sigset_t mask)
{
  // Connect the two commands with a pipe.
  int file_descriptors[2];
  
  // Pipe and make so that the previous command (if there is one) finishes before this continues.
  if(pipe(file_descriptors) == -1){
    perror("PIPE");
    exit(1);
  }

  // Fork the first command
  pid_t pid_first_command = fork();

  // If in the first command's process (the child)
  if (!pid_first_command){

    // Unblock incoming signals.
    sigprocmask(SIG_UNBLOCK, &mask, NULL);

    // Close the original read location.
    close(file_descriptors[0]);

    // Then change the write location of this branch to the pipe.
    dup2(file_descriptors[1], 1);
    close(file_descriptors[1]);

    // Execute the first command and write its output to the file descriptor so it can be read by the second command.
    if(execve(argv1[0], argv1, environ) == -1) {
      // Display an error if execve failed
      printf("%s: Command not found\n", argv1[0]);
      exit(1);
    }
  }

  // Close the original write location.
  close(file_descriptors[1]);

  // Else still in the parent, fork again for the second command
  pid_t pid_second_command = fork();

  // If in the second command's branch
  if (!pid_second_command){

    // Unblock incoming signals.
    sigprocmask(SIG_UNBLOCK, &mask, NULL);
    
    // Then change the read location of the second command to the file descriptor.
    dup2(file_descriptors[0], 0);
    close(file_descriptors[0]);

    // Execute the second branch
    if(execve(argv2[0], argv2, environ) == -1) {
      // Display an error if execve failed
      printf("%s: Command not found\n", argv2[0]);
      exit(1);
    }
  }
  // Else we're still in the parent. Unblock incoming signals.
    sigprocmask(SIG_UNBLOCK, &mask, NULL);

  // Close the file descriptors so they don't interrupt the children.
  close(file_descriptors[0]);

  // Wait for the first command to finish then use its output.
  int status = -1;
  waitpid(pid_first_command, &status, WUNTRACED | WNOHANG);
  waitpid(pid_second_command, &status, WUNTRACED | WNOHANG);
  wait(NULL);
  wait(NULL);
}

/*
 * parseline - Parse the command line and build the argv array.
 *
 * Characters enclosed in single quotes are treated as a single
 * argument.  Return true if the user has requested a BG job, false if
 * the user has requested a FG job.
 */
int parseline(const char *cmdline, char **argv, int cmdnum)
{
  static char array1[MAXLINE]; /* holds local copy of command line */
  static char array2[MAXLINE]; /* holds local copy of 2nd command line */
  char *buf;                   /* ptr that traverses command line */
  char *delim;                 /* points to first space delimiter */
  int argc;                    /* number of args */
  int bg;                      /* background job? */

  if (cmdnum == 1)
    buf = array1;
  else
    buf = array2;

  strcpy(buf, cmdline);

  buf[strlen(buf) - 1] = ' ';   /* replace trailing '\n' with space */
  while (*buf && (*buf == ' ')) /* ignore leading spaces */
    buf++;

  /* Build the argv list */
  argc = 0;
  if (*buf == '\'')
  {
    buf++;
    delim = strchr(buf, '\'');
  }
  else
  {
    delim = strchr(buf, ' ');
  }

  while (delim)
  {
    argv[argc++] = buf;
    *delim = '\0';
    buf = delim + 1;
    while (*buf && (*buf == ' ')) /* ignore spaces */
      buf++;

    if (*buf == '\'')
    {
      buf++;
      delim = strchr(buf, '\'');
    }
    else
    {
      delim = strchr(buf, ' ');
    }
  }
  argv[argc] = NULL;

  if (argc == 0) /* ignore blank line */
    return 1;

  /* should the job run in the background? */
  if ((bg = (*argv[argc - 1] == '&')) != 0)
  {
    argv[--argc] = NULL;
  }

  return bg;
}

/*
 * builtin_cmd - If the user has typed a built-in command then execute it immediately.
 *
 * List of built-in commands: quit, fg, bg, and jobs
 *
 * @param: **argv: The user's input to the program (command arg1 arg2).
 * @returns a boolean that indicates if the command is built-in.
 */
int builtin_cmd(char **argv)
{
  // The first argument (a string) is the command. Create a pointer to it.
  char *cmd = argv[0];

  // See if this command is recognized by searching for known strings and the start of the command.

  // The "jobs" command lists all background jobs.
  if (!strcmp(cmd, "jobs"))
  {
    // "jobs" is a global struct that's visible to the entire program.
    listjobs(jobs);

    // Return from this method. You're done.
    return 1;
  }

  // The "quit" command simply exits the shell with exit code zero (no errors).
  if (!strcmp(cmd, "quit"))
  {
    exit(0);
  }

  // Check for "bg" and "fg".
  if (!strcmp(cmd, "bg") || !strcmp(cmd, "fg"))
  {

    // If so then this command has a JobID.
    int jid;

    // Ignore the command if it has no arguments. It is still recognized so return true.
    if (argv[1] == NULL)
    {
      printf("%s command requires a %%jobid argument\n", argv[0]);
      return 1;
    }

    // If the first argument is a JobID then save it.
    if (argv[1][0] == '%')
    {
      jid = atoi(&argv[1][1]);
    }
    // else this is a built-in command but the arguments are invalid. Indicate this before returning true.
    else
    {
      printf("%s: argument must be a %%jobid\n", argv[0]);
      return 1;
    }

    // Check if this command is "bg". If it is then execute it.
    if (!strcmp(cmd, "bg"))
      do_bg(jid);

    // Otherwise it is "fg". Execute it.
    else
      do_fg(jid);

    return 1;
  }

  // Ignore single "&"
  if (!strcmp(cmd, "&"))
  {
    return 1;
  }

  // If execution got here then this is not a built-in command.
  return 0;
}

/*
 * "do in the background"
 * Restarts the specified job by sending it a SIGCONT signal, and then runs it in the BACKGROUND.
 *
 * Processes that are run in the background will display the following message in the shell
 * [JID] (PID) ./command arguments &
 */
void do_bg(int jid)
{
  // Create a pointer to this job.
  struct job_t *job = getjobjid(jobs, jid);

  // Display the error message and return if it's null.
  if (!ValidateJob(job, jid))
    return;

  // Display a message in the shell to indicate that this process is running.
  sio_puts("[");
  sio_putl(job->jid);
  sio_puts("] ");
  sio_puts("(");
  sio_putl(job->pid);
  sio_puts(") ");
  sio_puts(job->cmdline);

  // Restart the job. The negative pid (-pid) indicates to kill the process first if it is already running.
  kill(-1 * job->pid, SIGCONT);

  // Send the job to the background.
  job->state = BG;

  return;
}

/*
 * "do in the foreground"
 * Restarts the specified job by sending it a SIGCONT signal, and then runs it in the FOREGROUND.
 *
 * Unless the job is NULL in which case the error message is printed with format "%<jid> No such job".
 */
void do_fg(int jid)
{

  // Create a pointer to this job.
  struct job_t *job = getjobjid(jobs, jid);

  // Display the error message and return if it's null.
  if (!ValidateJob(job, jid))
    return;

  // Restart the job. The negative pid (-pid) indicates to kill the process first if it is already running.
  kill(-1 * job->pid, SIGCONT);

  // Send the job to the foreground.
  job->state = FG;

  // Block this thread until the job is complete.
  waitfg(job->pid);

  return;
}

/*
 * waitfg - Blocks the current process until the specified process is no longer the foreground process.
 */
void waitfg(pid_t pid)
{
  // Create a pointer to the specified job
  struct job_t *job = getjobpid(jobs, pid);

  // Block incoming signals in the current process.
  sigset_t mask;
  sigemptyset(&mask);
  sigaddset(&mask, SIGCHLD);
  sigset_t prev_mask;
  sigprocmask(SIG_BLOCK, &mask, &prev_mask);

  // Wait until the specified job is no longer in the foreground.
  while (job->state == FG)
    sigsuspend(&prev_mask);

  // Then unblock incoming signals in the current process.
  sigprocmask(SIG_SETMASK, &prev_mask, NULL);

  return;
}

/*
 * Helper method to determine if a job is valid.
 * It checks if the job is NULL and if it is then displays the error message.
 *
 * @returns a boolean indicating whether or not the job is valid. 1 is valid. 0 is invalid.
 */
int ValidateJob(struct job_t *job, int jid)
{
  if (job == NULL)
  {
    sio_puts("%");
    sio_putl(jid);
    sio_puts(": No such job\n");

    return 0;
  }

  return 1;
}

/*
 * Returns a pointer to the foreground process if there is one otherwise returns NULL.
 */
struct job_t *GetFg()
{
  // Iterate through every single job and see if it's in the foreground
  for (int i = 0; i < sizeof(jobs); i++)
  {
    // If it is then return a pointer to it.
    if (jobs[i].state == FG)
      return &jobs[i];
  }
  // else return NULL.
  return NULL;
}
/*****************
 * Signal handlers
 *****************/

/*
 * sigchld_handler - Signal Child Handler
 *
 * - Whenever a child job becomes a zombie (terminates) or stops because it received a SIGSTOP or SIGTSTP signal
 *    then the kernel sends a SIGCHLD to the shell.
 *
 * - The handler (this method) reaps all available zombie children, but doesn't wait for any other currently running children
 *    to terminate.
 *
 * @param sig: This is the signal number that was recieved upond the child's termination or stop.
 */
void sigchld_handler(int sig)
{
  // For each of the jobs, check what it's status is and handle appropriately.
  for (int i = 0; i < MAXJOBS; i++)
  {
    // Get the job's pid
    int pid = jobs[i].pid;

    // If the pid !=0 then wait for the children to finish then check the new status
    if (pid)
    {
      // Wait for the child processes to finish.
      int status = -1;
      waitpid(pid, &status, WUNTRACED | WNOHANG);

      // STOPPED
      if (WIFSTOPPED(status))
      {
        // Display the message.
        sio_puts("Job [");
        sio_putl(jobs[i].jid);
        sio_puts("] (");
        sio_putl(jobs[i].pid);
        sio_puts(") stopped by signal ");
        sio_putl(WSTOPSIG(status));
        sio_puts("\n");

        // Change the state.
        jobs[i].state = ST;
      }

      // Exited because of SIGSTOP or SIGTSTP.
      else if (WIFSIGNALED(status))
      {
        // Display the exit message.
        sio_puts("Job [");
        sio_putl(jobs[i].jid);
        sio_puts("] (");
        sio_putl(jobs[i].pid);
        sio_puts(") terminated by signal ");
        sio_putl(WTERMSIG(status));
        sio_puts("\n");

        // Reap this child
        deletejob(jobs, jobs[i].pid);
      }

      // EXITED on completion.
      else if (WIFEXITED(status))
      {
        // Reap this child with no exit message.
        deletejob(jobs, jobs[i].pid);
      }
    }
    // Else the child is still running. Do not wait for it.
  }

  // Now continue excepting incomming signals.
  // sigprocmask(SIG_UNBLOCK, &mask, NULL);

  return;
}

/*
 * sigint_handler -
 * The kernel sends a SIGINT to the shell whenver the user types ctrl-c at the keyboard.
 * Catch it and send it along to the foreground job.
 *
 * @param sig: This is the signal number that was recieved (ctrl + c).
 */
void sigint_handler(int sig)
{
  // Create a mask with all the interesting signals.
  int interestingSignals[] = {SIGCHLD, SIGINT, SIGTSTP};
  sigset_t mask = MaskTheseSignals(interestingSignals, 3);

  // Block them.
  sigprocmask(SIG_BLOCK, &mask, NULL);

  // Send the SIGINT signal to the foreground process.
  kill(-1 * GetFg()->pid, sig);

  // Unblock the blocked signals.
  sigprocmask(SIG_UNBLOCK, &mask, NULL);

  return;
}

/*
 * sigtstp_handler - Signal STOP handler.
 * The kernel sends a SIGTSTP to the shell whenever the user types ctrl-z at the keyboard.
 * Catch it and suspend the foreground job by sending a SIGTSTP.
 *
 * @param sig: This is the signal number that was recieved (ctrl + z).
 */
void sigtstp_handler(int sig)
{
  // Define the set of interesting signals in the current process.
  int interestingSignals[] = {SIGCHLD, SIGINT, SIGTSTP};
  sigset_t mask = MaskTheseSignals(interestingSignals, 3);

  // Block them.
  sigprocmask(SIG_BLOCK, &mask, NULL);

  // Send the SIGTSTP signal to the foreground process.
  kill(-1 * GetFg()->pid, sig);

  // Then unblock all the blocked signals.
  sigprocmask(SIG_UNBLOCK, &mask, NULL);
  return;
}

/*
 * Helper function I added to create a new mask containing all the signals specified in the parameters
 */
sigset_t MaskTheseSignals(int sigs[], int num_sigs)
{
  // Instantiate the mask.
  sigset_t mask;
  // Empty it.
  sigemptyset(&mask);
  // Add all the signals.
  for (int i = 0; i < num_sigs; i++)
  {
    sigaddset(&mask, sigs[i]);
  }
  // Return the created mask.
  return mask;
}

/*********************
 * End signal handlers
 *********************/

/***********************************************
 * Helper routines that manipulate the job list
 **********************************************/

/* clearjob - Clear the entries in a job struct */
void clearjob(struct job_t *job)
{
  job->pid = 0;
  job->jid = 0;
  job->state = UNDEF;
  job->cmdline[0] = '\0';
}

/* initjobs - Initialize the job list */
void initjobs(struct job_t *jobs)
{
  int i;

  for (i = 0; i < MAXJOBS; i++)
    clearjob(&jobs[i]);
}

/* maxjid - Returns largest allocated job ID */
int maxjid(struct job_t *jobs)
{
  int i, max = 0;

  for (i = 0; i < MAXJOBS; i++)
    if (jobs[i].jid > max)
      max = jobs[i].jid;
  return max;
}

/* addjob - Add a job to the job list */
int addjob(struct job_t *jobs, pid_t pid, int state, char *cmdline)
{
  int i;

  if (pid < 1)
    return 0;

  for (i = 0; i < MAXJOBS; i++)
  {
    if (jobs[i].pid == 0)
    {
      jobs[i].pid = pid;
      jobs[i].state = state;
      jobs[i].jid = nextjid++;
      if (nextjid > MAXJOBS)
        nextjid = 1;
      strcpy(jobs[i].cmdline, cmdline);
      if (verbose)
      {
        printf("Added job [%d] %d %s\n", jobs[i].jid, jobs[i].pid, jobs[i].cmdline);
      }
      return 1;
    }
  }
  printf("Tried to create too many jobs\n");
  return 0;
}

/* deletejob - Delete a job whose PID=pid from the job list */
int deletejob(struct job_t *jobs, pid_t pid)
{
  int i;

  if (pid < 1)
    return 0;

  for (i = 0; i < MAXJOBS; i++)
  {
    if (jobs[i].pid == pid)
    {
      clearjob(&jobs[i]);
      nextjid = maxjid(jobs) + 1;
      return 1;
    }
  }
  return 0;
}

/* getjobpid  - Find a job (by PID) on the job list */
struct job_t *getjobpid(struct job_t *jobs, pid_t pid)
{
  int i;

  if (pid < 1)
    return NULL;
  for (i = 0; i < MAXJOBS; i++)
    if (jobs[i].pid == pid)
      return &jobs[i];
  return NULL;
}

/* getjobjid  - Find a job (by JID) on the job list */
struct job_t *getjobjid(struct job_t *jobs, int jid)
{
  int i;

  if (jid < 1)
    return NULL;
  for (i = 0; i < MAXJOBS; i++)
    if (jobs[i].jid == jid)
      return &jobs[i];
  return NULL;
}

/* pid2jid - Map process ID to job ID */
int pid2jid(pid_t pid)
{
  int i;

  if (pid < 1)
    return 0;
  for (i = 0; i < MAXJOBS; i++)
    if (jobs[i].pid == pid)
    {
      return jobs[i].jid;
    }
  return 0;
}

/* listjobs - Print the job list */
void listjobs(struct job_t *jobs)
{
  int i;
  // For all of the jobs
  for (i = 0; i < MAXJOBS; i++)
  {
    // Print the job if it has a valid PIDs (Valid PIDs are positive. Negetive PIDs will be killed).
    if (jobs[i].pid != 0)
    {
      printf("[%d] (%d) ", jobs[i].jid, jobs[i].pid);
      switch (jobs[i].state)
      {
      case BG:
        printf("Running ");
        break;
      case FG:
        printf("Foreground ");
        break;
      case ST:
        printf("Stopped ");
        break;
      default:
        printf("listjobs: Internal error: job[%d].state=%d ", i, jobs[i].state);
      }
      printf("%s", jobs[i].cmdline);
    }
  }
}
/******************************
 * end job list helper routines
 ******************************/

/***********************
 * Other helper routines
 ***********************/

/*
 * usage - print a help message
 */
void usage(void)
{
  printf("Usage: shell [-hvp]\n");
  printf("   -h   print this message\n");
  printf("   -v   print additional diagnostic information\n");
  printf("   -p   do not emit a command prompt\n");
  exit(1);
}

/*
 * unix_error - unix-style error routine
 */
void unix_error(char *msg)
{
  fprintf(stdout, "%s: %s\n", msg, strerror(errno));
  exit(1);
}

/*
 * app_error - application-style error routine
 */
void app_error(char *msg)
{
  fprintf(stdout, "%s\n", msg);
  exit(1);
}

/*
 * Signal - wrapper for the sigaction function
 */
handler_t *Signal(int signum, handler_t *handler)
{
  struct sigaction action, old_action;

  action.sa_handler = handler;
  sigemptyset(&action.sa_mask); /* block sigs of type being handled */
  action.sa_flags = SA_RESTART; /* restart syscalls if possible */

  if (sigaction(signum, &action, &old_action) < 0)
    unix_error("Signal error");
  return (old_action.sa_handler);
}

/*
 * sigquit_handler - The driver program can gracefully terminate the
 *    child shell by sending it a SIGQUIT signal.
 */
void sigquit_handler(int sig)
{
  sio_puts("Terminating after receipt of SIGQUIT signal\n");
  exit(1);
}

/* Put string */
ssize_t sio_puts(char s[])
{
  return write(STDOUT_FILENO, s, sio_strlen(s));
}

/* Put long */
ssize_t sio_putl(long v)
{
  char s[128];
  sio_ltoa(v, s, 10); /* Based on K&R itoa() */
  return sio_puts(s);
}

/* sio_strlen - Return length of string (from K&R) */
static size_t sio_strlen(char s[])
{
  int i = 0;
  while (s[i] != '\0')
    ++i;
  return i;
}

/* sio_ltoa - Convert "long to ASCII" (from K&R)
  It is used to convert a long integer v to a string s in a given base b.

  @param v: The long to be converted.
  @param s: A character array where the resulting string will be stored.
  @param b: The base in which the conversion will be performed (e.g., 10 for decimal, 16 for hexadecimal, etc.).
*/
static void sio_ltoa(long v, char s[], int b)
{
  // "i" is an index variable used to keep track of where to place characters in the result string s.
  // "c" is a temporary variable to hold each of those characters
  int c, i = 0;
  int neg = v < 0;

  // If v is negative then make it positive.
  if (neg)
    v = -v;

  // Calculate the remainder of v/b and converts it to a character. That character is stored in the string "s[]"
  // If that remainder is less than 10 then '0' is added to the ASCII value otherwise 'a' is added to it.
  do
  {
    s[i++] = ((c = (v % b)) < 10) ? c + '0' : c - 10 + 'a';
  } while ((v /= b) > 0);

  // If neg was set earlier then append the "-" character to the string to indicate a negative sign.
  if (neg)
    s[i++] = '-';

  // Terminate the string with null/0
  s[i] = '\0';

  sio_reverse(s);
}

/* sio_reverse - Reverse a string (from K&R) */
static void sio_reverse(char s[])
{
  int temp, i, j;

  for (i = 0, j = strlen(s) - 1; i < j; i++, j--)
  {
    temp = s[i];
    s[i] = s[j];
    s[j] = temp;
  }
}