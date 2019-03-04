/**
 * @file execute.c
 *
 * @brief Implements interface functions between Quash and the environment and
 * functions that interpret an execute commands.
 *
 * @note As you add things to this file you may want to change the method signature
 */
 //background status, passing current process into create process
#include <stdlib.h>
#include <stdio.h>
#include <limits.h>
#include <unistd.h>
#include <string.h>

// open, close and  write to file
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/wait.h>

#include "execute.h"
#include "quash.h"
#include "deque.h"




// Remove this and all expansion calls to it
/**
 * @brief Note calls to any function that requires implementation
 */
#define IMPLEMENT_ME()                                                  \
  fprintf(stderr, "IMPLEMENT ME: %s(line %d): %s()\n", __FILE__, __LINE__, __FUNCTION__)

//sets up deque struct for processes
IMPLEMENT_DEQUE_STRUCT(piddeque, pid_t);
//sets up deque functions
IMPLEMENT_DEQUE(piddeque, pid_t);
//sets up job deque
typedef struct job_t
{
  piddeque process_list;
  int job_id;
  char* cmd;
}job_t;

IMPLEMENT_DEQUE_STRUCT(BG_job, job_t);
//sets up functions for deque
IMPLEMENT_DEQUE(BG_job, job_t);


//jobs contructor
static job_t _new_job()
{
  return(job_t){
    new_piddeque(1),
    0,
    get_command_string(),
  };
}


//needs work
//implement destructor, free process command and destroy process deque
static void _destroy_job(job_t b)
{
  //getting the size and using it and the destructor to deallocate the process dequeue
  if(b.cmd != NULL) {free(b.cmd);}
  destroy_piddeque(&b.process_list);
}

static void destroy_job(job_t* b)
{
  //getting the size and using it and the destructor to deallocate the process dequeue
  if(b->cmd != NULL) free(b->cmd);
  destroy_piddeque(&b->process_list);
}
/*void destroy_pointers(job_t b){
    _destroy_job(b);
}*/
BG_job bg_jobs;

//delcaring pipes
int pipes[2][2];
//ex: pipes[0][1] write end of pipe 0
//keeps track of which pipe we are using and %2 it to see which pipe we need to use
int pipeUsed = 0;
/***************************************************************************
 * Interface Functions
 ***************************************************************************/

// Return a string containing the current working directory.
char* get_current_directory(bool* should_free) {
  // TODO: Get the current working directory. This will fix the prompt path.
  // HINT: This should be pretty simple
  //IMPLEMENT_ME();
  *should_free = true;
  //char* dbuff = getcwd(NULL, 512);
  // Change this to true if necessary
  /*if (should_free==true){
    should_free = true;
  }*/

  return getcwd(NULL, 512);
}

// Returns the value of an environment variable env_var
const char* lookup_env(const char* env_var) {
  // TODO: Lookup environment variables. This is required for parser to be able
  // to interpret variables from the command line and display the prompt
  // correctly
  // HINT: This should be pretty simple
  //IMPLEMENT_ME();
  //char* var = getenv(env_var);
  // TODO: Remove warning silencers
  //(void) env_var; // Silence unused variable warning

  return getenv(env_var);
}

// Check the status of background jobs
void check_jobs_bg_status() {
  // TODO: Check on the statuses of all processes belonging to all background
  // jobs. This function should remove jobs from the jobs queue once all
  // processes belonging to a job have completed.
  //IMPLEMENT_ME();

  // first check if there is a background job:
  if (is_empty_BG_job(&bg_jobs)) {
    return;
  };

  // to iterate through the job queue we need to get its length:
  int length = length_BG_job(&bg_jobs);
  // for each job, create a current job to be the running job,
  // first process to be the running process in the bg job
  // git the length of the process_list
  for(int i=0; i<length; i++){
    // pop will return the first job in the background job que and remove it from the front.
    job_t curr_job = pop_front_BG_job(&bg_jobs);
    // peek will return the front process without removing it, we need to keep it in
    // until we check if it is complete or running.
    pid_t first_process = peek_front_piddeque(&curr_job.process_list);
    // get process list length.
    int process_list_length = length_piddeque(&curr_job.process_list);

    // for each process, we need to check if it is complete or still running:
    for(int j=0; j<process_list_length; j++){
      pid_t curr_process = pop_front_piddeque(&curr_job.process_list);
      int status;
      /*
      The waitpid() system call suspends execution of the calling process until a child specified by pid argument has changed state. By default, waitpid() waits only for terminated children, but this behavior is modifiable via the options argument, as described below.
      The value of pid can be:
      < -1
      meaning wait for any child process whose process group ID is equal to the absolute value of pid.
      -1
      meaning wait for any child process.
      0
      meaning wait for any child process whose process group ID is equal to that of the calling process.
      > 0*/
      pid_t next_process = waitpid(curr_process, &status, WNOHANG);

      if(next_process == 0) {
        push_back_piddeque(&curr_job.process_list, curr_process);
      }
      // I am not sure if we need to do anything in these cases.
      else if(next_process == -1){}
      else if(next_process == curr_process){}
    }

      if(!is_empty_piddeque(&curr_job.process_list))
        push_back_BG_job(&bg_jobs,curr_job);
      else{
        print_job_bg_complete(curr_job.job_id, first_process, curr_job.cmd);
        _destroy_job(curr_job);
      }

  }

  // TODO: Once jobs are implemented, uncomment and fill the following line
  // print_job_bg_complete(job_id, pid, cmd);
}

// Prints the job id number, the process id of the first process belonging to
// the Job, and the command string associated with this job
void print_job(int job_id, pid_t pid, const char* cmd) {
  printf("[%d]\t%8d\t%s\n", job_id, pid, cmd);
  fflush(stdout);
}

// Prints a start up message for background processes
void print_job_bg_start(int job_id, pid_t pid, const char* cmd) {
  printf("Background job started: ");
  print_job(job_id, pid, cmd);
}

// Prints a completion message followed by the print job
void print_job_bg_complete(int job_id, pid_t pid, const char* cmd) {
  printf("Completed: \t");
  print_job(job_id, pid, cmd);
}

/***************************************************************************
 * Functions to process commands
 ***************************************************************************/
// Run a program reachable by the path environment variable, relative path, or
// absolute path
void run_generic(GenericCommand cmd) {
  // Execute a program with a list of arguments. The `args` array is a NULL
  // terminated (last string is always NULL) list of strings. The first element
  // in the array is the executable
  char* exec = cmd.args[0];
  char** args = cmd.args;

  // TODO: Remove warning silencers
  execvp(exec, args); // Silence unused variable warning

  // TODO: Implement run generic


  perror("ERROR: Failed to execute program");
}

// Print strings
void run_echo(EchoCommand cmd) {
  // Print an array of strings. The args array is a NULL terminated (last
  // string is always NULL) list of strings.
  char** str = cmd.args;

  // TODO: Remove warning silencers
  //(void) str; // Silence unused variable warning

  // TODO: Implement echo
  //IMPLEMENT_ME();
  //int size =
  int k = 0;
  while(str[k] != NULL){
    printf("%s ", str[k]);
    k++;
  }
  printf("\n");
  //free(str);
  // Flush the buffer before returning
  fflush(stdout);
}

// Sets an environment variable
void run_export(ExportCommand cmd) {
  // Write an environment variable
  const char* env_var = cmd.env_var;
  const char* val = cmd.val;

  // TODO: Remove warning silencers
  //(void) env_var; // Silence unused variable warning
  //(void) val;     // Silence unused variable warning

  // TODO: Implement export.
  // HINT: This should be quite simple.
  //IMPLEMENT_ME();
  //changes env_var value to val and since parameter 3 is 1 it overwrites it
  setenv(env_var, val, 1);
}

// Changes the current working directory
void run_cd(CDCommand cmd) {
  // Get the directory name
  const char* dir = cmd.dir;
  //char path[PATH_MAX+1];

  // Check if the directory is valid
  if (dir == NULL) {
    perror("ERROR: Failed to resolve path");
    return;
  }
  // TODO: Change directory
  //char realPath[PATH_MAX +1];
  //chdir (realpath(dir, realPath));
  chdir(dir);
  // TODO: Update the PWD environment variable to be the new current working
  // directory and optionally update OLD_PWD environment variable to be the old
  // working directory.
  setenv("OLD_PWD", lookup_env("PWD"), 1);
  setenv("PWD", dir, 1);
  //IMPLEMENT_ME();
}

// Sends a signal to all processes contained in a job
void run_kill(KillCommand cmd) {
  int signal = cmd.sig;
  int job_id = cmd.job;

  // TODO: Remove warning silencers
  //(void) signal; // Silence unused variable warning
  //(void) job_id; // Silence unused variable warning

  // TODO: Kill all processes associated with a background job
  //IMPLEMENT_ME();
  int numJobs = length_BG_job(&bg_jobs);
  for(int k = 0; k < numJobs; k++)
  {
    job_t tempJob = pop_front_BG_job(&bg_jobs);
    if(job_id == tempJob.job_id){
      int numPro = length_piddeque(&tempJob.process_list);
      for(int j =0; j< numPro; j++){
        pid_t curr_process = pop_front_piddeque(&tempJob.process_list);
        kill(curr_process, signal);
        push_back_piddeque(&tempJob.process_list, curr_process);
      }
    }

    //print_job(tempJob.job_id, peek_front_piddeque(&tempJob.process_list), tempJob.cmd);

    push_back_BG_job(&bg_jobs, tempJob);
  }
}


// Prints the current working directory to stdout
void run_pwd() {
  // TODO: Print the current working directory
  //IMPLEMENT_ME();
  //used get current directory and free it
  bool should_free = false;
  char* dbuff = get_current_directory(&should_free);
  printf("%s\n", dbuff);
  if (should_free) free(dbuff);
  // Flush the buffer before returning
  fflush(stdout);
}

// Prints all background jobs currently in the job list to stdout
void run_jobs() {
  // TODO: Print background jobs
  //IMPLEMENT_ME();
  //if (is_empty_BG_job(&bg_jobs)) return;
  // if there is a background job:
  int length = length_BG_job(&bg_jobs);
  for(int i=0; i<length;i++){
    // pget the current job values
    job_t curr_job = pop_front_BG_job(&bg_jobs);
    // printing the job
    print_job(curr_job.job_id, peek_front_piddeque(&curr_job.process_list), curr_job.cmd);
    // return it back to the que
    push_back_BG_job(&bg_jobs, curr_job);
  }

  // Flush the buffer before returning
  fflush(stdout);
}

/***************************************************************************
 * Functions for command resolution and process setup
 ***************************************************************************/

/**
 * @brief A dispatch function to resolve the correct @a Command variant
 * function for child processes.
 *
 * This version of the function is tailored to commands that should be run in
 * the child process of a fork.
 *
 * @param cmd The Command to try to run
 *
 * @sa Command
 */
void child_run_command(Command cmd) {
  CommandType type = get_command_type(cmd);

  switch (type) {
  case GENERIC:
    run_generic(cmd.generic);
    break;

  case ECHO:
    run_echo(cmd.echo);
    break;

  case PWD:
    run_pwd();
    break;

  case JOBS:
    run_jobs();
    break;

  case EXPORT:
  case CD:
  case KILL:
  case EXIT:
  case EOC:
    break;

  default:
    fprintf(stderr, "Unknown command type: %d\n", type);
  }
}

/**
 * @brief A dispatch function to resolve the correct @a Command variant
 * function for the quash process.
 *
 * This version of the function is tailored to commands that should be run in
 * the parent process (quash).
 *
 * @param cmd The Command to try to run
 *
 * @sa Command
 */
void parent_run_command(Command cmd) {
  CommandType type = get_command_type(cmd);

  switch (type) {
  case EXPORT:
    run_export(cmd.export);
    break;

  case CD:
    run_cd(cmd.cd);
    break;

  case KILL:
    run_kill(cmd.kill);
    break;

  case GENERIC:
  case ECHO:
  case PWD:
  case JOBS:
  case EXIT:
  case EOC:
    break;

  default:
    fprintf(stderr, "Unknown command type: %d\n", type);
  }
}

/**
 * @brief Creates one new process centered around the @a Command in the @a
 * CommandHolder setting up redirects and pipes where needed
 *
 * @note Processes are not the same as jobs. A single job can have multiple
 * processes running under it. This function creates a process that is part of a
 * larger job.
 *
 * @note Not all commands should be run in the child process. A few need to
 * change the quash process in some way
 *
 * @param holder The CommandHolder to try to run
 *
 * @sa Command CommandHolder
 */
void create_process(CommandHolder holder, job_t* curr_job, int currPipe) {
  // Read the flags field from the parser
  bool p_in  = holder.flags & PIPE_IN;
  bool p_out = holder.flags & PIPE_OUT;
  bool r_in  = holder.flags & REDIRECT_IN;
  bool r_out = holder.flags & REDIRECT_OUT;
  bool r_app = holder.flags & REDIRECT_APPEND; // This can only be true if r_out
                                               // is true

  pipeUsed = currPipe-1;
  if(p_out)
  {
    pipe(pipes[currPipe % 2]);
  }
  pid_t pid;
  // TODO: Setup pipes, redirects, and new process
  pid = fork();
  if (pid == 0){
    //redirect to the same place
    if(r_in){
      FILE* in = fopen(holder.redirect_in, "r");
      dup2(fileno(in), STDIN_FILENO);
      fclose(in);
    }
    //redirect to a file
    if (r_out){
      FILE* newFile;
      //check if it is an appending
      if(r_app){
        newFile = fopen(holder.redirect_out, "a");
      }
      else{
        newFile = fopen(holder.redirect_out, "w");
      }
      dup2(fileno(newFile), STDOUT_FILENO);
      //because we are using a file fclose()
      fclose(newFile);
      //newFile = (char*) malloc(size);
    }
    // I have no clue how to do this or any pipe thing SORRY!!!
    //if pipe in
    if(p_in){
      //make a copy
      //pipe[][0]
      dup2(pipes[pipeUsed % 2][0], STDIN_FILENO);
      close(pipes[pipeUsed % 2][1]);
    }

    if(p_out){
      dup2(pipes[currPipe % 2][1], STDOUT_FILENO);
      close(pipes[currPipe % 2][0]);
    }

    //_destroy_job(this->curr_job);
    //destroy_pointers(curr_job);
    destroy_job(curr_job);
    child_run_command(holder.cmd);
    //destroy the job
    exit (0);
    //free(curr_job);

  }
  else{
    push_back_piddeque(&curr_job->process_list, pid);
    parent_run_command(holder.cmd);
    }
  if(p_in)
  {
    close(pipes[pipeUsed % 2][0]);
    close(pipes[pipeUsed % 2][1]);
  }
}
// Run a list of commands
void run_script(CommandHolder* holders) {
  if (holders == NULL)
    return;
  static bool newScript = true;
  if (newScript){
    bg_jobs = new_destructable_BG_job(1, _destroy_job);
    newScript = false;
  }

  job_t curr_job = _new_job();

  check_jobs_bg_status();

  if (get_command_holder_type(holders[0]) == EXIT &&
      get_command_holder_type(holders[1]) == EOC) {
    end_main_loop();
    return;
  }

  CommandType type;
  //create a new job to hold the current job, call the constructor
  //job_t curr_job = pop_front_BG_job(&bg_jobs);

  // Run all commands in the `holder` array
  for (int i = 0; (type = get_command_holder_type(holders[i])) != EOC; ++i){
    create_process(holders[i], &curr_job, i);
  }

  if (!(holders[0].flags & BACKGROUND)) {
    // Not a background Job
    // TODO: Wait for all processes under the job to complete
    //need another wait (waitpid) to wait the perfect amount of time
    while (!is_empty_piddeque(&curr_job.process_list)){
      pid_t curr_process = pop_front_piddeque(&curr_job.process_list);
      int status = 0;
      waitpid(curr_process, &status, 0);

    }
      _destroy_job(curr_job);

  }
  else {
    // A background job.
    if(is_empty_BG_job(&bg_jobs)) curr_job.job_id = 1;
    else curr_job.job_id = peek_back_BG_job(&bg_jobs).job_id + 1;

    //curr_job.cmd = get_command_string();

    // TODO: Push the new job to the job queue
    //figure out new job ID and push it to the back of the job queue and print that the background job has started
    //add the background job to the end of the q
    push_back_BG_job(&bg_jobs, curr_job);


    // TODO: Once jobs are implemented, uncomment and fill the following line
    //print the first element in the pid q (which what peek returns)
    print_job_bg_start(curr_job.job_id,peek_front_piddeque(&curr_job.process_list), curr_job.cmd);
  }
  //_destroy_job(curr_job);
}
