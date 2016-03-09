# Tiny-Shell
The purpose of this project is to become more familiar with the concepts of process control and signaling. You’ll do this by writing a simple Unix shell program that supports job control.   
## General Overview of Unix Shells 
A shell is an interactive command-line interpreter that runs programs on behalf of the user. A shell repeatedly prints a prompt, waits for a command line on stdin, and then carries out some action, as directed by
 the contents of the command line.   
 
The command line is a sequence of ASCII text words delimited by whitespace. The first 
word in the command line is either the name 
of a built-in command or the pathname of an 
executable file. The remaining words are command-line arguments.  If 
the first word is a 
built-in command, the shell immediately 
executes the command in the current 
process. Otherwise, the word is assumed to be the pathname of an executable 
program. In this case, the shell forks a child 
process, then loads and runs the program in 
the context of the child. The child processes created as a result of interpreting a single 
command line are known collectively as a job. In general, a job can consist of multiple 
child processes connected by Unix pipes. If the command line ends with an ampersand &, then the job runs in the 
background, which means that the shell does not wait for 
the job to terminate before printing the 
prompt and awaiting the next command line.
 Otherwise, the job runs in the 
foreground, 
which means that the shell waits for the job 
to terminate before prompting for the next 
command line. Thus, at any point in time, 
at most one job can be running in the 
foreground. However, an arbitrary number of jobs can run in the background.  

For example, typing the command line  
    ```tsh> jobs```
    
causes the shell to execute the built-in jobs command. 

Typing the command line ```tsh> /bin/ls -l -d```

runs the 
`ls`
 program in the foreground.  By convention, the shell ensures that when 
`ls`
begins executing its main routine:  
           `int main(int argc, char *argv[])` 
           
the 
`argc`
 and 
`argv`
 arguments have the 
following values:  
* argc == 3
* argv[0] == "/bin/ls"
* argv[1] == "-l"
* argv[2] == "-d"

Alternatively, typing the command line  
    `tsh> /bin/ls -l -d &` 
runs the 
`ls`
 program in the background.
 
 Unix shells support the notion of 
job control
, which allows users to move jobs back and 
forth between background and foreground, and to change the process state (running, 
stopped, or terminated) of the 
processes in a job. Typing 
ctrl-c
 causes a SIGINT signal to 
be delivered to each process in the foreground job. The default action for SIGINT is to 
terminate the process. Similarly, typing 
ctrl-z
 causes a SIGTSTP signal to be delivered to 
each process in the foreground job. The default action for SIGTSTP is to place a process 
in the stopped state, where it 
remains until it is awakened 
by the receipt of a SIGCONT 
signal. Unix shells also provide various built-in commands that support job control. For 
example:

* jobs: List the running and stopped background jobs.   
* bg <job>: Change a stopped background job to a running background job.   
* fg <job>: Change a stopped or running background job to a running foreground job.   
* kill <job>: Terminate a job.
