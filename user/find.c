#include "kernel/types.h"
#include "kernel/fcntl.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"

// 从整个路径中取得文件名
char *basename(char *pathname)
{
    char *prev = 0;
    char *curr = strchr(pathname, '/');
    while (curr != 0) {
        prev = curr;
        curr = strchr(curr + 1, '/');
    }
    return prev;
}

// 查找目录下的文件
void find(char *cur_path, char *target)
{
    char buf[512], *p;
    int fd;
    struct dirent de;
    struct stat st;

    if ((fd = open(cur_path, O_RDONLY)) < 0) {
        fprintf(2, "find: cannot open %s\n", cur_path);
        return;
    }

    if (fstat(fd, &st) < 0) {
        fprintf(2, "find: cannot stat %s\n", cur_path);
        close(fd);
        return;
    }

    switch (st.type) {
    case T_FILE:
        char *filename = basename(cur_path);
        int match = 1;
        // 检查文件名是否与目标匹配
        if(filename == 0 || strcmp(filename + 1, target) != 0) {
            match = 0;
        }
        if (match) {
            printf("%s\n", cur_path); // 输出匹配的文件路径
        }
        close(fd);
        break;

    case T_DIR:
        // 进入下一级目录
        memset(buf, 0, sizeof(buf));
        uint curr_path_len = strlen(cur_path);
        memcpy(buf, cur_path, curr_path_len);
        buf[curr_path_len] = '/';
        p = buf + curr_path_len + 1; // 指向路径末尾
        while(read(fd, &de, sizeof(de)) == sizeof(de)) {
            if (de.inum == 0 || strcmp(de.name, ".") == 0 || strcmp(de.name, "..") == 0)
                continue; // 跳过空目录、当前目录和上级目录
            memcpy(p, de.name, DIRSIZ);
            p[DIRSIZ] = 0; 
            find(buf, target); // 递归查找子目录
        }
        close(fd);
        break;
    }
}

int main(int argc, char *argv[])
{
    if (argc != 3) {
        fprintf(2, "Usage: find <directory> <target filename>\n");
        exit(1);
    }

    char *path = argv[1];
    char *target = argv[2];

    find(path, target);
    exit(0);
}