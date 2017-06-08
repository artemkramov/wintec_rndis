#include <stdio.h>        //printf
#include <stdlib.h>       //malloc
#include <unistd.h>       //execl, _exit
#include <string.h>       // strncpy
#include <sys/wait.h>     // sigaction
#include <errno.h>        // errno

/**
* Static variable for custom popen realization
*/
static int *pids;

/**
* Redefine popen function
* due to Android features
*/ 
FILE *
   popenCall(program, type)
       char *program;
       register char *type;
   {
       register FILE *iop;
       int pdes[2], fds, pid;
   
       if (*type != 'r' && *type != 'w' || type[1])
           return (NULL);
   
       if (pids == NULL) {
           if ((fds = getdtablesize()) <= 0)
               return (NULL);
           if ((pids = (int *)malloc((u_int)(fds * sizeof(int)))) == NULL)
               return (NULL);
           bzero((char *)pids, fds * sizeof(int));
       }
       if (pipe(pdes) < 0)
           return (NULL);
       switch (pid = vfork()) {
       case -1:            /* error */
           (void) close(pdes[0]);
           (void) close(pdes[1]);
           return (NULL);
           /* NOTREACHED */
       case 0:             /* child */
           if (*type == 'r') {
               if (pdes[1] != fileno(stdout)) {
                   (void) dup2(pdes[1], fileno(stdout));
                   (void) close(pdes[1]);
               }
               (void) close(pdes[0]);
           } else {
               if (pdes[0] != fileno(stdin)) {
                   (void) dup2(pdes[0], fileno(stdin));
                   (void) close(pdes[0]);
               }
               (void) close(pdes[1]);
           }
           execl("/system/bin/sh", "sh", "-c", program, NULL);
           _exit(127);
           /* NOTREACHED */
       }
       /* parent; assume fdopen can't fail...  */
       if (*type == 'r') {
           iop = fdopen(pdes[0], type);
           (void) close(pdes[1]);
       } else {
           iop = fdopen(pdes[1], type);
           (void) close(pdes[0]);
      }
      pids[fileno(iop)] = pid;
      return (iop);
  }
  
  /**
* Redefine the default system function
* due to Android features
*/
int
systemCall(const char *command)
{
    sigset_t blockMask, origMask;
    struct sigaction saIgnore, saOrigQuit, saOrigInt, saDefault;
    pid_t childPid;
    int status, savedErrno;

    if (command == NULL)                /* Is a shell available? */
        return system(":") == 0;

    /* The parent process (the caller of system()) blocks SIGCHLD
       and ignore SIGINT and SIGQUIT while the child is executing.
       We must change the signal settings prior to forking, to avoid
       possible race conditions. This means that we must undo the
       effects of the following in the child after fork(). */

    sigemptyset(&blockMask);            /* Block SIGCHLD */
    sigaddset(&blockMask, SIGCHLD);
    sigprocmask(SIG_BLOCK, &blockMask, &origMask);

    saIgnore.sa_handler = SIG_IGN;      /* Ignore SIGINT and SIGQUIT */
    saIgnore.sa_flags = 0;
    sigemptyset(&saIgnore.sa_mask);
    sigaction(SIGINT, &saIgnore, &saOrigInt);
    sigaction(SIGQUIT, &saIgnore, &saOrigQuit);

    switch (childPid = fork()) {
    case -1: /* fork() failed */
        status = -1;
        break;          /* Carry on to reset signal attributes */

    case 0: /* Child: exec command */

        /* We ignore possible error returns because the only specified error
           is for a failed exec(), and because errors in these calls can't
           affect the caller of system() (which is a separate process) */

        saDefault.sa_handler = SIG_DFL;
        saDefault.sa_flags = 0;
        sigemptyset(&saDefault.sa_mask);

        if (saOrigInt.sa_handler != SIG_IGN)
            sigaction(SIGINT, &saDefault, NULL);
        if (saOrigQuit.sa_handler != SIG_IGN)
            sigaction(SIGQUIT, &saDefault, NULL);

        sigprocmask(SIG_SETMASK, &origMask, NULL);
		//In Android we have to call /system/bin/sh instead of /bin/sh
        execl("/system/bin/sh", "sh", "-c", command, (char *) NULL);
        _exit(127);                     /* We could not exec the shell */

    default: /* Parent: wait for our child to terminate */

        /* We must use waitpid() for this task; using wait() could inadvertently
           collect the status of one of the caller's other children */

        while (waitpid(childPid, &status, 0) == -1) {
            if (errno != EINTR) {       /* Error other than EINTR */
                status = -1;
                break;                  /* So exit loop */
            }
        }
        break;
    }

    /* Unblock SIGCHLD, restore dispositions of SIGINT and SIGQUIT */

    savedErrno = errno;                 /* The following may change 'errno' */

    sigprocmask(SIG_SETMASK, &origMask, NULL);
    sigaction(SIGINT, &saOrigInt, NULL);
    sigaction(SIGQUIT, &saOrigQuit, NULL);

    errno = savedErrno;

    return status;
}