#include "kernel/param.h"
#include "kernel/types.h"
#include "user/user.h"

#define buf_size 512

int main(int argc, char *argv[])
{
  char buf[buf_size + 1] = {0}; // 缓冲区
  uint occupy = 0;  // 当前缓冲区占用字节数
  char *xargv[MAXARG] = {0};  // 存储命令行参数
  int stdin_end = 0; // 标记是否从标准输入读取

  for (int i = 1; i < argc; i++) {
    xargv[i - 1] = argv[i];
  }

  while(!(stdin_end && occupy == 0)){
    // 如果输入未达到末尾且缓冲区未满
    if (!stdin_end){
      int remain_size = buf_size - occupy;
      int read_bytes = read(0, buf + occupy, remain_size);
      if(read_bytes < 0){
        fprintf(2, "xargs: read returns -1 error\n");
      }
      if (read_bytes == 0){
        close(0);
        stdin_end = 1; // 标记已到达标准输入末尾
      }
      occupy += read_bytes;
    }
    // 
    char *line_end = strchr(buf, '\n');
    while (line_end){
      char xbuf[buf_size + 1] = {0}; // 临时缓冲区
      memcpy(xbuf, buf, line_end - buf);
      xargv[argc - 1] = xbuf; // 将命令行参数存入xargs
      int ret = fork();
      if (ret < 0) {
        fprintf(2, "xargs: fork error\n");
        exit(1);
      } else if (ret == 0) {
        // 子进程执行命令
        if (!stdin_end) {
          close(0);
        }
        if (exec(argv[1], xargv) < 0) {
          fprintf(2, "xargs: exec fails with -1\n");
          exit(1);
        }
      } else {
        memmove(buf, line_end + 1, occupy - (line_end - buf) - 1);
        occupy -= line_end - buf + 1;
        memset(buf + occupy, 0, buf_size - occupy);
        // 等待子进程结束
        int pid;
        wait(&pid);

        line_end = strchr(buf, '\n');
      }
    }
  }
  exit(0);
}