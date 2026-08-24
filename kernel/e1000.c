#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"
#include "e1000_dev.h"
#include "net.h"

#define TX_RING_SIZE 16
static struct tx_desc tx_ring[TX_RING_SIZE] __attribute__((aligned(16)));
static struct mbuf *tx_mbufs[TX_RING_SIZE];

#define RX_RING_SIZE 16
static struct rx_desc rx_ring[RX_RING_SIZE] __attribute__((aligned(16)));
static struct mbuf *rx_mbufs[RX_RING_SIZE];

// remember where the e1000's registers live.
static volatile uint32 *regs;

struct spinlock e1000_lock;

// called by pci_init().
// xregs is the memory address at which the
// e1000's registers are mapped.
void
e1000_init(uint32 *xregs)
{
  int i;

  initlock(&e1000_lock, "e1000");

  regs = xregs;

  // Reset the device
  regs[E1000_IMS] = 0; // disable interrupts
  regs[E1000_CTL] |= E1000_CTL_RST;
  regs[E1000_IMS] = 0; // redisable interrupts
  __sync_synchronize();

  // [E1000 14.5] Transmit initialization
  memset(tx_ring, 0, sizeof(tx_ring));
  for (i = 0; i < TX_RING_SIZE; i++) {
    tx_ring[i].status = E1000_TXD_STAT_DD;
    tx_mbufs[i] = 0;
  }
  regs[E1000_TDBAL] = (uint64) tx_ring;
  if(sizeof(tx_ring) % 128 != 0)
    panic("e1000");
  regs[E1000_TDLEN] = sizeof(tx_ring);
  regs[E1000_TDH] = regs[E1000_TDT] = 0;
  
  // [E1000 14.4] Receive initialization
  memset(rx_ring, 0, sizeof(rx_ring));
  for (i = 0; i < RX_RING_SIZE; i++) {
    rx_mbufs[i] = mbufalloc(0);
    if (!rx_mbufs[i])
      panic("e1000");
    rx_ring[i].addr = (uint64) rx_mbufs[i]->head;
  }
  regs[E1000_RDBAL] = (uint64) rx_ring;
  if(sizeof(rx_ring) % 128 != 0)
    panic("e1000");
  regs[E1000_RDH] = 0;
  regs[E1000_RDT] = RX_RING_SIZE - 1;
  regs[E1000_RDLEN] = sizeof(rx_ring);

  // filter by qemu's MAC address, 52:54:00:12:34:56
  regs[E1000_RA] = 0x12005452;
  regs[E1000_RA+1] = 0x5634 | (1<<31);
  // multicast table
  for (int i = 0; i < 4096/32; i++)
    regs[E1000_MTA + i] = 0;

  // transmitter control bits.
  regs[E1000_TCTL] = E1000_TCTL_EN |  // enable
    E1000_TCTL_PSP |                  // pad short packets
    (0x10 << E1000_TCTL_CT_SHIFT) |   // collision stuff
    (0x40 << E1000_TCTL_COLD_SHIFT);
  regs[E1000_TIPG] = 10 | (8<<10) | (6<<20); // inter-pkt gap

  // receiver control bits.
  regs[E1000_RCTL] = E1000_RCTL_EN | // enable receiver
    E1000_RCTL_BAM |                 // enable broadcast
    E1000_RCTL_SZ_2048 |             // 2048-byte rx buffers
    E1000_RCTL_SECRC;                // strip CRC
  
  // ask e1000 for receive interrupts.
  regs[E1000_RDTR] = 0; // interrupt after every received packet (no timer)
  regs[E1000_RADV] = 0; // interrupt after every packet (no timer)
  regs[E1000_IMS] = (1 << 7); // RXDW -- Receiver Descriptor Write Back
}

int
e1000_transmit(struct mbuf *m)
{
  acquire(&e1000_lock);

  // TDT 指向驱动应该填写的下一个发送描述符。
  uint32 index = regs[E1000_TDT];//读取网卡的 TDT 硬件寄存器
  struct tx_desc *desc = &tx_ring[index];

  // DD 没有置位，说明网卡还没有使用完这个描述符。
  // 此时不能覆盖它，否则会破坏尚未完成的发送操作。
  if ((desc->status & E1000_TXD_STAT_DD) == 0) {
    release(&e1000_lock);
    return -1;
  }

  // 网卡已经发送完这个位置原来的数据。
  // 因此现在可以释放原来由这个描述符引用的 mbuf。
  if (tx_mbufs[index] != 0) {
    mbuffree(tx_mbufs[index]);
    tx_mbufs[index] = 0;
  }

  // 让描述符指向当前要发送的数据包。
  desc->addr = (uint64)m->head;
  desc->length = m->len;

  // EOP：这个描述符包含数据包的最后一段。RS：发送完成后，请网卡把 DD 状态写回来。
  desc->cmd = E1000_TXD_CMD_EOP | E1000_TXD_CMD_RS;

  // 清除旧状态，表示这个描述符现在交给网卡处理。
  desc->status = 0;

  // 保存 mbuf，等网卡发送完成后再释放。
  tx_mbufs[index] = m;

  // 保证描述符内容先写入内存，再通知网卡。
  __sync_synchronize();

  // 推进发送环的尾指针，通知网卡有新包需要发送。
  regs[E1000_TDT] = (index + 1) % TX_RING_SIZE;

  release(&e1000_lock);
  return 0;
}

static void
e1000_recv(void)
{
  while (1)
  {
    acquire(&e1000_lock);

    // RDT 指向驱动最后处理完并归还给网卡的描述符。
    // 所以下一个可能包含新数据包的位置是 RDT + 1。
    uint32 index = (regs[E1000_RDT] + 1) % RX_RING_SIZE;
    struct rx_desc *desc = &rx_ring[index];

    // 没有 DD，说明网卡还没有在这个描述符中放入新数据。
    if ((desc->status & E1000_RXD_STAT_DD) == 0) {
      release(&e1000_lock);
      break;
    }

    // 取出网卡刚刚写入数据的旧 mbuf。
    struct mbuf *m = rx_mbufs[index];
    m->len = desc->length;

    // 当前 mbuf 将交给网络协议栈。
    // 因此必须给网卡准备一个新的空 mbuf。
    struct mbuf *new_m = mbufalloc(0);
    if (new_m == 0)
      panic("e1000_recv");

    rx_mbufs[index] = new_m;
    desc->addr = (uint64)new_m->head;

    // 清除完成状态，让网卡以后可以再次使用这个描述符。
    desc->status = 0;

    // 确保新缓冲区地址和状态已经写入描述符。
    __sync_synchronize();

    // 把这个描述符归还给网卡。
    regs[E1000_RDT] = index;

    release(&e1000_lock);

    // 交给 IP/ARP/UDP 网络协议栈处理
    net_rx(m);
  }
}

void
e1000_intr(void)
{
  // tell the e1000 we've seen this interrupt;
  // without this the e1000 won't raise any
  // further interrupts.
  regs[E1000_ICR] = 0xffffffff;

  e1000_recv();
}
