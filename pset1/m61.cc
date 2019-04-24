#define M61_DISABLE 1
#include "m61.hh"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <inttypes.h>
#include <assert.h>
#include <vector>
#include <algorithm>

/// Global stats struct to hold running statistics totals
m61_statistics gstats = {0, 0, 0, 0, 0, 0, 0, 0};

/// Unique identifiers for metadata and trailing data
size_t id = 1006199731171040;
char idc = 'Z';
int unfreed_id = 12345;

/// Global pointer to start of alloc linked list
metadata base_meta = {0,0,0,nullptr,0,nullptr,nullptr};

/// Vector for creating heavy hitters report
hh_vec hhvec;

/// overflowed(a, b)
///   Helper function to check for multiplication overflow
bool overflowed(size_t a, size_t b){
  size_t prod = a * b;
  return (prod / a != b);
}

/// was_freed(ptr)
///   Helper function to determine if a pointer has been freed
bool was_freed(metadata* ptr){
  return (ptr->unfreed != unfreed_id);
}

/// inrange(ptr, mptr)
///   Helper function to decide if a pointer points within an allocation
///   ptr --> pointer to check
///   mptr --> pointer to allocated metadata
bool inrange(void* ptr, metadata* mptr){
  void* endptr = (char*) mptr + sizeof(metadata) + mptr->size;
  return(ptr >= mptr && ptr <= endptr);
  }

/// line_before(h1, h2)
///   Comparison helper function for vector sorting by line number
bool line_before(hhstruct h1, hhstruct h2) {
  return (h1.file_line < h2.file_line);
}

/// size_bigger(h1, h2)
///   Comparison helper function for vector sorting by alloc size
bool size_bigger(hhstruct h1, hhstruct h2) {
  return (h1.size > h2.size);
}

/// m61_malloc(sz, file, line)
///    Return a pointer to `sz` bytes of newly-allocated dynamic memory.
///    The memory is not initialized. If `sz == 0`, then m61_malloc must
///    return a unique, newly-allocated pointer value. The allocation
///    request was at location `file`:`line`.

void* m61_malloc(size_t sz, const char* file, long line) {
    (void) file, (void) line;   // avoid uninitialized variable warnings
    
    // Create metadata struct
    metadata data = {};
    data.size = sz;
    data.id = id;
    data.unfreed = unfreed_id;
    data.file = (char*) file;
    data.line = line;

    // Check for size
    if(sz >= (size_t) -1 - 1024 - gstats.active_size - sizeof(metadata)){
      gstats.nfail++;
      gstats.fail_size += sz;
      return nullptr;
    }

    // Create pointer to overall allocation, including metadata
    metadata* metaptr = (metadata*) base_malloc(sz + sizeof(metadata) + sizeof(size_t) + 8);
    *metaptr = data;

    // Update alloc list pointers
    metaptr->next = base_meta.next;
    metaptr->last = &base_meta;
    if(base_meta.next != nullptr){
      (base_meta.next)->last = metaptr;
    }
    base_meta.next = metaptr;

    // Create pointer to payload, instead of metadata
    void* ptr = (void*) ((char*) metaptr + sizeof(metadata));

    // Create pointer to trailing byte, assign id to that pointer
    char* trailptr = (char*) ptr + sz;
    *trailptr = idc;

    // Update stats
    gstats.nactive++;
    gstats.active_size += sz;
    gstats.ntotal++;
    gstats.total_size += sz;
    if(((char*) ptr < gstats.heap_min) || gstats.heap_min == nullptr){
      gstats.heap_min = (char*) ptr;
    }
    if(((char*) ptr + sz > gstats.heap_max) || gstats.heap_min == nullptr){
      gstats.heap_max = (char*) ptr + sz;
    }

    // Update heavy hitters vector
    hhstruct hhdata =  {std::pair(file, line), sz};
    hhvec.push_back(hhdata);

    return(ptr);
}


/// m61_free(ptr, file, line)
///    Free the memory space pointed to by `ptr`, which must have been
///    returned by a previous call to m61_malloc. If `ptr == NULL`,
///    does nothing. The free was called at location `file`:`line`.

void m61_free(void* ptr, const char* file, long line) {
    (void) file, (void) line;   // avoid uninitialized variable warnings

    // Calculate pointer to metadata
    metadata* metaptr = (metadata*) ((char*) ptr - sizeof(metadata));

    // Check for nullptr
    if(ptr == nullptr){
      return;
    }
    
    // Check if pointer points to heap
    if(ptr < gstats.heap_min || ptr > gstats.heap_max){
      printf("MEMORY BUG %s:%li: invalid free of pointer %p, not in heap\n", file, line, ptr);
      return;
    }

    // Check for invalid pointer
    if((((uintptr_t) ptr & 7) != 0) || (metaptr->id != id)){
      printf("MEMORY BUG: %s:%li: invalid free of pointer %p, not allocated\n", file, line, ptr);
      
      // Check if pointer points inside existing allocation
      metadata* checkptr = base_meta.next;
      while(checkptr != nullptr){
        if(inrange(ptr, checkptr)){
          unsigned long offset = (char*) ptr - ((char*) checkptr + sizeof(metadata));
          printf("  %s:%i: %p is %lu bytes inside a %lu byte region allocated here\n", 
                 file, checkptr->line, ptr, offset, checkptr->size);
          break;
        }
        checkptr = checkptr->next;
      }

    return;
    }

    // Check for sneaky frees
    if(metaptr->next != nullptr){
      if(metaptr->next->last != metaptr){
        printf("MEMORY BUG: %s:%li: invalid free of pointer %p, not allocated\n", 
            file, line, ptr);
      }
    }
    if(metaptr->last != nullptr){
      if(metaptr->last->next != metaptr){
        printf("MEMORY BUG: %s:%li: invalid free of pointer %p, not allocated\n", 
            file, line, ptr);
      }
    }
      
    // Check for double free
    bool dbfreed = was_freed(metaptr);
    if(metaptr->id == id && dbfreed){
      printf("MEMORY BUG: %s:%li: invalid free of pointer %p\n", file, line, ptr);
      return;
    }

    // Check for corruption of trailing data
    char* trailptr = (char*) ptr + metaptr->size;
    if(*trailptr != idc){
      printf("MEMORY BUG: %s:%li: detected wild write during free of pointer %p\n",
          file, line, ptr);
      return;
    }

    // Update metadata linked list
    if(metaptr->next != nullptr){
      (metaptr->next)->last = metaptr->last;
    }
    if(metaptr->last != nullptr){
      (metaptr->last)->next = metaptr->next;
    }
    else{
      base_meta.next = metaptr->next;
    }

    // Update stats and wipe id from metadata
    gstats.active_size -= (metaptr->size);
    gstats.nactive--;
    metaptr->unfreed = 0;
    base_free(metaptr);
    
    return;
}


/// m61_calloc(nmemb, sz, file, line)
///    Return a pointer to newly-allocated dynamic memory big enough to
///    hold an array of `nmemb` elements of `sz` bytes each. If `sz == 0`,
///    then must return a unique, newly-allocated pointer value. Returned
///    memory should be initialized to zero. The allocation request was at
///    location `file`:`line`.

void* m61_calloc(size_t nmemb, size_t sz, const char* file, long line) {
   
    // Check for overflow
    if(overflowed(nmemb, sz)){
        gstats.nfail++;
        return nullptr;
    }

    void* ptr = m61_malloc(nmemb * sz, file, line);
    if (ptr != nullptr) {
        memset(ptr, 0, nmemb * sz);
    }
    return ptr;
}


/// m61_getstatistics(stats)
///    Store the current memory statistics in `*stats`.
void m61_getstatistics(m61_statistics* stats) {
    // Stub: set all statistics to enormous numbers
    memset(stats, 255, sizeof(m61_statistics));
    *stats = gstats;
}


/// m61_printstatistics()
///    Print the current memory statistics.

void m61_printstatistics() {
    m61_statistics stats;
    m61_getstatistics(&stats);

    printf("alloc count: active %10llu   total %10llu   fail %10llu\n",
           stats.nactive, stats.ntotal, stats.nfail);
    printf("alloc size:  active %10llu   total %10llu   fail %10llu\n",
           stats.active_size, stats.total_size, stats.fail_size);
}


/// m61_printleakreport()
///    Print a report of all currently-active allocated blocks of dynamic
///    memory.

void m61_printleakreport() {
  metadata* next = base_meta.next;
  while(next != nullptr){
    char* file  = next->file;
    long int line = next->line;
    long long int sz = next->size;
    printf("LEAK CHECK: %s:%li: allocated object %p with size %lli\n", 
           file, line, (metadata*) ((char*) next + sizeof(metadata)), sz);
    next = next->next; 
  }
  return;
}

/// hhreport()
///   Heavy hitter report prints a report of the lines of code allocating 
///   the most memory.

void hhreport() {
  
  // Sort vector by line number
  std::sort (hhvec.begin(), hhvec.end(), line_before);
  
  // Condense list into entries for each line number, with total size summed
  hh_vec condensed;
  condensed.push_back(hhvec[0]);
  for(unsigned int i = 1; i != hhvec.size(); i++) {
    if(hhvec[i].file_line == condensed.back().file_line) {
      condensed.back().size = condensed.back().size + hhvec[i].size;
    }
    else {
      condensed.push_back(hhvec[i]);
    }
  }
  // Sort list by size
  std::sort (condensed.begin(), condensed.end(), size_bigger);

  // Get and print top 5, if over 20%
  // use stats.total_size
  for(int i=5; i!=0; i--) {
    float percent = gstats.total_size / condensed[i].size;
    if (percent >= 20) {
      printf("HEAVY HITTER: %s:%li: %lu bytes (~%.3f\%)\n", 
             std::get<const char*>(condensed[i].file_line), 
             std::get<long int>(condensed[i].file_line), 
             condensed[i].size, 
             percent);
    }
  }
return;
}

thread_local const char* m61_file = "?";
thread_local int m61_line = 1;

void* operator new(size_t sz) {
    return m61_malloc(sz, m61_file, m61_line);
}
void* operator new[](size_t sz) {
    return m61_malloc(sz, m61_file, m61_line);
}
void operator delete(void* ptr) noexcept {
    m61_free(ptr, m61_file, m61_line);
}
void operator delete(void* ptr, size_t) noexcept {
    m61_free(ptr, m61_file, m61_line);
}
void operator delete[](void* ptr) noexcept {
    m61_free(ptr, m61_file, m61_line);
}
void operator delete[](void* ptr, size_t) noexcept {
    m61_free(ptr, m61_file, m61_line);
}
