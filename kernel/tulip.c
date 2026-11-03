//
// A driver for qemu's "tulip" DEC 21143 NIC.
//
// see: Digital Semiconductor 21143 PCI/CardBus 10/100-Mb/s Ethernet
//      LAN Controller Hardware Reference Manual
//

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"

#define Reg(reg)         ((volatile uint32 *)(TULIP_REGS + (reg)))
#define ReadReg(reg)     (*(Reg(reg)))
#define WriteReg(reg, v) (*(Reg(reg)) = (v))

#define RDES0_OWN (1 << 31)  // owned by 21143
#define RDES0_FL  0x3fff0000 // frame length
#define RDES0_ES  (1 << 15)  // error summary
#define RDES1_RER (1 << 25)  // receive end of ring
#define TDES0_OWN (1 << 31)  // owned by 21143
#define TDES0_ES  (1 << 15)  // error summary
#define TDES1_IC  (1 << 31)  // interrupt on completion
#define TDES1_LS  (1 << 30)  // last segment
#define TDES1_FS  (1 << 29)  // first segment
#define TDES1_TER (1 << 25)  // transmit end of ring

#define CSR0     0x0
#define CSR1     0x08
#define CSR2     0x10
#define CSR3     0x18
#define CSR4     0x20
#define CSR5     0x28
#define CSR6     0x30
#define CSR6_SR  (1 << 1)  // start/stop receive
#define CSR6_PR  (1 << 6)  // promiscuous mode
#define CSR6_ST  (1 << 13) // start/stop transmission command
#define CSR6_MBO (1 << 25) // must be one
#define CSR7     0x38
#define CSR7_TIE (1 << 0)  // transmit interrupt enable
#define CSR7_RIE (1 << 6)  // receive interrupt enable
#define CSR7_RUE (1 << 7)  // receive buffer unavailable enable
#define CSR7_AIE (1 << 15) // abnormal interrupt summary enable
#define CSR7_NIE (1 << 16) // normal interrupt enable
#define CSR8     0x40

struct rxdes {
  uint32 rdes0;
  uint32 rdes1;
  uint32 rdes2;
  uint32 rdes3;
};

struct txdes {
  uint32 tdes0;
  uint32 tdes1;
  uint32 tdes2;
  uint32 tdes3;
};

struct spinlock lock;
#define RING_SIZE 4
struct rxdes rxring[RING_SIZE];
struct txdes txring[RING_SIZE];
uint64 txbufs[RING_SIZE];
uint rx_index;
uint tx_index;

//
// called by pci_init().
// csrs is the memory address at which the
// tulip's 16 command and status registers (CSRs) are mapped.
//
void
tulip_init(volatile uint64 *csrs)
{
  initlock(&lock, "tulip");

  for (int i = 0; i < NELEM(rxring); ++i) {
    void *buf = kalloc();
    if (!buf)
      panic("tulip_init");
    rxring[i].rdes2 = (uint32)(uint64)buf;
    rxring[i].rdes1 = 1020;
    if (i == RING_SIZE - 1)
      rxring[i].rdes1 |= RDES1_RER;
    __sync_synchronize();
    rxring[i].rdes0 = RDES0_OWN;
  }

  // receive list base address
  WriteReg(CSR3, (uint32)(uint64)rxring);

  // transmit list base address
  WriteReg(CSR4, (uint32)(uint64)txring);

  // start/stop receive
  // promiscuous mode
  // start/stop transmission control
  WriteReg(CSR6, CSR6_MBO | CSR6_ST | CSR6_PR | CSR6_SR);

  // enable normal, receive, and transmit interrupts
  WriteReg(CSR7, CSR7_NIE | CSR7_AIE | CSR7_RUE | CSR7_RIE | CSR7_TIE);
}


//
// buf contains an ethernet frame.
// tulip_xmit() should ask the tulip NIC to transmit the packet.
// the tulip driver should kfree(buf) when it's done.
//
void
tulip_xmit(char *buf, int len)
{
  acquire(&lock);

  struct txdes *des = &txring[tx_index];

  if (des->tdes0 & TDES0_OWN) {
    release(&lock);
    return;
  }

  if (txbufs[tx_index])
    kfree((void *)txbufs[tx_index]);
  txbufs[tx_index] = (uint64)buf;

  des->tdes2 = (uint32)(uint64)buf;
  des->tdes1 = TDES1_IC | TDES1_LS | TDES1_FS | len;
  if (tx_index == RING_SIZE - 1)
    des->tdes1 |= TDES1_TER;
  __sync_synchronize();
  des->tdes0 = TDES0_OWN;

  tx_index = (tx_index + 1) % RING_SIZE;

  release(&lock);

  // transmit poll demand
  WriteReg(CSR1, 1);
}

void
tulipintr(void)
{
  // uint32 status = ReadReg(CSR5);
  WriteReg(CSR5, ~(uint32)0); // clear bits

  while (1) {
    acquire(&lock);
    if (rxring[rx_index].rdes0 & RDES0_OWN) {
      release(&lock);
      break;
    }

    int len = 0;
    char *buf = 0;
    if ((rxring[rx_index].rdes0 & RDES0_ES) == 0) {
      len = (rxring[rx_index].rdes0 & RDES0_FL) >> 16;
      buf = (char *)(uint64)rxring[rx_index].rdes2;
      char *newbuf = kalloc();
      if (!newbuf) {
        newbuf = buf;
        buf = 0;
      }
      rxring[rx_index].rdes2 = (uint32)(uint64)newbuf;
      __sync_synchronize();
    }

    rxring[rx_index].rdes0 = RDES0_OWN;
    rx_index = (rx_index + 1) % RING_SIZE;
    release(&lock);

    if (buf)
      net_rx(buf, len-4);
  }

  // receive poll demand
  WriteReg(CSR2, 1);
}
