# Lab: Xv6 and Unix utilities
## 1. Boot xv6(easy)
### 1) 实验目的
- 安装xv6并利用qemu启动xv6
### 2) 实验步骤
1. 通过以下命令获取xv6资源
  ```$ git clone git://g.csail.mit.edu/xv6-labs-2021```
2. 克隆完成后进入到xv6-labs-2021文件夹中，切换git分支到util
  ```$ cd xv6-labs-2021```
  ```$ git checkout util```

3. 创建并运行xv6
  ```$ make qemu```
  ```
    riscv64-unknown-elf-gcc    -c -o kernel/entry.o kernel/entry.S
    riscv64-unknown-elf-gcc -Wall -Werror -O -fno-omit-frame-pointer -ggdb -DSOL_UTIL -MD -mcmodel=medany -ffreestanding -fno-common -nostdlib -mno-relax -I. -fno-stack-protector -fno-pie -no-pie   -c -o kernel/start.o kernel/start.c
    ...  
    riscv64-unknown-elf-ld -z max-page-size=4096 -N -e main -Ttext 0 -o user/_zombie user/zombie.o user/ulib.o user/usys.o user/printf.o user/umalloc.o
    riscv64-unknown-elf-objdump -S user/_zombie > user/zombie.asm
    riscv64-unknown-elf-objdump -t user/_zombie | sed '1,/SYMBOL TABLE/d; s/ .* / /; /^$/d' > user/zombie.sym
    mkfs/mkfs fs.img README  user/xargstest.sh user/_cat user/_echo user/_forktest user/_grep user/_init user/_kill user/_ln user/_ls user/_mkdir user/_rm user/_sh user/_stressfs user/_usertests user/_grind user/_wc user/_zombie 
    nmeta 46 (boot, super, log blocks 30 inode blocks 13, bitmap blocks 1) blocks 954 total 1000
    balloc: first 591 blocks have been allocated
    balloc: write bitmap block at sector 45
    qemu-system-riscv64 -machine virt -bios none -kernel kernel/kernel -m 128M -smp 3 -nographic -drive file=fs.img,if=none,format=raw,id=x0 -device virtio-blk-device,drive=x0,bus=virtio-mmio-bus.0

    xv6 kernel is booting

    hart 2 starting
    hart 1 starting
    init: starting sh
    $
  ```
4. 查看文件
  ```$ ls```
  ```
    .              1 1 1024
    ..             1 1 1024
    README         2 2 2059
    xargstest.sh   2 3 93
    cat            2 4 24256
    echo           2 5 23080
    forktest       2 6 13272
    grep           2 7 27560
    init           2 8 23816
    kill           2 9 23024
    ln             2 10 22880
    ls             2 11 26448
    mkdir          2 12 23176
    rm             2 13 23160
    sh             2 14 41976
    stressfs       2 15 24016
    usertests      2 16 148456
    grind          2 17 38144
    wc             2 18 25344
    zombie         2 19 22408
    console        3 20 0
  ```
5. 退出qemu
   ```Ctrl+a x```

### 3) 实验中遇到的问题和解决办法
- 在运行```make qemu```命令时发生报错：
  ```
    user/sh.c: In function ‘runcmd’:
    user/sh.c:58:1: error: infinite recursion detected [-Werror=infinite-recursion]
   58 | runcmd(struct cmd *cmd)
      | ^~~~~~
    user/sh.c:89:5: note: recursive call
   89 |     runcmd(rcmd->cmd);
      |     ^~~~~~~~~~~~~~~~~
    user/sh.c:109:7: note: recursive call
  109 |       runcmd(pcmd->left);
      |       ^~~~~~~~~~~~~~~~~~
    user/sh.c:116:7: note: recursive call
  116 |       runcmd(pcmd->right);
      |       ^~~~~~~~~~~~~~~~~~~
    user/sh.c:95:7: note: recursive call
   95 |       runcmd(lcmd->left);
      |       ^~~~~~~~~~~~~~~~~~
    user/sh.c:97:5: note: recursive call
   97 |     runcmd(lcmd->right);
      |     ^~~~~~~~~~~~~~~~~~~
    user/sh.c:127:7: note: recursive call
  127 |       runcmd(bcmd->cmd);
      |       ^~~~~~~~~~~~~~~~~
    cc1: all warnings being treated as errors
    make: *** [<builtin>: user/sh.o] Error 1
  ```
  - 这是由于编译器检测到无限递归并将警告当作错误而出现的报错。可能和编译器版本对警告的限制有关。
  - 通过临时删除Makefile文件里的-Werror来解决此问题。
  - 修改了Makefile文件后成功进入qemu虚拟机。先前因警告而报错部分不再发生报错
  ```
    user/sh.c: In function ‘runcmd’:
    user/sh.c:58:1: warning: infinite recursion detected [-Winfinite-recursion]
   58 | runcmd(struct cmd *cmd)
      | ^~~~~~
    user/sh.c:89:5: note: recursive call
   89 |     runcmd(rcmd->cmd);
      |     ^~~~~~~~~~~~~~~~~
    user/sh.c:109:7: note: recursive call
  109 |       runcmd(pcmd->left);
      |       ^~~~~~~~~~~~~~~~~~
    user/sh.c:116:7: note: recursive call
  116 |       runcmd(pcmd->right);
      |       ^~~~~~~~~~~~~~~~~~~
    user/sh.c:95:7: note: recursive call
   95 |       runcmd(lcmd->left);
      |       ^~~~~~~~~~~~~~~~~~
    user/sh.c:97:5: note: recursive call
   97 |     runcmd(lcmd->right);
      |     ^~~~~~~~~~~~~~~~~~~
    user/sh.c:127:7: note: recursive call
  127 |       runcmd(bcmd->cmd);
      |       ^~~~~~~~~~~~~~~~~
    ...
    xv6 kernel is booting

    hart 2 starting
    hart 1 starting
    init: starting sh
    $
  ```

### 4) 实验心得
- 通过实验掌握了获取xv6以及使用qemu启动xv6的方法
- 学习到了不同版本的编译器中可能存在着把警告都视为错误的保险机制，可以通过自身需求对编译器的此项功能进行更改
## 2. sleep(easy)
### 1) 实验目的
- 通过实现用户函数sleep来熟悉系统函数（如sleep、atoi、exit）的调用

### 2) 实验步骤
1. 在user文件夹下创建一个sleep.c文件
2. 在此文件中实现用户函数sleep的代码
3. 在xv6中运行此文件如```sleep 10```（系统会等待10ticks）；或者将其加入Makefile后对其进行```./grade-lab-util sleep```的lab批分操作，批分结果如下
  ```
  == Test sleep, no arguments == sleep, no arguments: OK (2.7s)
    (Old xv6.out.sleep_no_args failure log removed)
  == Test sleep, returns == sleep, returns: OK (1.0s)
  == Test sleep, makes syscall == sleep, makes syscall: OK (1.0s)
    (Old xv6.out.sleep failure log removed)
  ```

### 3) 实验中遇到的问题和解决办法
- ```sleep.c```的代码中，头文件因为包含```kernel/sysproc.c```，导致用户态和内核态的类型冲突。头文件中不能包含```kernel/sysproc.c```。
- 在sleep的调用输入中限制argc等于2，即第一个为调用名```sleep```，第二个为```ticks num```
- 在使用```./grade-lab-util sleep```的方法进行lab批分发生错误，是因为lab批分前，没有在Makefile里的UPROGS变量中加入sleep这一项，即
  ```
  {
  UPROGS=\
  	$U/_cat\
  	$U/_echo\
  	$U/_forktest\
  	$U/_grep\
  	$U/_init\
  	$U/_kill\
  	$U/_ln\
  	$U/_ls\
  	$U/_mkdir\
  	$U/_rm\
  	$U/_sh\
  	$U/_sleep\  # 此项，此注释不应该出现在Makefile文件里
  	$U/_stressfs\
  	$U/_usertests\
  	$U/_grind\
  	$U/_wc\
  	$U/_zombie\
  }
  ```
### 4) 实验心得
- 掌握了了一些系统函数的调用
- 掌握了关于main函数中调用参数的限制方法
- 掌握了Makefile中UPROGS变量的修改
- 掌握了lab```./grade-lab-util sleep```的批分方法

## 3. pingpong(easy)
### 1) 实验目的
- 利用系统调用函数fork、pipe、read和getpid，使用一对管道（每个方向一个）实现两个进程之间pingpong一个字节。父级应向子级发送一个字节；子级应该打印 “\<pid>： received ping”，其中 \<pid> 是它的进程 ID，将管道上的字节写入父级，然后退出;父级应该从子级读取字节，打印 “\<pid>： received pong”，然后退出。
### 2) 实验步骤
1. 在user文件夹中创建pingpong.c文件
2. 在pingpong.c中实现pingpong的代码
3. 将pingpong加入到Makefile内的UPROGS变量中
   ```
   UPROGS=\
  	$U/_cat\
  	$U/_echo\
  	$U/_forktest\
  	$U/_grep\
  	$U/_init\
  	$U/_kill\
  	$U/_ln\
  	$U/_ls\
  	$U/_mkdir\
  	$U/_pingpong\ # 此项，此注释不应该出现在Makefile文件里
  	$U/_rm\
  	$U/_sh\
  	$U/_sleep\
  	$U/_stressfs\
  	$U/_usertests\
  	$U/_grind\
  	$U/_wc\
  	$U/_zombie\
   ```
4. - 在xv6中调用```pingpong```，结果如下
     ```
     xv6 kernel is booting

     hart 1 starting
     hart 2 starting
     init: starting sh
     $ pingpong
     4: received ping
     3: received pong
     $
     ```
   - 或者使用lab批分，结果如下
     ```
     drinkin@DrinkinsLaptop:~/xv6-labs-2021$ ./grade-lab-util pingpong
     make: 'kernel/kernel' is up to date.
     == Test pingpong == pingpong: OK (1.6s)
     ```
### 3) 实验中遇到的问题和解决办法
- 在子进程和父进程进行传输过程中未对两个管道的读端和写端进行管控，如子进程向父进程输送时没有禁用子进程的写端和父进程的读端导致父进程同时能想子进程输送而导致死锁。解决方法是需要在子进程向父进程输送时禁用子进程的写端和父进程的读端。父进程向子进程输送时同理。

### 4) 实验心得
- 掌握了```pipe```管道的创建
- 掌握了父子进程之间通过管道进行并发数据传输时通过对不同管道不同端的开关控制实现互斥同步以及进行死锁防止的具体方法

## 4. primes(moderate)/(hard)
### 1) 实验目的
- 使用系统调用函数pipe和fork设置管道，以实现：第一个进程将整数2到35顺序输送到管道中，然后对于每个素数创建一个进程，其通过管道从其左邻居读取，并通过另一个管道写入其右邻居。由于 xv6 的文件描述符和进程数量有限，因此第一个进程可以在 35 个时停止。
### 2) 实验步骤
1. 在user文件夹中创建primes.c文件
2. 在primes.c中实现primes的代码
3. 将primes加入到Makefile内的UPROGS变量中
   ```
   UPROGS=\
	$U/_cat\
	$U/_echo\
	$U/_forktest\
	$U/_grep\
	$U/_init\
	$U/_kill\
	$U/_ln\
	$U/_ls\
	$U/_mkdir\
	$U/_pingpong\
	$U/_primes\ # 此项，此注释不应该出现在Makefile文件里
	$U/_rm\
	$U/_sh\
	$U/_sleep\
	$U/_stressfs\
	$U/_usertests\
	$U/_grind\
	$U/_wc\
	$U/_zombie\
   ```

4. - 在xv6中调用```primes```，结果如下
     ```
     xv6 kernel is booting

     hart 1 starting
     hart 2 starting
     init: starting sh
     $ primes
     prime 2
     prime 3
     prime 5
     prime 7
     prime 11
     prime 13
     prime 17
     prime 19
     prime 23
     prime 29
     prime 31
     $
     ```
   - 或者使用lab批分，结果如下
     ```
     drinkin@DrinkinsLaptop:~/xv6-labs-2021$ ./grade-lab-util primes
     make: 'kernel/kernel' is up to date.
     == Test primes == primes: OK (1.7s)
     ```

### 3) 实验中遇到的问题和解决办法
- 一个进程如何知道自己被分配的质数？从父进程读来的第一个数字就是自己的质数。
- 从左边进程读取数子的方法？通过递归将pipe的读端listenfd传入。
- 怎么等待子进程都结束以保证进程生命周期链不发生错误？每个进程最多fork出一个子进程，使用wait(int* pid)等待子进程即可。如果一个进程没有子进程则不需要等待。

### 4) 实验心得
- 熟悉了进程的递归fork的方法
- 熟悉了通过pipe在进程和子进程间进行数据传输的方法
- 认识到了进程的生命周期链正确性的重要性

## 5. find(moderate)
### 1) 实验目的
- 实现一个find的用户函数，用于在目录树中查找具有特定名称的所有文件。
- 参考user/ls.c以了解如何读取目录，并使用递归允许find下降到子目录进行find

### 2) 实验步骤
1. 在user文件夹中创建find.c文件
2. 参考user/ls.c，在find.c中实现find的代码
3. 将find加入到Makefile内的UPROGS变量中
   ```
   UPROGS=\
	$U/_cat\
	$U/_echo\
	$U/_find\ # 此项，此注释不应该出现在Makefile文件里
	$U/_forktest\
	$U/_grep\
	$U/_init\
	$U/_kill\
	$U/_ln\
	$U/_ls\
	$U/_mkdir\
	$U/_pingpong\
	$U/_primes\
	$U/_rm\
	$U/_sh\
	$U/_sleep\
	$U/_stressfs\
	$U/_usertests\
	$U/_grind\
	$U/_wc\
	$U/_zombie\
   ```
4. - 在xv6中调用find
     ```
     xv6 kernel is booting

     hart 2 starting
     hart 1 starting
     init: starting sh
     $ echo > b
     $ mkdir a
     $ find . b
     ./b
     $ echo > a/b
     $ find . b
     ./b
     ./a/b
     ```
   - 使用lab批分
   ```
   drinkin@DrinkinsLaptop:~/xv6-labs-2021$ ./grade-lab-util find
    make: 'kernel/kernel' is up to date.
    == Test find, in current directory == find, in current directory: OK (1.5s)
    (Old xv6.out.find_curdir failure log removed)
    == Test find, recursive == find, recursive: OK (0.8s)
    (Old xv6.out.find_recursive failure log removed)
   ```

### 3) 实验中遇到的问题和解决办法
- 在对文件进行路径输出时其父目录与文件名之间缺少```/```符号，是一个细节问题，需要在```case T_FILE```中对```filename```进行```strcmp```的时候进行长度加一。
- 未将find用户函数的参数限定在三个，即```find <directory> <target filename>```。在main函数中添加对```argc!=3```时```exit(1)```的限制就行
- 未及时关闭不再使用的fd，要在寻找到路径之后使用```close```对其进行关闭

### 4) 实验心得
- 掌握了查询文件路径时路径拼接的方法
- 熟悉了递归在文件查找时的使用

## 6. xargs(moderate)
### 1) 实验目的
- 利用系统调用函数```fork```和```exec```，从标准输入中读取每一个以```\n```分割的行，将每行作为单独1个参数，传递并执行下一个命令。

### 2) 实验步骤
1. 在user文件夹中创建xargs.c文件
2. 在xargs.c中实现primes的代码
3. 将xargs加入到Makefile内的UPROGS变量中
   ```
   UPROGS=\
	$U/_cat\
	$U/_echo\
	$U/_find\
	$U/_forktest\
	$U/_grep\
	$U/_init\
	$U/_kill\
	$U/_ln\
	$U/_ls\
	$U/_mkdir\
	$U/_pingpong\
	$U/_primes\
	$U/_rm\
	$U/_sh\
	$U/_sleep\
	$U/_stressfs\
	$U/_usertests\
	$U/_grind\
	$U/_wc\
	$U/_xargs\ # 此项，此注释不应该出现在Makefile文件里
	$U/_zombie\
   ```
4. - 在xv6中调用xargs
   ```
   $ find . b | xargs grep hello
   hello
   hello
   hello
   $
   ```

   ```
   $ sh < xargstest.sh
   $ $ $ $ $ $ hello
   ello
   hello
   $ $
   ```
   - 使用lab批分
   ```
   drinkin@DrinkinsLaptop:~/xv6-labs-2021$ ./grade-lab-util xargs
   make: 'kernel/kernel' is up to date.
   == Test xargs == xargs: OK (1.7s)
   ```

### 3) 实验中遇到的问题和解决办法
- 什么时候知道不会有更多的行输入？从file description 0读的时候，读到返回值为0
- xv6没有从file descriptor里读到空行符的库函数，怎么取出每个以```\n```分割的行？自己管理一个滑动窗口buf：
  假设我们有1个长度为10的buffer, 以 . 代表为空
  uf = [. . . . . . . . . .]
  我们read(buf, 10)读进来6个bytes
  buf = [a b \n c d \n . . . .]

  这时我们需要
  1. 找到第一个'\n'的下标, 用xv6提供的strchr函数, 得到下标为2
  2. 把下标0～1的byte转移到另一个buffer去作为额外参数
  3. 执行fork+exec+wait组合拳去执行真正执行的程序, 使用我们parse出来的额外的参数
  4. 修建我们的buffer, 把0～2的byte移除, 把3～9的byte移到队头
  此时buffer变成:
  buf = [c d \n . . . . . . .]

### 4) 实验心得
- 熟悉了利用缓冲区读取file decription并取出需要的字符的方法
- 更加清楚了进程创建和关闭的时机，每个进程都是作为每行承上启下的容器