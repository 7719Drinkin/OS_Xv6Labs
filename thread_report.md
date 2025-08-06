# Lab: Multithreading
## 1. Uthread: switching between threads (moderate)
### 1) 实验目的
- 设计并实现一个用户级线程调度。

### 2) 实验步骤
1. 参考kernel/switch.S里用来保存寄存器状态的汇编代码，将其放置到user/uthread_switch.S里。
    ```S
        .text

    /*
         * save the old thread's registers,
         * restore the new thread's registers.
         */
        .globl thread_switch
    thread_switch:
        /* YOUR CODE HERE */
        sd ra, 0(a0)
        sd sp, 8(a0)
        sd s0, 16(a0)
        sd s1, 24(a0)
        sd s2, 32(a0)
        sd s3, 40(a0)
        sd s4, 48(a0)
        sd s5, 56(a0)
        sd s6, 64(a0)
        sd s7, 72(a0)
        sd s8, 80(a0)
        sd s9, 88(a0)
        sd s10, 96(a0)
        sd s11, 104(a0)

        ld ra, 0(a1)
        ld sp, 8(a1)
        ld s0, 16(a1)
        ld s1, 24(a1)
        ld s2, 32(a1)
        ld s3, 40(a1)
        ld s4, 48(a1)
        ld s5, 56(a1)
        ld s6, 64(a1)
        ld s7, 72(a1)
        ld s8, 80(a1)
        ld s9, 88(a1)
        ld s10, 96(a1)
        ld s11, 104(a1)
        ret    /* return to ra */
    ```
2. 在user/uthread.c中构造出`struct thread`用来保存每个用户态线程寄存器的`struct thread_context`。
    ```c
    // Saved registers for user-level thread switching
    struct thread_context {
      uint64 ra;
      uint64 sp;

      // callee-saved
      uint64 s0;
      uint64 s1;
      uint64 s2;
      uint64 s3;
      uint64 s4;
      uint64 s5;
      uint64 s6;
      uint64 s7;
      uint64 s8;
      uint64 s9;
      uint64 s10;
      uint64 s11;
    };

    struct thread {
      struct thread_context     thread_context;    /* register status */
      char                      stack[STACK_SIZE]; /* the thread's stack */
      int                       state;             /* FREE, RUNNING, RUNNABLE */
    };
    ```

3. 在user/uthread.c中实现一个`clear_thread`函数来初始化一个新线程的状态，需要把它的第一次的返回地址ra设置成用户传进来的线程函数func的地址, 并且把这个线程的状态设置成RUNNABLE, 让调度器去跑它。需要注意的是, 在risc-v里, 栈是由高地址向低地址增长的, 所以这个线程的最初stack pointer sp应该在栈的顶端。
    ```c
    /*
     * helper function to setup the routine for a newly-created thread
     */
    void clear_thread(struct thread *t, void (*func)()) {
      memset((void *)&t->stack, 0, STACK_SIZE);
      memset((void *)&t->thread_context, 0, sizeof(struct thread_context));
      t->state = RUNNABLE;
      t->thread_context.sp = (uint64) ((char *)&t->stack + STACK_SIZE);  // 初始sp在栈顶
      t->thread_context.ra = (uint64) func;  // 初始跳转位置是user传进来的线程函数
    }
    ```

4. 将user/uthread.c中缺失的两个` /* YOUR CODE HERE */ `的地方填补上。
    ```c
    void 
    thread_schedule(void)
    {
      struct thread *t, *next_thread;

      // ...省略...

      if (current_thread != next_thread) {         /* switch threads?  */
        next_thread->state = RUNNING;
        t = current_thread;
        current_thread = next_thread;
        /* YOUR CODE HERE
         * Invoke thread_switch to switch from t to next_thread:
         * thread_switch(??, ??);
         */
        thread_switch((uint64) t, (uint64) current_thread);
      } else
        next_thread = 0;
    }

    void 
    thread_create(void (*func)())
    {
      struct thread *t;

      for (t = all_thread; t < all_thread + MAX_THREAD; t++) {
        if (t->state == FREE) break;
      }
      // YOUR CODE HERE
      clear_thread(t, func);
    }
    ```

5. 将`uthread`添加到Makefile中的UPROGS变量中：
   ```Makefile
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
	$U/_stressfs\
	$U/_usertests\
	$U/_uthread\
	$U/_grind\
	$U/_wc\
	$U/_zombie\

   ```

6. - 在xv6中运行`uthread`
     ```bash
     $ uthread
     thread_a started
     thread_b started
     thread_c started
     thread_c 0
     thread_a 0
     thread_b 0
     thread_c 1
     thread_a 1
     thread_b 1
     thread_c 2
     thread_a 2
     thread_b 2
     ...
     thread_c 99
     thread_a 99
     thread_b 99
     thread_c: exit after 100
     thread_a: exit after 100
     thread_b: exit after 100
     thread_schedule: no runnable threads
     ```

   - 使用lab批分
     ```bash
     drinkin@DrinkinsLaptop:~/xv6-labs-2021$ ./grade-lab-thread uthread
     make: 'kernel/kernel' is up to date.
     == Test uthread == uthread: OK (2.1s)
     ```
### 3) 实验中遇到的问题和解决办法
- 实现一个`clear_thread`函数来初始化一个新线程的状态时，没有注意到在risc-v里, 栈是由高地址向低地址增长的, 线程的最初stack pointer sp指向错误。这个线程的最初stack pointer sp应该在栈的顶端。

### 4) 实验心得
- 熟悉了用户级线程的实现方式
- 理解了用户级线程和内核级线程的区别

## 2. Using threads (moderate)
### 1) 实验目的
- 实验给出了一个单线程版本的hashtable, 在多线程运行的情况下有race condition, 需要在合适的地方加锁来避免竞态。

### 2) 实验步骤
1. 回答题目问题：
   > Q: Why are there missing keys with 2 threads, but not with 1 thread? 
Identify a sequence of events with 2 threads that can lead to a key being missing.

   > A: For example, consider 2 threads are concurrently adding[4, 'd'] & [5, 'e'] pair into the same bucket respectively:
    the bucket is originally [<1, 'a'>, <2, 'b'>, \<3, 'c'>]
    in put() function, they both iterate to the end of the linked list
    and decided to insert at the back of \<3, 'c'>
    whoever execute the line '*p = e' will have the other side's changed overwritten and thus lost.

2. 在notxv6/ph.c中，为每个`bucket`配置一个`pthread_mutex`锁来保证只有一个线程可以读这个`bucket`里的所有数据进行读和写的操作。
    ```c
    pthread_mutex_t locks[NBUCKET]; // one lock per bucket

    void init_locks() {
      // 在main函数一开始 呼叫一下这个函数
      for (int i = 0; i < NBUCKET; i++) {
        pthread_mutex_init(&locks[i], NULL);
      }
    }

    static 
    void put(int key, int value)
    {
      int i = key % NBUCKET;
      pthread_mutex_lock(&locks[i]); // 加bucket锁
      // is the key already present?
      struct entry *e = 0;
      for (e = table[i]; e != 0; e = e->next) {
        if (e->key == key)
          break;
      }
      if(e){
        // update the existing key.
        e->value = value;
      } else {
        // the new is new.
        insert(key, value, &table[i], table[i]);
      }
      pthread_mutex_unlock(&locks[i]); // 放bucket锁
    }

    static struct entry*
    get(int key)
    {
      int i = key % NBUCKET;
      pthread_mutex_lock(&locks[i]);  // 加bucket锁

      struct entry *e = 0;
      or (e = table[i]; e != 0; e = e->next) {
        if (e->key == key) break;
      }

      pthread_mutex_unlock(&locks[i]); // 放bucket锁
      return e;
    }
    ```

3. - `make ph`后运行`./ph 1` `./ph 2`：
     ```bash
     drinkin@DrinkinsLaptop:~/xv6-labs-2021$ make ph
     gcc -o ph -g -O2 -DSOL_THREAD -DLAB_THREAD notxv6/ph.c -pthread
     drinkin@DrinkinsLaptop:~/xv6-labs-2021$ ./ph 1
     100000 puts, 4.993 seconds, 20028 puts/second
     0: 0 keys missing
     100000 gets, 6.797 seconds, 14712 gets/second
     drinkin@DrinkinsLaptop:~/xv6-labs-2021$ ./ph 2
     100000 puts, 4.565 seconds, 21907 puts/second
     1: 0 keys missing
     0: 0 keys missing
     200000 gets, 6.904 seconds, 28970 gets/second
     ```
   - 使用lab批分：
     ```bash
     == Test ph_safe == make[1]: Entering directory '/home/drinkin/xv6-labs-2021'
     gcc -o ph -g -O2 -DSOL_THREAD -DLAB_THREAD notxv6/ph.c -pthread
     make[1]: Leaving directory '/home/drinkin/xv6-labs-2021'
     ph_safe: OK (13.9s)
     == Test ph_fast == make[1]: Entering directory '/home/drinkin/xv6-labs-2021'
     make[1]: 'ph' is up to date.
     make[1]: Leaving directory '/home/drinkin/xv6-labs-2021'
     ph_fast: OK (26.1s)
     ```

### 3) 实验中遇到的问题和解决办法
- 在notxv6/ph.c的main函数里忘记添加`pthread_mutex_init(&lock, NULL);`以初始化锁，使得锁没有发挥作用。在notxv6/ph.c中实现一个`init_locks()`的函数将所有需要用到的锁初始化即可。

### 4) 实验心得
- 熟悉了多线程运行时锁的必要性
- 掌握了锁的添加方法

## 3. Barrier(moderate)
### 1) 实验目的
- 实现一个`barrier`，其为一个多线程同步的手段和数据结构。每个线程都会呼叫`barrier()`函数，将会卡在里面直到每个需要同步的线程都进入这个函数. 需要使用条件变量来实现等待和唤醒的功能。

### 2) 实验步骤
1. 在notxv6/barrier.c中实现`barrier()`函数
    ```c
    static __thread int thread_flag = 0;  // local sense 每个线程单独的内存地址

    struct barrier {
      pthread_mutex_t barrier_mutex;
      pthread_cond_t barrier_cond;
      int nthread;      // Number of threads that have reached this round of the barrier
      int round;        // Barrier round
      int flag;  
    } bstate;

    static void
    barrier_init(void)
    {
      assert(pthread_mutex_init(&bstate.barrier_mutex, NULL) == 0);
      assert(pthread_cond_init(&bstate.barrier_cond, NULL) == 0);
      bstate.round = 0;
      bstate.nthread = 0;
      bstate.flag = 0;
    }

    static void 
    barrier()
    {
      // YOUR CODE HERE
      //
      // Block until all threads have called barrier() and
      // then increment bstate.round.
      //
      pthread_mutex_lock(&bstate.barrier_mutex);
      thread_flag = !thread_flag;
      // wait until all previous round threads has exited the barrier
      // 等上一轮的最后一个抵达的人来翻页
      while (thread_flag == bstate.flag) {
          pthread_cond_wait(&bstate.barrier_cond, &bstate.barrier_mutex);
      }
      int arrived = ++bstate.nthread;
      if (arrived == nthread) {
        // I am the last thread in this round
        // need to flip the round flag
        // 我是最后一个抵达的人, 我来翻页并唤醒前N-1个在沉睡的线程
        bstate.round++;
        bstate.flag = !bstate.flag;
        bstate.nthread = 0;
        pthread_cond_broadcast(&bstate.barrier_cond);
      } else {
        // wait for other threads
        // 主动沉睡 等待这一轮的结束并被唤醒
        pthread_cond_wait(&bstate.barrier_cond, &bstate.barrier_mutex);
      }
      pthread_mutex_unlock(&bstate.barrier_mutex);
    }
    ```

2. - `make barrier`后运行`./barrier 2`:
     ```bash
     drinkin@DrinkinsLaptop:~/xv6-labs-2021$ make barrier
     gcc -o barrier -g -O2 -DSOL_THREAD -DLAB_THREAD notxv6/barrier.c -pthread
     drinkin@DrinkinsLaptop:~/xv6-labs-2021$ ./barrier 2
     OK; passed
     ```
   - 使用lab批分
     ```bash
     == Test barrier == make[1]: Entering directory '/home/drinkin/xv6-labs-2021'
     gcc -o barrier -g -O2 -DSOL_THREAD -DLAB_THREAD notxv6/barrier.c -pthread
     make[1]: Leaving directory '/home/drinkin/xv6-labs-2021'
     barrier: OK (4.2s)
     ```
   - 总分
     ```bash
     == Test uthread ==
     $ make qemu-gdb
     uthread: OK (3.5s)
     == Test answers-thread.txt == answers-thread.txt: OK
     == Test ph_safe == make[1]: Entering directory '/home/drinkin/xv6-labs-2021'
     gcc -o ph -g -O2 -DSOL_THREAD -DLAB_THREAD notxv6/ph.c -pthread
     make[1]: Leaving directory '/home/drinkin/xv6-labs-2021'
     ph_safe: OK (14.2s)
     == Test ph_fast == make[1]: Entering directory '/home/drinkin/xv6-labs-2021'
     make[1]: 'ph' is up to date.
     make[1]: Leaving directory '/home/drinkin/xv6-labs-2021'
     ph_fast: OK (26.7s)
     == Test barrier == make[1]: Entering directory '/home/drinkin/xv6-labs-2021'
     gcc -o barrier -g -O2 -DSOL_THREAD -DLAB_THREAD notxv6/barrier.c -pthread
     make[1]: Leaving directory '/home/drinkin/xv6-labs-2021'
     barrier: OK (4.2s)
     == Test time ==
     time: OK
     Score: 60/60
     ```
### 3) 实验中遇到的问题和解决办法
- 在放置每一轮barrier线程的睡眠和唤醒的位置（即睡眠和唤醒的时机）时候，没有意识到需要等上一轮的最后一个抵达的线程来翻页，导致实现有错误。需要通过`thread_flag != bstate.flag`的条件来让上一轮的最后一个抵达的线程来翻页。

### 4) 实验心得
- 熟悉了barrier线程同步的逻辑
- 掌握了线程同步的概念
- 熟悉了barrier的实现方式