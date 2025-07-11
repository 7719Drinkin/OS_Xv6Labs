
#include "kernel/types.h"
#include "user/user.h"

int main(int argc, char *argv[]) 
{
    int pid;
    int pipes1[2], pipes2[2];
    char buf[]={'a'};
    pipe(pipes1); // 创建第一个管道，从父进程到子进程，0是读端，1是写端
    pipe(pipes2); // 创建第二个管道，从子进程到父进程，0是读端，1是写端

    pid = fork(); // 创建子进程

    if (pid < 0) 
    {
        fprintf(2, "fork failed\n");
        exit(1);
    }
    else if (pid == 0)
    {
        // 子进程
        close(pipes1[1]); // 关闭父进程到子进程的写端，防止父进程读子进程时阻塞
        close(pipes2[0]); // 关闭子进程到父进程的读端，防止子进程写父进程时阻塞

        read(pipes1[0], buf, 1);
        printf("%d: received ping\n", getpid());
        write(pipes2[1], buf, 1);
        exit(0);
    } 
    else { 
        // 父进程
        close(pipes1[0]); // 关闭父进程到子进程的读端，防止父进程写子进程时阻塞
        close(pipes2[1]); // 关闭子进程到父进程的写端，防止子进程读父进程时阻塞

        write(pipes1[1], buf, 1);
        read(pipes2[0], buf, 1);
        printf("%d: received pong\n", getpid());

        exit(0);
    }
}
