# Lab: locks
## 1. Memory allocator (moderate)
### 1) 实验目的
- 把一个全局的物理内存分配器打散到每个cpu核上, 使得在每个cpu核上运行的代码可以并行的呼叫kalloc/kfree来分配和释放内存，且不对同一个kmem.lock造成竞态。即为每一个cpu核维系一个freelist空闲内存页链表, 只有在本核的内存不够时, 才试图去偷取别的核的内存。

### 2) 实验步骤
1. 在kernel/kalloc.c中改写数据结构，改为每cpu核一个freelist。
    ```c
    struct {
      struct spinlock lock;
      struct run *freelist;
    } kmem[NCPU];

    void
    kinit()
    {
      for (int i = 0; i < NCPU; i++) {
        char name[9] = {0};
        snprintf(name, 8, "kmem-%d", i);
        initlock(&kmem[i].lock, name);
      }
      freerange(end, (void*)PHYSTOP);
    }
    ```

2. 在kernel/kalloc.c中的`kfree()`中暂时先关闭interrupt中断, 免得在读取到当前cpu id是1后, 这一段代码被放到cpu 2上去跑, 造成内存不均衡。
    ```c
    void
    kfree(void *pa)
    {
      struct run *r;

      if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
        panic("kfree");

      push_off();
      int cpu = cpuid();
      memset(pa, 1, PGSIZE);
      r = (struct run*)pa;
      // --- critical session ---
      acquire(&kmem[cpu].lock);
      r->next = kmem[cpu].freelist;
      kmem[cpu].freelist = r;
      release(&kmem[cpu].lock);
      // --- end of critical session ---
      pop_off();
    }
    ```

3. 在kernel/kalloc.c中编写一个`ksteal()`的helper函数，以round-robin的形式去偷取邻居cpu的空余内存页, 一次只偷取一页。
    ```c
    // Try steal a free physical memory page from another core
    // interrupt should already be turned off
    // return NULL if not found free page
    void *
    ksteal(int cpu) {
      struct run *r;
      for (int i = 1; i < NCPU; i++) {
        // 从右边的第一个邻居开始偷
        int next_cpu = (cpu + i) % NCPU;
        // --- critical session ---
        acquire(&kmem[next_cpu].lock);
        r = kmem[next_cpu].freelist;
        if (r) {
          // steal one page
          kmem[next_cpu].freelist = r->next;
        }
        release(&kmem[next_cpu].lock);
        // --- end of critical session ---
        if (r) {
          break;
        }
      }
      // 有可能返回NULL, 如果邻居也都没有空余页的话
      return r;
    }

    void *
    kalloc(void)
    {
      struct run *r;
      push_off();
      int cpu = cpuid();
      // --- critical session ---
      acquire(&kmem[cpu].lock);
      r = kmem[cpu].freelist;
      if (r) {
        kmem[cpu].freelist = r->next;
      }
      release(&kmem[cpu].lock);
      // --- end of critical session ---

      if (r == 0) {
        r = ksteal(cpu);
      }
      if(r)
        memset((char*)r, 5, PGSIZE); // fill with junk

      pop_off();
      return (void*)r;
    }
    ```

4. - 在xv6中运行`kalloctest`和`usertests`
     ```bash
     $ kalloctest
     start test1
     test1 results:
     --- lock kmem/bcache stats
     lock: bcache: #test-and-set 0 #acquire() 356
     --- top 5 contended locks:
     lock: proc: #test-and-set 66437 #acquire() 430548
     lock: proc: #test-and-set 65790 #acquire() 430553
     lock: proc: #test-and-set 55756 #acquire() 430486
     lock: proc: #test-and-set 46489 #acquire() 430405
     lock: proc: #test-and-set 40996 #acquire() 430486
     tot= 0
     test1 OK
     start test2
     total free number of pages: 32495 (out of 32768)
     .....
     test2 OK
     $ usertests sbrkmuch
     usertests starting
     test sbrkmuch: OK
     ALL TESTS PASSED
     $ usertests
     usertests starting
     ...
     ALL TESTS PASSED
     ```

   - 使用lab批分
     ```bash
     $ make qemu-gdb
     (83.7s)
     == Test   kalloctest: test1 ==
       kalloctest: test1: OK
     == Test   kalloctest: test2 ==
       kalloctest: test2: OK
     == Test kalloctest: sbrkmuch ==
     $ make qemu-gdb
     kalloctest: sbrkmuch: OK (9.1s)
     ```

### 3) 实验中遇到的问题和解决办法
- 实现`ksteal()`的helper函数时，由于使用的是round-robin的方法对邻居cpu进行空闲访问，要注意对整个steal过程加上互斥锁。否则会出现冲突。

### 4) 实验心得
- 熟悉了实现每个CPU带独自内存的逻辑
- 熟悉了CPU借用其他CPU的内存的逻辑
- 掌握了CPU操作的互斥锁逻辑
- 深刻认识了CPU多核分内存的逻辑

## 2. Buffer cache (hard)
### 1) 实验目的
- 对buffer cache进行修改，使得每次重复调用xv6缓存在内存里的block块时直接从内存(即这个buffer cache pool)读取, 而不用做磁盘I/O。

### 2) 实验步骤
1. 在kernel/bio.c中改写数据结构，并修改两个很简单的小功能`bpin`和`bunpin`, 然后在`binit`里把每个锁都初始化。
   原始版本的buffer cache由一个大锁bcache.lock保护，限制了并行运行的效率。我们要把它拆解为更精细的锁管理，用hash bucket的思想。并且放弃双链表的管理方式，直接使用ticks时间戳来实现LRU(least-recently-used)算法。
    ```c
    #define BUCKETSIZE 13 // number of hashing buckets
    #define BUFFERSIZE 5 // number of available buckets per bucket

    extern uint ticks; // system time clock

    // 一共有 13 * 5 = 65个buffer块儿
    struct {
      struct spinlock lock;
      struct buf buf[BUFFERSIZE];
    } bcachebucket[BUCKETSIZE];

    int
    hash(uint blockno)
    {
      return blockno % BUCKETSIZE;
      }

    void
    bpin(struct buf *b) {
      int bucket = hash(b->blockno);
      acquire(&bcachebucket[bucket].lock);
      b->refcnt++;
      release(&bcachebucket[bucket].lock);
     }

    void
    bunpin(struct buf *b) {
      int bucket = hash(b->blockno);
      acquire(&bcachebucket[bucket].lock);
      b->refcnt--;
      release(&bcachebucket[bucket].lock);
    }

    void
    binit(void)
    {
      for (int i = 0; i < BUCKETSIZE; i++) {
        initlock(&bcachebucket[i].lock, "bcachebucket");
        for (int j = 0; j < BUFFERSIZE; j++) {
          initsleeplock(&bcachebucket[i].buf[j].lock, "buffer");
        }
      }
    }
    ```

2. 在kernel/bio.c中对`bget()`进行改写：根据所需要的`blockno`, 计算出对应哪个`bucket`后, 拿锁进行查找。如果没能找到对应的`buffer cache block`，则就在当前`bucket`里试图寻找一个空闲的来分配。
    ```c
    // Look through buffer cache for block on device dev.
    // If not found, allocate a buffer.
    // In either case, return locked buffer.
    static struct buf*
    bget(uint dev, uint blockno)
    {
      struct buf *b;
      int bucket = hash(blockno);
      acquire(&bcachebucket[bucket].lock);
      // --- critical session for the bucket ---
      // Is the block already cached?
      for (int i = 0; i < BUFFERSIZE; i++) {
        b = &bcachebucket[bucket].buf[i];
        if (b->dev == dev && b->blockno == blockno) {
          b->refcnt++;
          b->lastuse = ticks;
          release(&bcachebucket[bucket].lock);
          acquiresleep(&b->lock);
          // --- end of critical session
          return b;
        }
      }
      // Not cached.
      // Recycle the least recently used (LRU) unused buffer.
      uint least = 0xffffffff; // 这个是最大的unsigned int
      int least_idx = -1;
      for (int i = 0; i < BUFFERSIZE; i++) {
        b = &bcachebucket[bucket].buf[i];
        if(b->refcnt == 0 && b->lastuse < least) {
          least = b->lastuse;
          least_idx = i;
        }
      }

      if (least_idx == -1) {
        // 理论上, 这里应该去邻居bucket偷取空闲buffer
        panic("bget: no unused buffer for recycle");
      }

      b = &bcachebucket[bucket].buf[least_idx];
      b->dev = dev;
      b->blockno = blockno;
      b->lastuse = ticks;
       b->valid = 0;
      b->refcnt = 1;
      release(&bcachebucket[bucket].lock);
      acquiresleep(&b->lock);
      // --- end of critical session
      return b;
    }

    // Release a locked buffer.
    void
    brelse(struct buf *b)
    {
      if(!holdingsleep(&b->lock))
        panic("brelse");

      int bucket = hash(b->blockno);
      acquire(&bcachebucket[bucket].lock);
      b->refcnt--;
      release(&bcachebucket[bucket].lock);
      releasesleep(&b->lock);
    }
    ```

3. - 在xv6中运行`bcachetest`和`usertests`
     ```bash
     $ bcachetest
     start test0
     test0 results:
     --- lock kmem/bcache stats
     lock: bcachebucket: #test-and-set 0 #acquire() 6174
     lock: bcachebucket: #test-and-set 0 #acquire() 6186
     lock: bcachebucket: #test-and-set 0 #acquire() 4274
     lock: bcachebucket: #test-and-set 0 #acquire() 4270
     lock: bcachebucket: #test-and-set 0 #acquire() 2272
     lock: bcachebucket: #test-and-set 0 #acquire() 4272
     lock: bcachebucket: #test-and-set 0 #acquire() 2682
     lock: bcachebucket: #test-and-set 0 #acquire() 6952
     lock: bcachebucket: #test-and-set 0 #acquire() 4174
     lock: bcachebucket: #test-and-set 0 #acquire() 6176
     lock: bcachebucket: #test-and-set 0 #acquire() 6174
     lock: bcachebucket: #test-and-set 0 #acquire() 6174
     lock: bcachebucket: #test-and-set 0 #acquire() 6174
     --- top 5 contended locks:
     lock: virtio_disk: #test-and-set 560083 #acquire() 1119
     lock: proc: #test-and-set 144083 #acquire() 664546
     lock: proc: #test-and-set 127725 #acquire() 685048
     lock: proc: #test-and-set 122224 #acquire() 664549
     lock: proc: #test-and-set 111095 #acquire() 685050
     tot= 0
     test0: OK
     start test1
     test1 OK
     ```
   - 使用lab批分
     ```bash
     make[1]: Leaving directory '/home/drinkin/xv6-labs-2021'
     == Test running kalloctest ==
     $ make qemu-gdb
     (91.8s)
     == Test   kalloctest: test1 ==
       kalloctest: test1: OK
     == Test   kalloctest: test2 ==
       kalloctest: test2: OK
     == Test kalloctest: sbrkmuch ==
     $ make qemu-gdb
     kalloctest: sbrkmuch: OK (9.1s)
     == Test running bcachetest ==
     $ make qemu-gdb
     (9.6s)
     == Test   bcachetest: test0 ==
       bcachetest: test0: OK
     == Test   bcachetest: test1 ==
       bcachetest: test1: OK
     == Test usertests ==
       $ make qemu-gdb
     usertests: OK (134.6s)
     == Test time ==
     time: OK
     Score: 70/70
     ```

### 3) 实验中遇到的问题和解决办法
- 在实现`bget()`的时候，需要在buf.h中buf结构中添加一个`uint lastuse`的域，用于记录最后被使用的ticks是多少，以实现LRU算法。否则LRU算法无法执行。

### 4) 实验心得
- 熟悉了buffer cache的实现方法
- 熟悉了buffer cache的实现过程中所需要的互斥锁的逻辑
