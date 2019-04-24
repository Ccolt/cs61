#include "sh61.hh"
#include <cstring>
#include <cerrno>
#include <vector>
#include <sys/stat.h>
#include <sys/wait.h>

volatile sig_atomic_t interrupt = 0;


// struct command
//    Data structure describing a command. Also contains the implementation
//    of pipelines

struct command {
    std::vector<std::string> args;
    pid_t pid;      // process ID running this command, -1 if none
    pid_t pgid;     // process group ID of this command, -1 if none
    command* next;  // next command pointer
    int read_end;
    int write_end;
    bool should_pipe;
    bool pipe_start;
    std::string redir_type;
    std::string infile;
    std::string outfile;
    std::string errfile;

    command();
    ~command();

    int run();
    pid_t make_child(pid_t pgid);
};


// struct conditional
//    Data structure describing a conditional

struct conditional {
    conditional* next;  // pointer to next conditional
    command* cmd;    // pointer to pipeline
    bool is_and;        // dermines type of conditional (and/or)
    int last_status;    // stores status of last executed command
    bool piped;         // stores whether conditional already contains a pipe

    conditional();      // Initialization
    ~conditional();     // Destruction
    
    void run();
};


// struct task
//   Data structure describing a fore or background task

struct task {
    task* next;        // pointer to next task
    conditional* cond; // pointer to child conditional
    bool is_background; // determines fore or background process

    task();            // Initialization
    ~task();           // Destruction
    
    void run();
//TODO remove this: cp f%%a.txt f%%b.txt &
};


// command::command()
//    This constructor function initializes a `command` structure. You may
//    add stuff to it as you grow the command structure.

command::command() {
    pid = -1;
    pgid = -1;
    next = nullptr;
    read_end = 0;
    write_end = 1;
    should_pipe = false;
    redir_type = "NONE";
    infile = "";
    outfile = "";
    errfile = "";
}


// command::~command()
//    This destructor function is called to delete a command.

command::~command() {
}


// conditional::conditional()
//    This constructor function initializes a `conditional` structure

conditional::conditional() {
    next = nullptr;
    cmd = nullptr;
    is_and = true;
    last_status = 0;
    piped = false;
}


// conditional::conditional()
//    This destructor function is called to delete a conditional.

conditional::~conditional() {
}

// task::task()
//    This constructor function initializes a `task` structure

task::task() {
    next = nullptr;
    cond = nullptr;
    is_background = false;
}


// task::task()
//    This destructor function is called to delete a task.

task::~task() {
}

// COMMAND EVALUATION

// command::make_child(pgid)
//    Create a single child process running `this` command. Sets `this->pid`
//    to the pid of the child process and returns `this->pid`.
//
//    PART 1: Fork a child process and run the command using `execvp`.
//       This will require creating an array of `char*` arguments using
//       `args[N].c_str()`.
//    PART 5: Set up a pipeline if appropriate. This may require creating a
//       new pipe (`pipe` system call), and/or replacing the child process's
//       standard input/output with parts of the pipe (`dup2` and `close`).
//       Draw pictures!
//    PART 7: Handle redirections.
//    PART 8: The child process should be in the process group `pgid`, or
//       its own process group (if `pgid == 0`). To avoid race conditions,
//       this will require TWO calls to `setpgid`.

pid_t command::make_child(pid_t pgid) {
    (void) pgid;
    int pipefd[2];
    
    // Initialize pipe if necessary
    if (should_pipe) {
        int r = pipe(pipefd);
        if (r == -1) {
            fprintf(stderr, "Error: pipe() in make_child failed to execute\n");
            _exit(1);
        }
        write_end = pipefd[1];
        if (next) {
            next->read_end = pipefd[0];
            next->pgid = pgid;
        }
    }

    // Deal with cd if necessary
    if (args[0] == "cd") {
        int r = chdir(args[1].c_str());
        if (r != 0) {
            chdir("/");
        }
        return (0);
    }

    // Fork
    pid = fork();
    if (pid == -1) {
        fprintf(stderr, "Error: fork() in make_child failed to execute\n");
        _exit(1);
    }
    
    // If Child:
    else if (pid == 0) {
        if (pipe_start) {
            pgid = getpid();
            next->pgid = pgid;
        }
        setpgid(getpid(), pgid);
        dup2(read_end, 0);
        dup2(write_end, 1);
        if (read_end != 0) {close(read_end);}
        if (write_end != 1) {close(write_end);}
        if (should_pipe) {close(pipefd[0]);}

        // Handle redirections
        if (redir_type == "<") {
            int fid = open(infile.c_str(), O_RDONLY|O_CLOEXEC);
            if (fid == -1) {
                fprintf(stderr,"No such file or directory\n");
                _exit(1);
            }
            dup2(fid, 0);
        }
        if (redir_type == ">") {
            int fid = open(outfile.c_str(), O_CREAT|O_WRONLY|O_TRUNC, S_IRWXU);
            if (fid == -1) {
                fprintf(stderr,"No such file or directory\n");
                _exit(1);
            }
            dup2(fid, 1);
        }
        if (redir_type == "2>") {
            int fid = open(errfile.c_str(), O_CREAT|O_WRONLY|O_TRUNC, S_IRWXU);
            if (fid == -1) {
                fprintf(stderr,"No such file or directory\n");
                _exit(1);
            }
            dup2(fid, 2);
        }

        // Exexvp
        int argc = sizeof(this->args);
        char* cargs[argc];
        for (int N=0; N < argc; ++N) {
            cargs[N] = (char*) args[N].c_str();
        }
        int r = execvp(cargs[0], cargs);
        if (r == -1) {
            fprintf(stderr, "Error: execvp() in make_child failed to execute\n");
            _exit(1);
        }
        _exit(0);

    }
    // If Parent
    else {
        if (read_end != 0) {close(read_end);}
        if (write_end != 1) {close(write_end);}
        return(pid);
    }
}


// command::run()
//    Run the command list starting at `this`.
//
//    PART 1: Start the single command `this` with `command::start`,
//        and wait for it to finish using `waitpid`.
//    The remaining parts may require that you change `struct command`
//    (e.g., to track whether a command is in the background)
//    and write code in `run` (or in helper functions!).
//    PART 2: Treat background commands differently.
//    PART 3: Introduce a loop to run all commands in the list.
//    PART 4: Change the loop to handle conditionals.
//    PART 5: Change the loop to handle pipelines. Start all processes in
//       the pipeline in parallel. The status of a pipeline is the status of
//       its LAST command.
//    PART 8: - Choose a process group for each pipeline.
//       - Call `claim_foreground(pgid)` before waiting for the pipeline.
//       - Call `claim_foreground(0)` once the pipeline is complete.

int command::run() {
    int wstatus;
    if (!args.empty()) {
        pid = make_child(0);
        if (!should_pipe && args[0] != "cd") {
            pid_t exited_pid = waitpid(pid, &wstatus, 0);
            assert(exited_pid == pid);

            if (!WIFEXITED(wstatus)) {
                fprintf(stderr, "Child exited abnormally [%x]\n", wstatus);
            }
        }
    }
    if (next) {return(next->run());} 
    return (WEXITSTATUS(wstatus));
}


// conditional::run()
//    Run a conditional

void conditional::run() {
    if (cmd && (is_and ^ (last_status != 0))) {
        int status = cmd->run();
        if (next) {
            next->last_status = status;
        }
    }
    else if (next) {
        next->last_status = last_status;
    }
    if (next) {next->run();}
}


// task::run()
//    Run a task 

void task::run() {
    if (cond) {
        if (is_background) {
            pid_t p = fork();
            if (p == 0) {
                cond->run();
                _exit(0);
            }
        }
        else cond->run();
    }
    if (next) {next->run();} 
}


// clean(tsk)
//     Clear the task, conditional, and command lists from memory

void clean(task* tsk) {
    while (tsk->cond) {
        while (tsk->cond->cmd) {
            command* next_cmd = tsk->cond->cmd->next;
            delete tsk->cond->cmd;
            tsk->cond->cmd = next_cmd;
        }
        conditional* next_cond = tsk->cond->next;
        delete tsk->cond;
        tsk->cond = next_cond;
    }
    if (tsk->next) {
        clean(tsk->next);
    }
    delete tsk;
}


// eval_line(c)
//    Parse the command list in `s` and run it via `command::run`.

void eval_line(const char* s) {
    int type;
    std::string token;

    // initialize the command tree
    task* tsk_head = new task;
    task* tsk = tsk_head;
    tsk->cond = new conditional;
    conditional* cond_head = tsk->cond;
    tsk->cond->cmd = new command;
    command* cmd_head = tsk->cond->cmd;

    while ((s = parse_shell_token(s, &type, &token)) != nullptr) {
        if (type == TOKEN_NORMAL) {
            // Add token to current command args
            if (!tsk->cond) {tsk->cond = new conditional;}
            if (!tsk->cond->cmd) {tsk->cond->cmd = new command;}
            tsk->cond->cmd->args.push_back(token);
        }
        else if (type == TOKEN_SEQUENCE && token != "") {
            // Create new command for next token
            tsk->cond->cmd->next = new command;
            tsk->cond->cmd = tsk->cond->cmd->next;
        }
        else if (type == TOKEN_BACKGROUND) {
            // Update current task to background
            // Create new task for next token
            tsk->is_background = true;
            tsk->next = new task;
            tsk = tsk->next;
        }
        else if (type == TOKEN_AND) {
            // Update current conditional
            // Create new conditional for next token
            tsk->cond->next = new conditional;
            tsk->cond->next->is_and = true;
            tsk->cond = tsk->cond->next;

        }
        else if (type == TOKEN_OR) {
            // Update current conditional
            // Create new conditional for next token
            tsk->cond->next = new conditional;
            tsk->cond->next->is_and = false;
            tsk->cond = tsk->cond->next;
        }
        else if (type == TOKEN_PIPE) {
            // Create new pipeline
            if (!tsk->cond->piped) {
                tsk->cond->cmd->pipe_start = true;
            }
            tsk->cond->piped = true;
            tsk->cond->cmd->should_pipe = true;
            tsk->cond->cmd->next = new command;
            tsk->cond->cmd = tsk->cond->cmd->next;
        }
        else if (type == TOKEN_REDIRECTION) {
            // Setup for redirection
            tsk->cond->cmd->redir_type = token;
            std::string redir_type = token;
            s = parse_shell_token(s, &type, &token);
            if (redir_type == "<") {
                tsk->cond->cmd->infile = token;
            }
            if (redir_type == ">") {
                tsk->cond->cmd->outfile = token;
            }
            if (redir_type == "2>") {
                tsk->cond->cmd->errfile = token;
            }
        }
    }
    // execute it
    tsk = tsk_head;
    tsk->cond = cond_head;
    tsk->cond->cmd = cmd_head;
    tsk->run();
    clean(tsk_head);
}             

// int_handler()
//    Handles interrupts

void int_handler(int i) {
    interrupt = 1;
}

int main(int argc, char* argv[]) {
    FILE* command_file = stdin;
    bool quiet = false;

    // Check for '-q' option: be quiet (print no prompts)
    if (argc > 1 && strcmp(argv[1], "-q") == 0) {
        quiet = true;
        --argc, ++argv;
    }

    // Check for filename option: read commands from file
    if (argc > 1) {
        command_file = fopen(argv[1], "rb");
        if (!command_file) {
            perror(argv[1]);
            exit(1);
        }
    }

    // - Put the shell into the foreground
    // - Ignore the SIGTTOU signal, which is sent when the shell is put back
    //   into the foreground
    claim_foreground(0);
    set_signal_handler(SIGTTOU, SIG_IGN);
    set_signal_handler(SIGINT, int_handler);

    char buf[BUFSIZ];
    int bufpos = 0;
    bool needprompt = true;

    while (!feof(command_file)) {
        // Print the prompt at the beginning of the line
        if (needprompt && !quiet) {
            printf("sh61[%d]$ ", getpid());
            fflush(stdout);
            needprompt = false;
        }

        // Read a string, checking for error or EOF
        if (fgets(&buf[bufpos], BUFSIZ - bufpos, command_file) == nullptr) {
            if (ferror(command_file) && errno == EINTR) {
                // ignore EINTR errors
                clearerr(command_file);
                buf[bufpos] = 0;
            } else {
                if (ferror(command_file)) {
                    perror("sh61");
                }
                break;
            }
        }

        // If a complete command line has been provided, run it
        bufpos = strlen(buf);
        if (bufpos == BUFSIZ - 1 || (bufpos > 0 && buf[bufpos - 1] == '\n')) {
            eval_line(buf);
            bufpos = 0;
            needprompt = 1;
        }

        // Handle zombie processes and/or interrupt requests
        // Your code here!
        int wstatus;
        while(waitpid(-1, &wstatus, WNOHANG) > 0) {}
        
        // Kill process if receive keyboard interrupt
        if (interrupt == 1) {
            kill(getpid(), SIGINT);
            needprompt = true;
            interrupt = 0;
            continue;
        }
    }

    return 0;
}
