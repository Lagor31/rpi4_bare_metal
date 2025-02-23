#include "../include/KernelHeapAllocator.h"

#include <stdint.h>

#include "../include/BuddyAlloc.h"
#include "../include/Console.h"
#include "../include/Core.h"
#include "../include/SystemTimer.h"

uint64_t allocations = 0;
KernelHeapAllocator::KernelHeapAllocator(unsigned char *s, unsigned char *e) {
  ptr = s;
  bytes_left = (long)e - (long)s;
  l.l.val = SPLCK_UNLOCKED;

  uint64_t arena_size = (uint64_t)e - (uint64_t)s;
  /* You need space for the metadata and for the arena */
  void *buddy_metadata = GlobalKernelAlloc::alloc(buddy_sizeof(arena_size));
  void *buddy_arena = s;
  buddy = buddy_init((unsigned char *)buddy_metadata,
                     (unsigned char *)buddy_arena, arena_size);
};

unsigned long KernelHeapAllocator::freeSpace() {
  return buddy_arena_free_size(buddy);
}

void *KernelHeapAllocator::alloc(unsigned size) {
  // if (size > bytes_left) Core::panic("FInished memory in simple
  // allocator!\n");

  l.spin();
  void *out;
  out = buddy_malloc(buddy, size);
  l.release();

  if (out == NULL) {
    Console::print_no_lock("ERROR - Allocations: %d", allocations);
    Console::print_no_lock("\n\n");
    for (int i = 0; i < NUM_CORES; ++i) {
      Console::print_no_lock("#################\nCore%d\n", i);
      for (int p = 0; p < PRIORITIES; ++p) {
        if (Core::runningQ[i][p]->getSize() > 0)
          Console::print_no_lock("RunninQ[%d] Core%d: %d\n", p, i,
                                 Core::runningQ[i][p]->getSize());
      }
    }
    Console::print_no_lock("SleepingQ: %d\n\n", Core::sleepingQ->getSize());
    Console::print_no_lock("\n\n");
    rpi_sys_timer_t *timer = SystemTimer::getTimer();
    Console::print_no_lock("System Timer Counter: %x\n",
                           SystemTimer::getCounter());
    Console::print_no_lock("System Timer Lo: %x\n", timer->counter_lo);
    Console::print_no_lock("System Timer Hi: %x\n", timer->counter_hi);

    Console::print_no_lock("System Timer Compare0: %x\n", timer->compare0);
    Console::print_no_lock("Allocations: %d\n", allocations);
    Console::print_no_lock("\n\n");
    Core::panic("Buddy out of memory!\n");
  }
  bytes_left -= size;
  /* Console::print("Alloc out=0x%x ptr=%x for size=%d ESize=0x%x\n", out,
     ptr, size, effectiveSize); */
  allocations++;
  return out;
};
void KernelHeapAllocator::free(void *p) {
  l.spin();
  buddy_free(buddy, p);
  allocations--;

  l.release();
};
void KernelHeapAllocator::addChunk(void *, void *) { return; };
