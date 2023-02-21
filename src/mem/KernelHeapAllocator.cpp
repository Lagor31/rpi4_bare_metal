#include "../include/KernelHeapAllocator.h"

#include <stdint.h>

#include "../include/Console.h"
#include "../include/Core.h"
#include "../include/buddy_alloc.h"
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
  return buddy_arena_size(buddy);
}

void *KernelHeapAllocator::alloc(unsigned size) {
  // if (size > bytes_left) Core::panic("FInished memory in simple
  // allocator!\n");

  l.getLock();
  void *out;
  out = buddy_malloc(buddy, size);
  l.release();

  if (out == NULL) Core::panic("Buddy out of memory!\n");
  bytes_left -= size;
  /* Console::print("Alloc out=0x%x ptr=%x for size=%d ESize=0x%x\n", out,
     ptr, size, effectiveSize); */

  return out;
};
void KernelHeapAllocator::free(void *p) {
  l.getLock();
  buddy_free(buddy, p);
  l.release();
};
void KernelHeapAllocator::addChunk(void *, void *) { return; };
