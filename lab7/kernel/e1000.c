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
  //
  // Your code here.
  //
  // the mbuf contains an ethernet frame; program it into
  // the TX descriptor ring so that the e1000 sends it. Stash
  // a pointer so that it can be freed after sending.
  //
  //printf("e1000 transmit\n");
  acquire(&e1000_lock);
  // First ask the E1000 for the TX ring index at which it's expecting
  // the next packet, by reading the E1000_TDT control register.
  // 首先，通过读取E1000_TDT控制寄存器，向E1000询问其下一个数据包的TX环索引。
  uint32 index = regs[E1000_TDT];
  // Then check if the the ring is overflowing. If E1000_TXD_STAT_DD is
  // not set in the descriptor indexed by E1000_TDT, the E1000 hasn't
  // finished the corresponding previous transmission request, so return an error.
  // 然后检查环是否溢出。如果E1000_TDT索引的描述符中没有设置E1000_TXD_STAT_DD，则E1000没有完成相应的先前传输请求，因此返回错误。
  if ((tx_ring[index].status & E1000_TXD_STAT_DD) == 0) {
    release(&e1000_lock);
    return -1;
  }

  // Otherwise, use mbuffree() to free the last mbuf that was transmitted
  // from that descriptor (if there was one).
  // 否则，使用mbuffree()释放从该描述符传输的最后一个mbuf（如果有的话）。
  if (tx_mbufs[index]) {
    mbuffree(tx_mbufs[index]);
  }

  // 然后填写描述符。m->head指向内存中数据包的内容，m->len是数据包的长度。
  // 设置必要的cmd标志（参阅E1000手册中的第3.3节），并隐藏一个指向mbuf的指针，以便稍后释放。
  // m->head points to the packet's content in memory
  tx_ring[index].addr = (uint64)(m->head);
  //  and m->len is the packet length
  tx_ring[index].length = m->len;
  // Set the necessary cmd flags (look at Section 3.3 in the E1000 manual)
  tx_ring[index].cmd = E1000_TXD_CMD_RS | E1000_TXD_CMD_EOP;
  // and stash away a pointer to the mbuf for later freeing.
  tx_mbufs[index] = m;

  // update the ring position by adding one to E1000_TDT modulo TX_RING_SIZE.
  // 最后，通过在E1000_TDT模TX_RING_SIZE上加一来更新环位置。
  regs[E1000_TDT] = (index + 1) % TX_RING_SIZE;
  release(&e1000_lock);
  return 0;
}

static void
e1000_recv(void)
{
  //
  // Your code here.
  //
  // Check for packets that have arrived from the e1000
  // Create and deliver an mbuf for each packet (using net_rx()).
  //
  //printf("e2000 receive\n");
  for (;;) {
    // First ask the E1000 for the ring index at which the next waiting
    // received packet (if any) is located, by fetching the E1000_RDT
    // control register and adding one modulo RX_RING_SIZE.
    // 首先通过提取E1000_RDT控制寄存器并添加一个模块RX_RING_SIZE，询问E1000下一个等待接收分组（如果有）所在的环索引。
    uint32 index = regs[E1000_RDT];
    index = (index + 1) % RX_RING_SIZE;

    // Then check if a new packet is available by checking for the
    // E1000_RXD_STAT_DD bit in the status portion of the descriptor.
    // If not, stop.
    // 然后通过检查描述符状态部分中的E1000_RXD_STAT_DD位来检查新数据包是否可用。如果没有，停止。
    if ((rx_ring[index].status & E1000_RXD_STAT_DD) == 0) {
      return;
    }

    // 否则，将mbuf的m->len更新为描述符中报告的长度。使用net_rx()将mbuf传递到网络堆栈。
    // update the mbuf's m->len to the length reported in the descriptor.
    rx_mbufs[index]->len = rx_ring[index].length;
    // rx_mbufs[index]->head = (char*)rx_ring[index].addr;

    // Deliver the mbuf to the network stack using net_rx()
    net_rx(rx_mbufs[index]);
    // Then allocate a new mbuf using mbufalloc() to replace the one just
    // given to net_rx().
    // 然后使用mbufalloc()分配一个新的mbuf，以替换刚才传递给net_rx()函数的mbuf。
    // 将其数据指针(m->head)编程到描述符中。将描述符的状态位清除为零。
    struct mbuf* buf = mbufalloc(0);
    rx_mbufs[index] = buf;
    // Program its data pointer (m->head) into the descriptor.
    rx_ring[index].addr = (uint64)buf->head;
    // Clear the descriptor's status bits to zero
    rx_ring[index].status = 0;

    // Finally, update the E1000_RDT register to be the index of the
    // last ring descriptor processed.
    // 最后，将E1000_RDT寄存器更新为最后处理的环描述符的索引。
    regs[E1000_RDT] = index;
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
