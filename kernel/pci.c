//
// simple PCI-Express initialization, only
// works for qemu and its tulip card.
//

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"

void
pci_init()
{
  // we'll place the tulip registers at this address.
  // vm.c maps this range.
  uint64 tulip_regs = TULIP_REGS;

  // qemu -machine virt puts PCIe config space here.
  // vm.c maps this range.
  uint32  *ecam = (uint32 *) 0x30000000L;
  
  // look at each possible PCI device on bus 0.
  for(int dev = 0; dev < 32; dev++){
    int bus = 0;
    int func = 0;
    int offset = 0;
    uint32 off = (bus << 16) | (dev << 11) | (func << 8) | (offset);
    volatile uint32 *base = ecam + off; // configuration registers
    uint32 id = base[0];
    
    // 0019:1011 is a tulip 21143
    if(id == 0x00191011){
      // base[] points to the Tulip configuration registers
      // described in Section 3.1 of the 21143 manual.
      
      // command and status register.
      // bit 0 : I/O access enable
      // bit 1 : memory access enable
      // bit 2 : enable mastering
      base[1] = 7;
      __sync_synchronize();

      for(int i = 0; i < 6; i++){
        uint32 old = base[4+i];

        // writing all 1's to the BAR causes it to be
        // replaced with its size.
        base[4+i] = 0xffffffff;
        __sync_synchronize();

        base[4+i] = old;
      }

      // tell the tulip to reveal its 16 CSRs at
      // physical address 0x40000000.
      // base[5] is the CBMA (Section 3.1.2.6).
      base[5+0] = tulip_regs;

      tulip_init((uint64*)tulip_regs);
    }
  }
}
