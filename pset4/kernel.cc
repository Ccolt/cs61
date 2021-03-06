#include "kernel.hh"
#include "lib.hh"
#include "k-vmiter.hh"

// kernel.cc
//
//    This is the kernel.


// INITIAL PHYSICAL MEMORY LAYOUT
//
//  +-------------- Base Memory --------------+
//  v                                         v
// +-----+--------------------+----------------+--------------------+---------/
// |     | Kernel      Kernel |       :    I/O | App 1        App 1 | App 2
// |     | Code + Data  Stack |  ...  : Memory | Code + Data  Stack | Code ...
// +-----+--------------------+----------------+--------------------+---------/
// 0  0x40000              0x80000 0xA0000 0x100000             0x140000
//                                             ^
//                                             | \___ PROC_SIZE ___/
//                                      PROC_START_ADDR

#define PROC_SIZE 0x40000       // initial state only

proc ptable[NPROC];             // array of process descriptors
                                // Note that `ptable[0]` is never used.
proc* current;                  // pointer to currently executing proc

#define HZ 100                  // timer interrupt frequency (interrupts/sec)
static unsigned ticks;          // # timer interrupts so far


// Memory state
//    Information about physical page with address `pa` is stored in
//    `pages[pa / PAGESIZE]`. In the handout code, each `pages` entry
//    holds an *owner*, which is 0 for free pages and non-zero for
//    allocated pages. You can change this as you see fit.

pageinfo pages[NPAGES];


void __noreturn schedule();
void __noreturn run(proc* p);
void exception(regstate* regs);
uintptr_t syscall(regstate* regs);
void memshow();


// kernel(command)
//    Initialize the hardware and processes and start running. The `command`
//    string is an optional string passed from the boot loader.

static void process_setup(pid_t pid, int program_number);

void kernel(const char* command) {
    // clear memory that should be initialized to 0
    extern char kernel_edata[], kernel_end[];
    memset(kernel_edata, 0, kernel_end - kernel_edata);

    // initialize hardware
    init_hardware();

    console_clear();

    ticks = 1;
    init_timer(HZ);

    // initalize it with user-accessible mappings for all physical memory
    for (vmiter it(kernel_pagetable);
         it.va() < PROC_START_ADDR;
         it += PAGESIZE) {
        if(it.va() != (uintptr_t) console) {
          int r = it.map(it.va(), PTE_P | PTE_W);
          assert(r == 0);
        }
    }

    // set up process descriptors
    for (pid_t i = 0; i < NPROC; i++) {
        ptable[i].pid = i;
        ptable[i].state = P_FREE;
    }

    if (command && strcmp(command, "fork") == 0) {
        process_setup(1, 4);
    } else if (command && strcmp(command, "forkexit") == 0) {
        process_setup(1, 5);
    } else {
        for (pid_t i = 1; i <= 4; ++i) {
            process_setup(i, i - 1);
        }
    }

    // Switch to the first process using run()
    run(&ptable[1]);
}


// kalloc(sz)
//    Kernel memory allocator. Allocates `sz` contiguous bytes and
//    returns a pointer to the allocated memory, or `nullptr` on failure.
//    The returned memory is initialized to zero.
//
//    On WeensyOS, `kalloc` is a page-based allocator: if `sz > PAGESIZE`
//    the allocation fails; if `sz < PAGESIZE` it allocates a whole page
//    anyway.
//
//    The handout code returns the next allocatable free page it can find.
//    It marks allocated pages by setting `pages[pagenum].owner = -1`.
//    It never reuses pages or supports freeing memory (you'll change that).

static uintptr_t next_alloc_pa;

void* kalloc(size_t sz) {
    if (sz > PAGESIZE) {
        return nullptr;
    }

    for (size_t i = 0; i < MEMSIZE_PHYSICAL; i++) {
        uintptr_t pa = next_alloc_pa;
        next_alloc_pa = (next_alloc_pa + PAGESIZE) % MEMSIZE_PHYSICAL;

        if (allocatable_physical_address(pa)
            && pages[pa / PAGESIZE].owner == 0) {
            pages[pa / PAGESIZE].owner = -1;
            pages[pa / PAGESIZE].sharers = 0;
            memset((void*) pa, 0, PAGESIZE);
            return (void*) pa;
        }
    }
    return nullptr;
}


// kfree(ptr)
//    Free `ptr`, which must have been previously returned by `kalloc`.
//    If `ptr == nullptr` does nothing.
//
//    The handout code ignores `ptr` -- you'll change that.

void kfree(void* ptr) {
    uintptr_t pa = (uintptr_t) ptr;

    // check that `ptr` is page-aligned, and either nullptr or allocatable
    assert((pa & PAGEOFFMASK) == 0);
    assert(!ptr || allocatable_physical_address(pa));
    //assert(pages[pa / PAGESIZE].sharers == 0);
    
    // Clear memory to 0
    memset((void*) pa, 0, PAGESIZE);
    
    // Set owner to 0 to indicate page is freed
    pages[pa / PAGESIZE].owner = 0;
}


// proc_free(pid)
//    Free an entire process, given a process id, 'pid'

void proc_free(pid_t pid) {
    // Mark process as free
    ptable[pid].state = P_FREE;
   
    // Create vmiter object
    vmiter pit(ptable[pid].pagetable, 0);
    
    // Free all freeable process memory 
    for (pit; pit.va() < MEMSIZE_VIRTUAL; pit += PAGESIZE) {
        bool shared = (pages[pit.pa()/PAGESIZE].sharers > 0);
        if (pit.user() && !shared && pit.va() != (uintptr_t) console) {
            kfree((void*) pit.pa());
        }
        else if (pit.user() && shared && pit.va() != (uintptr_t) console) {
            pages[pit.pa()/PAGESIZE].sharers -= 1;
        }
        pit += PAGESIZE;
    }
      
    // Free page table pages
    for (ptiter it(ptable[pid].pagetable); 
         it.active(); 
         it.next()) {
        kfree(it.ptp());
    }

    // Free page table first page
    kfree(ptable[pid].pagetable);
}


// process_setup(pid, program_number)
//    Load application program `program_number` as process number `pid`.
//    This loads the application's code and data into memory, sets its
//    %rip and %rsp, gives it a stack page, and marks it as runnable.

void process_setup(pid_t pid, int program_number) {
    init_process(&ptable[pid], 0);

    // set up initial page table
    ptable[pid].pagetable = (x86_64_pagetable*) kalloc(PAGESIZE);

    vmiter pit(ptable[pid].pagetable);

    for (vmiter kit(kernel_pagetable);
         kit.va() < PROC_START_ADDR;
         kit += PAGESIZE) {
        int r = pit.map(kit.pa(), kit.perm());
        assert(r==0);
        pit += PAGESIZE;
    }

    // load the program
    program_loader loader(program_number);

    // allocate and map all memory
    for (loader.reset(); loader.size() != 0; ++loader) {
        for (uintptr_t a = round_down(loader.va(), PAGESIZE);
            a < loader.va() + loader.size();
            a += PAGESIZE) {
            uintptr_t pa = (uintptr_t) kalloc(PAGESIZE);
            if (!pa) {
                panic("Cannot allocate memory for process!");
            }
            pit.find(a);
            if (loader.writable()) {
                int r = pit.map(pa, PTE_P | PTE_W | PTE_U);
                assert(r==0);
            }
            else {
                int r = pit.map(pa, PTE_P | PTE_U);
                assert(r==0);
            }
        }
    }


    // copy instructions and data into place
    for (loader.reset(); loader.size() != 0; ++loader) {
        pit.find(loader.va());
        memset((void*) pit.pa(), 0, loader.size());
        memcpy((void*) pit.pa(), loader.data(), loader.data_size());
    }

    // mark entry point
    ptable[pid].regs.reg_rip = loader.entry();

    // allocate stack
    uintptr_t stack_addr = MEMSIZE_VIRTUAL - PAGESIZE;
    uintptr_t pa = (uintptr_t) kalloc(PAGESIZE);
    if (!pa) {
        panic("Cannot allocate stack memory for process!");
    }
    pit.find(stack_addr);
    int r = pit.map(pa, PTE_P | PTE_W | PTE_U);
    assert(r==0);

    ptable[pid].regs.reg_rsp = stack_addr + PAGESIZE;

    // mark process as runnable
    ptable[pid].state = P_RUNNABLE;
}



// exception(regs)
//    Exception handler (for interrupts, traps, and faults).
//
//    The register values from exception time are stored in `regs`.
//    The processor responds to an exception by saving application state on
//    the kernel's stack, then jumping to kernel assembly code (in
//    k-exception.S). That code saves more registers on the kernel's stack,
//    then calls exception().
//
//    Note that hardware interrupts are disabled when the kernel is running.

void exception(regstate* regs) {
    // Copy the saved registers into the `current` process descriptor.
    current->regs = *regs;

    // It can be useful to log events using `log_printf`.
    // Events logged this way are stored in the host's `log.txt` file.
    /*log_printf("proc %d: exception %d\n", current->pid, regs->reg_intno);*/

    // Show the current cursor location and memory state
    // (unless this is a kernel fault).
    console_show_cursor(cursorpos);
    if (regs->reg_intno != INT_PAGEFAULT || (regs->reg_err & PFERR_USER)) {
        memshow();
    }

    // If Control-C was typed, exit the virtual machine.
    check_keyboard();


    // Actually handle the exception.
    switch (regs->reg_intno) {

    case INT_SYSCALL:
        current->regs.reg_rax = syscall(regs);
        break;

    case INT_TIMER:
        ++ticks;
        schedule();
        break;                  /* will not be reached */

    case INT_PAGEFAULT: {
        // Analyze faulting address and access type.
        uintptr_t addr = rcr2();
        const char* operation = regs->reg_err & PFERR_WRITE
                ? "write" : "read";
        const char* problem = regs->reg_err & PFERR_PRESENT
                ? "protection problem" : "missing page";

        if (!(regs->reg_err & PFERR_USER)) {
            panic("Kernel page fault for %p (%s %s, rip=%p)!\n",
                  addr, operation, problem, regs->reg_rip);
        }
        console_printf(CPOS(24, 0), 0x0C00,
                       "Process %d page fault for %p (%s %s, rip=%p)!\n",
                       current->pid, addr, operation, problem, regs->reg_rip);
        current->state = P_BROKEN;
        break;
    }

    default:
        panic("Unexpected exception %d!\n", regs->reg_intno);

    }


    // Return to the current process (or run something else).
    if (current->state == P_RUNNABLE) {
        run(current);
    } else {
        schedule();
    }
}


// syscall(regs)
//    System call handler.
//
//    The register values from system call time are stored in `regs`.
//    The return value, if any, is returned to the user process in `%rax`.
//
//    Note that hardware interrupts are disabled when the kernel is running.

uintptr_t syscall(regstate* regs) {
    // Copy the saved registers into the `current` process descriptor.
    current->regs = *regs;

    // It can be useful to log events using `log_printf`.
    // Events logged this way are stored in the host's `log.txt` file.
    /*log_printf("proc %d: syscall %d\n", current->pid, regs->reg_rax);*/

    // Show the current cursor location and memory state
    // (unless this is a kernel fault).
    console_show_cursor(cursorpos);
    memshow();

    // If Control-C was typed, exit the virtual machine.
    check_keyboard();


    // Actually handle the exception.
    switch (regs->reg_rax) {

    case SYSCALL_PANIC:
        panic(nullptr);         // does not return

    case SYSCALL_GETPID:
        return current->pid;

    case SYSCALL_YIELD:
        current->regs.reg_rax = 0;
        schedule();             // does not return

    case SYSCALL_PAGE_ALLOC: {
        uintptr_t addr = current->regs.reg_rdi;
        if (addr > PROC_START_ADDR) {
          uintptr_t pa = (uintptr_t) kalloc(PAGESIZE);
          if(pa) {
            vmiter pit(current->pagetable);
            pit = pit.find(addr);
            int r = pit.map(pa, PTE_P | PTE_W | PTE_U);
            assert(r==0);
            return 0;
          }
          else {
            return -1;
          }
        } else {
            return -1;
        }
    }

    case SYSCALL_FORK: {
        pid_t pid = 0;
        for (pid_t i = 1; i < NPROC; i++) {
            if (ptable[i].state == P_FREE) {
              pid = i;
              break;
            }
        }
        if (pid == 0) {return -1;}

        // set up initial page table
        ptable[pid].pagetable = (x86_64_pagetable*) kalloc(PAGESIZE);
        if (!ptable[pid].pagetable) {
          kfree(ptable[pid].pagetable);
          return -1;}
        
        // Child iterator
        vmiter cit(ptable[pid].pagetable, 0);

        for (vmiter pit(current->pagetable, 0);
             pit.va() < MEMSIZE_VIRTUAL;
             pit += PAGESIZE) {
            assert(pit.va() == cit.va());
            
            // Allocate new page if writable
            if (pit.writable() && pit.user() && pit.va() != (uintptr_t) console) {
                uintptr_t pa = (uintptr_t) kalloc(PAGESIZE);
                if (!pa) {
                    proc_free(pid);
                    return -1;
                }
                int r = cit.map(pa, pit.perm());
                if (r) {
                    proc_free(pid);
                    return -1;
                }
                memcpy((void*) pa, pit.pa_ptr(), PAGESIZE);
            }

            // Share page if not writable
            else if (!pit.writable() && pit.user() && pit.va() != (uintptr_t) console){
                int r = cit.map(pit.pa(), pit.perm());
                if (r) {
                    proc_free(pid);
                    return -1;
                }
                pages[pit.pa() / PAGESIZE].sharers += 1; 
            }

            // Otherwise simply copy page table mappings
            else {
                int r = cit.map(pit.pa(), pit.perm());
                if (r) {
                    proc_free(pid);
                    return -1;
                }
            }
            cit += PAGESIZE;
        }
        
        // Modify return addresses
        ptable[pid].regs = current->regs;
        ptable[pid].regs.reg_rax = 0;
        ptable[pid].pid = pid;

        // Set child state to runnable
        ptable[pid].state = P_RUNNABLE;
        
        return pid;
    }

    case SYSCALL_EXIT: {
        proc_free(current->pid);
        current->regs.reg_rax = 0;
        schedule(); 
    } 

    default:
        panic("Unexpected system call %ld!\n", regs->reg_rax);

    }

    panic("Should not get here!\n");
}


// schedule
//    Pick the next process to run and then run it.
//    If there are no runnable processes, spins forever.

void schedule() {
    pid_t pid = current->pid;
    for (unsigned spins = 0; true; ++spins) {
        pid = (pid + 1) % NPROC;
        if (ptable[pid].state == P_RUNNABLE) {
            run(&ptable[pid]);
        }

        // If Control-C was typed, exit the virtual machine.
        check_keyboard();

        // If spinning forever, show the memviewer.
        if (spins % (1 << 12) == 0) {
            memshow();
        }
    }
}


// run(p)
//    Run process `p`. This involves setting `current = p` and calling
//    `exception_return` to restore its page table and registers.

void run(proc* p) {
    assert(p->state == P_RUNNABLE);
    current = p;

    // Check the process's current pagetable.
    check_pagetable(p->pagetable);

    // This function is defined in k-exception.S. It restores the process's
    // registers then jumps back to user mode.
    exception_return(p->pagetable, &p->regs);

    // should never get here
    while (1) {
    }
}


// memshow()
//    Draw a picture of memory (physical and virtual) on the CGA console.
//    Switches to a new process's virtual memory map every 0.25 sec.
//    Uses `console_memviewer()`, a function defined in `k-memviewer.cc`.

void memshow() {
    static unsigned last_ticks = 0;
    static int showing = 0;

    // switch to a new process every 0.25 sec
    if (last_ticks == 0 || ticks - last_ticks >= HZ / 2) {
        last_ticks = ticks;
        showing = (showing + 1) % NPROC;
    }

    proc* p = nullptr;
    for (int search = 0; !p && search < NPROC; ++search) {
        if (ptable[showing].state != P_FREE
            && ptable[showing].pagetable) {
            p = &ptable[showing];
        } else {
            showing = (showing + 1) % NPROC;
        }
    }

    extern void console_memviewer(proc* vmp);
    console_memviewer(p);
}
