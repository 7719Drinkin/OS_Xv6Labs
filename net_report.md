# Lab: networking

## Your Job (hard)
### 1) 实验目的
- 完成 `e1000_transmit()` 和 `e1000_recv()` 在 kernel/e1000.c 中，以便驱动程序可以传输和接收数据包。

### 2) 实验步骤
1. 在kernel/e1000.c中实现`e1000_transmit`，`e1000_transmit`提供一个新的packet, 需要在ring里找到下一个空余位置, 然后把它放进去等待传输。
    ```c
    int
    e1000_transmit(struct mbuf *m)
    {
      acquire(&e1000_lock);
      // 查询ring里下一个packet的下标
      int idx = regs[E1000_TDT];

      if ((tx_ring[idx].status & E1000_TXD_STAT_DD) == 0) {
        // 之前的传输还没有完成
        release(&e1000_lock);
        return -1;
      }

      // 释放上一个包的内存
      if (tx_mbufs[idx])
        mbuffree(tx_mbufs[idx]);

      // 把这个新的网络包的pointer塞到ring这个下标位置
      tx_mbufs[idx] = m;
      tx_ring[idx].length = m->len;
      tx_ring[idx].addr = (uint64) m->head;
      tx_ring[idx].cmd = E1000_TXD_CMD_RS | E1000_TXD_CMD_EOP;
      regs[E1000_TDT] = (idx + 1) % TX_RING_SIZE;

      release(&e1000_lock);
      return 0;
    }
    ```

2. 在kernel/e1000.c中实现`e1000_recv`，`e1000_recv`需要遍历这个ring, 把所有新到来的packet交由网络上层的协议/应用去处理。
    ```c
    static void
    e1000_recv(void)
    {
      while (1) {
        // 把所有到达的packet向上层递交
        int idx = (regs[E1000_RDT] + 1) % RX_RING_SIZE;
        if ((rx_ring[idx].status & E1000_RXD_STAT_DD) == 0) {
          // 没有新包了
          return;
        }
        rx_mbufs[idx]->len = rx_ring[idx].length;
        // 向上层network stack传输
        net_rx(rx_mbufs[idx]);
        // 把这个下标清空 放置一个空包
        rx_mbufs[idx] = mbufalloc(0);
        rx_ring[idx].status = 0;
        rx_ring[idx].addr = (uint64)rx_mbufs[idx]->head;
        regs[E1000_RDT] = idx;
      }
    }
    ```

3. 使用lab批分
    ```bash
    == Test running nettests ==
    $ make qemu-gdb
    (5.4s)
    == Test   nettest: ping ==
      nettest: ping: OK
    == Test   nettest: single process ==
      nettest: single process: OK
    == Test   nettest: multi-process ==
      nettest: multi-process: OK
    == Test   nettest: DNS ==
      nettest: DNS: OK
    == Test time ==
    time: OK
    Score: 100/100
    ```
### 3) 实验中遇到的问题和解决办法
- 实现`e1000_transmit`时，由于之前已经上锁，若判断得到之前的传输还没有完成，需要取消传包操作，但是不记得释放锁导致一直锁着。所以需要记得释放锁。

- 实现`e1000_recv`时，需要记得在没有新包后退出函数，否则会一直空转。

### 4) 实验心得
- 熟悉了xv6中网络的实现方式
- 熟悉了网络传包的逻辑