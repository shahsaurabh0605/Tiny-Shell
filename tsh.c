/* 
 * tsh - A tiny shell program with job control
 * 
 * Saurabh Shah 201401245
 * 201401245@daiict.ac.in
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

/* Misc manifest constants */
#define MAXLINE    1024   /* max line size */
#define MAXARGS     128   /* max args on a command line */
#define MAXJOBS      16   /* max jobs at any point in time */
#define MAXJID    1<<16   /* max job ID */

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
extern char **environ;      /* defined in libc */
char prompt[] = "tsh> ";    /* command line prompt (DO NOT CHANGE) */
int verbose = 0;            /* if true, print additional output */
int nextjid = 1;            /* next job ID to allocate */
char sbuf[MAXLINE];         /* for composing sprintf messages */

struct job_t {              /* The job struct */
    pid_t pid;              /* job PID */
    int jid;                /* job ID [1, 2, ...] */
    int state;              /* UNDEF, BG, FG, or ST */
    char cmdline[MAXLINE];  /* command line */
};
struct job_t jobs[MAXJOBS]; /* The job list */
/* End global variables */


/* Function prototypes */

/* Here are the functions that you will implement */
void eval(char *cmdline);
int builtin_cmd(char **argv);
void do_bgfg(char **argv);
void waitfg(pid_t pid);

void sigchld_handler(int sig);
void sigtstp_handler(int sig);
void sigint_handler(int sig);
int Sigprocmask(int action, sigset_t* set, void*);
int Sigaddset(sigset_t *set, int signal);
int Sigemptyset(sigset_t* set);
/* Here are helper routines that we've provided for you */
int parseline(const char *cmdline, char **argv); 
void sigquit_handler(int sig);

void clearjob(struct job_t *job);
void initjobs(struct job_t *jobs);
int maxjid(struct job_t *jobs); 
int addjob(struct job_t *jobs, pid_t pid, int state, char *cmdline);
int deletejob(struct job_t *jobs, pid_t pid); 
pid_t fgpid(struct job_t *jobs);
struct job_t *getjobpid(struct job_t *jobs, pid_t pid);
struct job_t *getjobjid(struct job_t *jobs, int jid); 
int pid2jid(pid_t pid); 
void listjobs(struct job_t *jobs);

void usage(void);
void unix_error(char *msg);
void app_error(char *msg);
typedef void handler_t(int);
handler_t *Signal(int signum, handler_t *handler);

/*
 * main - The shell's main routine 
 */
int main(int argc, char **argv) 
{
    char c;
    char cmdline[MAXLINE];
    int emit_prompt = 1; /* emit prompt (default) */

    /* Redirect stderr to stdout (so that driver will get all output
     * on the pipe connected to stdout) */
    dup2(1, 2);

    /* Parse the command line */
    while ((c = getopt(argc, argv, "hvp")) != EOF) {
        switch (c) {
        case 'h':             /* print help message */
            usage();
	    break;
        case 'v':             /* emit additional diagnostic info */
            verbose = 1;
	    break;
        case 'p':             /* don't print a prompt */
            emit_prompt = 0;  /* handy for automatic testing */
	    break;
	default:
            usage();
	}
    }

    /* Install the signal handlers */

    /* These are the ones you will need to implement */
    Signal(SIGINT,  sigint_handler);   /* ctrl-c */
    Signal(SIGTSTP, sigtstp_handler);  /* ctrl-z */
    Signal(SIGCHLD, sigchld_handler);  /* Terminated or stopped child */

    /* This one provides a clean way to kill the shell */
    Signal(SIGQUIT, sigquit_handler); 

    /* Initialize the job list */
    initjobs(jobs);

    /* Execute the shell's read/eval loop */
    while (1) {

	/* Read command line */
	if (emit_prompt) {
	    printf("%s", prompt);
	    fflush(stdout);
	}
	if ((fgets(cmdline, MAXLINE, stdin) == NULL) && ferror(stdin))
	    app_error("fgets error");
	if (feof(stdin)) { /* End of file (ctrl-d) */
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
 * If the user has requested a built-in command (quit, jobs, bg or fg)
 * then execute it immediately. Otherwise, fork a child process and
 * run the job in the context of the child. If the job is running in
 * the foreground, wait for it to terminate and then return.  Note:
 * each child process must have a unique process group ID so that our
 * background children don't receive SIGINT (SIGTSTP) from the kernel
 * when we type ctrl-c (ctrl-z) at the keyboard.  
*/
void eval(char *cmdline) 
{
	/* Here call the parseline function which will return the argv array*/
	/* After parseline has returned we will call builtin_cmd to execute our commands*/
	char *temp[MAXARGS];
	int pid;
        struct job_t *jbid;
	int parse= parseline(cmdline,temp);
	if(!strcmp(cmdline,"\n"))
		return;
	int ret= builtin_cmd(temp);
	if(ret==1)
		return;
	sigset_t mask;
	Sigemptyset(&mask);	   /* initialising signal set mask */
	Sigaddset(&mask, SIGINT);  /* adding SIGINT signal to the set mask */
	Sigaddset(&mask, SIGTSTP); /* adding SIGSTOP signal to the set mask */
	Sigaddset(&mask, SIGCHLD); /* adding SIGCHLD signal to the set mask */
	Sigprocmask(SIG_BLOCK, &mask, NULL);     /* blocking the set so that the parent does not receive SIGCHLD SIGINT or SIGSTOP signal before creating this job process is initiated in child process */

	if((pid=fork())==0){                       /* creating child process for non-builtin command */
	    Sigprocmask(SIG_UNBLOCK, &mask, NULL); /* unblocking for child process */
            setpgid(0,0);                          /* setting the group id of command that is being executed */
	    if(execvp(*temp,temp)<0){              /* execing the non-builltin command */
	        printf("%s: Command not found\n",temp[0]);
	        exit(0);
	    }
	}
	else{
	    if(!parse){
	    	addjob(jobs, pid, FG, cmdline);         /* adding foreground job */       
            	Sigprocmask(SIG_UNBLOCK, &mask,NULL);   /* unblocking parent process after adding job as now the child is added in jobs table and the parent is ready to receive signals */
	    	waitfg(pid);                            /* waiting for sigchld signal or any other signal in waitfg as it is a foreground process */
            }
	    else{
	    	addjob(jobs, pid, BG, cmdline);         /* adding background job */                             
            	Sigprocmask(SIG_UNBLOCK, &mask, NULL);  /* unblocking for parent process after adding the job as now the child is added in jobs table and the parent is ready to receive signals */                         
            	jbid = getjobpid(jobs, pid);            /* getting the job from the process id */                              
            	printf("[%d] (%d) %s", jbid->jid, jbid->pid, jbid->cmdline);
	    }
	}
    return;
}

/* 
 * parseline - Parse the command line and build the argv array.
 * 
 * Characters enclosed in single quotes are treated as a single
 * argument.  Return true if the user has requested a BG job, false if
 * the user has requested a FG job.  
 */
int parseline(const char *cmdline, char **argv) 
{
    static char array[MAXLINE]; /* holds local copy of command line */
    char *buf = array;          /* ptr that traverses command line */
    char *delim;                /* points to first space delimiter */
    int argc;                   /* number of args */
    int bg;                     /* background job? */

    strcpy(buf, cmdline);
    buf[strlen(buf)-1] = ' ';  /* replace trailing '\n' with space */
    while (*buf && (*buf == ' ')) /* ignore leading spaces */
	buf++;

    /* Build the argv list */
    argc = 0;
    if (*buf == '\'') {
	buf++;
	delim = strchr(buf, '\'');
    }
    else {
	delim = strchr(buf, ' ');
    }

    while (delim) {
	argv[argc++] = buf;
	*delim = '\0';
	buf = delim + 1;
	while (*buf && (*buf == ' ')) /* ignore spaces */
	       buf++;

	if (*buf == '\'') {
	    buf++;
	    delim = strchr(buf, '\'');
	}
	else {
	    delim = strchr(buf, ' ');
	}
    }
    argv[argc] = NULL;
    
    if (argc == 0)  /* ignore blank line */
	return 1;

    /* should the job run in the background? */
    if ((bg = (*argv[argc-1] == '&')) != 0) {
	argv[--argc] = NULL;
    }
    return bg;
}

/* 
 * builtin_cmd - If the user has typed a built-in command then execute
 *    it immediately.  
 */
int builtin_cmd(char **argv) 
{
	int i;
	if(!strcmp(argv[0],"quit")){
	for (i=0;i< MAXJOBS;i++){                                            
		if(jobs[i].state==ST){           /* checking for stopped jobs and printing them */
			printf("[%d] (%d) Stopped", jobs[i].jid, jobs[i].pid);
			return 1;	/* return 1 for a builtin command*/
		}
	}
	exit(0);                  
	}
	if(!strcmp(argv[0],"jobs")){
		listjobs(jobs);
		return 1;               /* returns 1 for a builtin command*/
	}
	if(!strcmp(argv[0],"bg") || !strcmp(argv[0],"fg")){
		do_bgfg(argv);
		return 1;              /* returns 1 for a builtin command */
	}
	else 	
    		return 0;     /* not a builtin command */
}

/* 
 * do_bgfg - Execute the builtin bg and fg commands
 */
void do_bgfg(char **argv) 
{
    struct job_t *jb= NULL;
	if(argv[1]==NULL){      /* Checks if there is anything after the fg or bg command */
		printf("%s command requires PID or [%]jobid argument\n",argv[0]);   /* If nothing is there then prints accordingly */
		return;
	}

	if(argv[1][0]=='%'){    /* getting the jid if second argument begins with % */
		argv[1][0]= '0';
		if(!atoi(argv[1])){     /* the input after % must be a number here */
			printf("%s: argument must be a PID or [%]jobid\n",argv[0]);
			return;
		}
		else{           /* if the input is an integer */
		    int jid= atoi(argv[1]);
		    jb= getjobjid(jobs,jid);    /* gets the job from jid */
            		if(jb==NULL){           /* checks if no job is found */
               			argv[1][0]= '%';
                		printf("%s %s: No such job\n",argv[0],argv[1]);
				return;
            		}		    
		}
	}

	else if(!atoi(argv[1])){
		printf("%s: argument must be a PID or [%]jobid\n",argv[0]);
		return;
	}
    	else{
        	int pid= atoi(argv[1]);  /* checks if the argument after fg or bg is a process id */
        	jb= getjobpid(jobs,pid); /* get the job from pid */
        	if(jb==NULL){                                     /* check if no job from the given process id is found */
            		printf("(%s): No such process\n",argv[1]); 
			return;
        	}
        }
  	 if(strcmp(argv[0],"bg")==0){
		jb->state=BG;                              /* set the state to background jobs */
		kill(-jb->pid,SIGCONT);                    /* continuing the stopped execution */
		printf("[%d] (%d) %s",jb->jid,jb->pid,jb->cmdline);
	}
	if(strcmp(argv[0],"fg")==0){
		jb->state=FG;                             /* set the state to foreground jobs */
		kill(-(jb->pid),SIGCONT);                 /* continuing the stopped execution */
		waitfg(jb->pid);                          /* waiting for foreground jobs to complete */
	}
	return;
}

/* 
 * waitfg - Block until process pid is no longer the foreground process
 */
void waitfg(pid_t pid)
{
    struct job_t *jb = getjobpid(jobs,pid);    /* getting job from the pid */
    if(jb==NULL){
   	return;
    }

    while(jb->pid==pid && jb->state==FG){     /* checking if the job process is still a foreground process */
        sleep(1);
    }
    
    return;
}

/*****************
 * Signal handlers
 *****************/

/* 
 * sigchld_handler - The kernel sends a SIGCHLD to the shell whenever
 *     a child job terminates (becomes a zombie), or stops because it
 *     received a SIGSTOP or SIGTSTP signal. The handler reaps all
 *     available zombie children, but doesn't wait for any other
 *     currently running children to terminate.  
 */
void sigchld_handler(int sig) 
{
    int status;
    pid_t cpid= fgpid(jobs);
    struct job_t *jb;
    while((cpid= waitpid(cpid,&status,WNOHANG|WUNTRACED))>0){  /*  for cases where more than one child terminates */
        jb= getjobpid(jobs,cpid);
        if(WIFEXITED(status)){                                 /* If child terminated normally  */
            deletejob(jobs,cpid);                              /* delete jobs */
        }
        if(WIFSIGNALED(status)){                              /* If child terminated due to sigint  */
	    printf("Job [%d] (%d) terminated by signal %d\n",jb->jid, jb->pid, WTERMSIG(status));
	    //psignal(WTERMSIG(status),"terminated by signal");
            deletejob(jobs,cpid);                           /* delete jobs */
        }
        if(WIFSTOPPED(status)){                             /* If child terminated due to sigtstp */ 
            printf("Job [%d] (%d) stopped by signal %d\n",jb->jid,jb->pid, WSTOPSIG(status));
            jb->state= ST;                                 /* change the state accordingly */
        }
    }
    return;
}

/* 
 * sigint_handler - The kernel sends a SIGINT to the shell whenver the
 *    user types ctrl-c at the keyboard.  Catch it and send it along
 *    to the foreground job.  
 */
void sigint_handler(int sig) 
{
    pid_t pid=fgpid(jobs);   /* get the pid of foreground jobs */
	if(pid>0){
		kill(-pid,SIGINT);  /* apply SIGINT signal */
	}
    return;
}

/*
 * sigtstp_handler - The kernel sends a SIGTSTP to the shell whenever
 *     the user types ctrl-z at the keyboard. Catch it and suspend the
 *     foreground job by sending it a SIGTSTP.  
 */
void sigtstp_handler(int sig) 
{
    pid_t pid=fgpid(jobs); /* get the pid of foreground jobs */
	if(pid>0){
		kill(-pid,SIGTSTP);  /* apply SIGTSTP signal */
	}
	return;
}

/*********************
 * End signal handlers
 *********************/

/***********************************************
 * Helper routines that manipulate the job list
 **********************************************/

/* clearjob - Clear the entries in a job struct */
void clearjob(struct job_t *job) {
    job->pid = 0;
    job->jid = 0;
    job->state = UNDEF;
    job->cmdline[0] = '\0';
}

/* initjobs - Initialize the job list */
void initjobs(struct job_t *jobs) {
    int i;

    for (i = 0; i < MAXJOBS; i++)
	clearjob(&jobs[i]);
}

/* maxjid - Returns largest allocated job ID */
int maxjid(struct job_t *jobs) 
{
    int i, max=0;

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

    for (i = 0; i < MAXJOBS; i++) {
	if (jobs[i].pid == 0) {
	    jobs[i].pid = pid;
	    jobs[i].state = state;
	    jobs[i].jid = nextjid++;
	    if (nextjid > MAXJOBS)
		nextjid = 1;
	    strcpy(jobs[i].cmdline, cmdline);
  	    if(verbose){
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

    for (i = 0; i < MAXJOBS; i++) {
	if (jobs[i].pid == pid) {
	    clearjob(&jobs[i]);
	    nextjid = maxjid(jobs)+1;
	    return 1;
	}
    }
    return 0;
}

/* fgpid - Return PID of current foreground job, 0 if no such job */
pid_t fgpid(struct job_t *jobs) {
    int i;

    for (i = 0; i < MAXJOBS; i++)
	if (jobs[i].state == FG)
	    return jobs[i].pid;
    return 0;
}

/* getjobpid  - Find a job (by PID) on the job list */
struct job_t *getjobpid(struct job_t *jobs, pid_t pid) {
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
	if (jobs[i].pid == pid) {
            return jobs[i].jid;
        }
    return 0;
}

/* listjobs - Print the job list */
void listjobs(struct job_t *jobs) 
{
    int i;
    
    for (i = 0; i < MAXJOBS; i++) {
	if (jobs[i].pid != 0) {
	    printf("[%d] (%d) ", jobs[i].jid, jobs[i].pid);
	    switch (jobs[i].state) {
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
		    printf("listjobs: Internal error: job[%d].state=%d ", 
			   i, jobs[i].state);
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
    printf("Terminating after receipt of SIGQUIT signal\n");
    exit(1);
}

/* Implementation of the safer functions */

int Sigprocmask(int action, sigset_t* Sigset, void* t){  /* Safe Sigprocmask function */
    int stat;                                                                           

    if((stat = sigprocmask(action, Sigset, NULL))){                                         
        unix_error("Fatal: Sigprocmask Error!");                                           
    }

    return stat;
}
int Sigemptyset(sigset_t* Sigset){   /* Safe Sigemptyset function */
    int stat;                                                                             
    if((stat = sigemptyset(Sigset))){                                                      
        unix_error("Fatal: Sigemptyset Error!");                                            
    }
    return stat;
}
int Sigaddset(sigset_t *Sigset, int signal){   /* Safe Sigaddset function */
    int stat;                                                                            
    if((stat = sigaddset(Sigset, signal))){                                               
        unix_error("Fatal: Sigaddset Error!");                                              
    }
    return stat;
}

