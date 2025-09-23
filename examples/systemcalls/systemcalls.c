#include "systemcalls.h"
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>

/**
 * @param cmd the command to execute with system()
 * @return true if the command in @param cmd was executed
 *   successfully using the system() call, false if an error occurred,
 *   either in invocation of the system() call, or if a non-zero return
 *   value was returned by the command issued in @param cmd.
*/
bool do_system(const char *cmd)
{

/*
 * TODO  add your code here
 *  Call the system() function with the command set in the cmd
 *   and return a boolean true if the system() call completed with success
 *   or false() if it returned a failure
*/

    return system(cmd) == 0 ? true: false;
}

/**
* @param count -The numbers of variables passed to the function. The variables are command to execute.
*   followed by arguments to pass to the command
*   Since exec() does not perform path expansion, the command to execute needs
*   to be an absolute path.
* @param ... - A list of 1 or more arguments after the @param count argument.
*   The first is always the full path to the command to execute with execv()
*   The remaining arguments are a list of arguments to pass to the command in execv()
* @return true if the command @param ... with arguments @param arguments were executed successfully
*   using the execv() call, false if an error occurred, either in invocation of the
*   fork, waitpid, or execv() command, or if a non-zero return value was returned
*   by the command issued in @param arguments with the specified arguments.
*/

bool do_exec(int count, ...)
{
    va_list args;
    va_start(args, count);
    char * command[count+1];
    int i;
    for(i=0; i<count; i++)
    {
        command[i] = va_arg(args, char *);
    }
    command[count] = NULL;

    int pid = fork();
    int   status;
    if (pid == 0)
    {
        // Child
        execv(command[0], command);
        perror("execv");
        exit(1);
    }
    if (pid > 0)
    {
        waitpid(pid, &status, 0);
        if (WIFEXITED(status)) {
            int rc = WEXITSTATUS(status);
            if (rc != 0)
            {
                return false;
            }
        }
        if (WIFSIGNALED(status)) {
            printf("The process ended with kill -%d.\\n", WTERMSIG(status));
            return false;
        }

    }
    if (pid < 0) {
        perror("fork failed");
        return false;
    }


    va_end(args);

    return true;
}

/**
* @param outputfile - The full path to the file to write with command output.
*   This file will be closed at completion of the function call.
* All other parameters, see do_exec above
*/
bool do_exec_redirect(const char *outputfile, int count, ...)
{
    va_list args;
    va_start(args, count);
    char * command[count+1];
    int i;
    for(i=0; i<count; i++)
    {
        command[i] = va_arg(args, char *);
    }
    command[count] = NULL;

    int kidpid;
    int status;
    int fd = open(outputfile, O_WRONLY|O_TRUNC|O_CREAT, 0644);
    if (fd < 0) {
        perror("open");
        return false;
    }
    switch (kidpid = fork())
    {
        case -1:
            perror("fork"); 
            return false;
        case 0:
            if (dup2(fd, 1) < 0)
            {
                perror("dup2");
                exit(1);
            }
            close(fd);
            execvp(command[0], command);
            perror("execvp");
            exit(1);
        default:
            waitpid(kidpid, &status, 0);
            close(fd);
            if (WIFEXITED(status)) {
                printf("The process ended with exit(%d).\\n", WEXITSTATUS(status));
            }
            if (WIFSIGNALED(status)) {
                printf("The process ended with kill -%d.\\n", WTERMSIG(status));
                return false;
            }
    }

    va_end(args);

    return true;
}
