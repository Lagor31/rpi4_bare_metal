#include "include/BootAllocator.h"
#include "include/BuddyAlloc.h"
#include "include/Console.h"
#include "include/Core.h"
#include "include/FrameBuffer.h"
#include "include/GIC.h"
#include "include/GPIO.h"
#include "include/IRQ.h"
#include "include/KernelHeapAllocator.h"
#include "include/List.h"
#include "include/MMU.h"
#include "include/Mem.h"
#include "include/RNG.h"
#include "include/RedFS.h"
#include "include/SMP.h"
#include "include/Spinlock.h"
#include "include/Stdlib.h"
#include "include/String.h"
#include "include/SystemTimer.h"
#include "include/Task.h"
#include "include/Uart.h"
#define BUDDY_ALLOC_IMPLEMENTATION

extern void* _boot_alloc_start;
extern void* _boot_alloc_end;

extern void* _heap_start;
extern void* _heap_end;

extern "C" void init_core();

#define BOOT_SIGNATURE 0xAA55

typedef struct PACKED {
  uint8_t head;
  uint8_t sector : 6;
  uint8_t cylinder_hi : 2;
  uint8_t cylinder_lo;
} chs_address;

typedef struct PACKED {
  uint8_t status;
  chs_address first_sector;
  uint8_t type;
  chs_address last_sector;
  uint32_t first_lba_sector;
  uint32_t num_sectors;
} partition_entry;

typedef struct PACKED {
  uint8_t bootCode[0x1BE];
  partition_entry partitions[4];
  uint16_t bootSignature;
} master_boot_record;

extern "C" void kernel_main() {
  Core::disableIRQ();

  BootAllocator boot_allocator = BootAllocator(
      (unsigned char*)&_boot_alloc_start, (unsigned char*)&_boot_alloc_end);

  GlobalKernelAlloc::setAllocator(&boot_allocator);

  // We can use new with the boot allocator
  DriverManager::init();
  GPIO* gpio = new GPIO();
  UART* uart = new UART(gpio);
  GIC400* gic = new GIC400();
  SystemTimer* timer = new SystemTimer();
  RNG* rng200 = new RNG();
  DriverManager::load(gpio);
  DriverManager::load(uart);
  DriverManager::load(gic);
  DriverManager::load(timer);
  DriverManager::load(rng200);
  DriverManager::startAll();

  Console::setKernelConsole(uart);
  Console::print("\n\n\n\n\n\n############################################\n");
  Console::print("Current EL: %u\n", Std::getCurrentEL());

  Console::print("BootAlloc Start: 0x%x BootAlloc end: 0x%x\n",
                 &_boot_alloc_start, &_boot_alloc_end);
  Console::print("Heap Start: 0x%x Heap end: 0x%x\n",
                 (unsigned char*)&_heap_start, (unsigned char*)&_heap_end);

  KernelHeapAllocator* kha = new KernelHeapAllocator(
      (unsigned char*)&_heap_start, (unsigned char*)&_heap_end);
  GlobalKernelAlloc::setAllocator(kha);
  Console::print("Free Mem Bytes: %d\n", GlobalKernelAlloc::freeSpace());
  Console::print("List of loaded drivers:\n");
  for (auto d : *DriverManager::getAll())
    Console::print("Driver %s\n", d->getName());

  initSchedLock();
  FBInit();

  Console::print("Timer init on core: %d\n", get_core());
  Console::print("############################################\n");

  for (int i = 0; i < NUM_CORES; ++i) Core::runningQLock[i] = new Spinlock();
  Core::sleepingQLock = new Spinlock();
  for (int p = 0; p < PRIORITIES; ++p)
    Core::runningQ[get_core()][p] = new ArrayList<Task*>();

  Core::sleepingQ = new ArrayList<Task*>();
  Task* idle = Task::createKernelTask((uint64_t)&idleTask);
  Core::runningQ[get_core()][idle->p]->add(idle);

  for (int i = 0; i < THREAD_N; ++i) {
    Task* t = Task::createKernelTask((uint64_t)&kernelTask);
    t->p = 19;
    Core::runningQ[get_core()][t->p]->add(t);
  }

  Task* screen = Task::createKernelTask((uint64_t)&screenTask);
  screen->p = 0;
  screen->pinToCore(get_core());
  Core::runningQ[get_core()][screen->p]->add(screen);
  current = new Task();

  /*   for (int i = 0; i < 10; ++i)
      Console::print("RNG%d=%d", i, rng200->getNumber()); */
  /* Task* topBar = Task::createKernelTask((uint64_t)&topBarTask);
  Core::runningQ[get_core()]->insert(topBar); */

  Core::start(1, &init_core);
  Core::start(2, &init_core);
  Core::start(3, &init_core);
  Console::print("All cores UP!\n");
  Core::enableIRQ();

  SystemTimer::WaitMicroT1(100000);

  _hang_forever();
}
