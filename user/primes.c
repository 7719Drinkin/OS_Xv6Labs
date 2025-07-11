
# include "kernel/types.h"
# include "user/user.h"

void runprocess(int listenfd)
{
    int num = 0;
    int forked = 0;
    int passed_num = 0;
    int pipes[2];

    if (pipe(pipes) < 0) {
        fprintf(2, "pipe failed\n");
        exit(1);
    }

    while(1){
        int read_bytes = read(listenfd, &passed_num, 4);
        // 如果读到的字节数小于等于0，表示没有更多数据可读
        if (read_bytes <= 0) {
            close(listenfd);
            if (forked) {
                // 通知子进程（所谓的右邻居）没有更多数据
                close(pipes[1]); 
                // 等待子进程结束
                int child_pid;
                wait(&child_pid);
            }
            exit(0);
        }

            // 如果是第一次读取到数据
        if (num == 0) {
            num = passed_num; // 将读取到的第一个数作为当前进程的素数
            printf("prime %d\n", num); // 打印当前进程的素数
        }

        // 如果读取到的数不是当前进程的素数
        if (passed_num % num != 0) {
            if (!forked){
                pipe(pipes); // 创建管道
                forked = 1; // 标记已经创建了子进程
                int pid = fork();
                if (pid < 0) {
                    fprintf(2, "fork failed\n");
                    exit(1);
                } else if (pid == 0) {
                    // 子进程
                    close(pipes[1]); // 关闭写端
                    close(listenfd); // 关闭读端
                    runprocess(pipes[0]); // 递归调用
                    exit(0);
                } else {
                    // 父进程
                    close(pipes[0]); // 关闭读端
                }
            }
            // 将读取到的数写入管道传递给子进程（右邻居）
            write(pipes[1], &passed_num, 4);
        }
    }
}

int main(int argc, char *argv[])
{
    int pipes[2];
    if (pipe(pipes) < 0) {
        fprintf(2, "pipe failed\n");
        exit(1);
    }

    for (int i = 2; i <= 35; i++) {
        write(pipes[1], &i, 4); // 将数字写入管道
    }
    close(pipes[1]); // 关闭写端，表示不再写入数据
    runprocess(pipes[0]); // 启动处理进程，读取管道中的数据
    exit(0);
}   