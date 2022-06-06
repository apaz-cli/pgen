
/* START OF AST ALLOCATOR */

#ifndef PGEN_ARENA_INCLUDED
#define PGEN_ARENA_INCLUDED
#include <limits.h>
#include <stdalign.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define MAL alignof(max_align_t)
#define ABSZ (PGEN_PAGESIZE * 1024)
#define NUM_ARENAS 256

#define PGEN_IGNORE_FLAG 0
#define PGEN_FREE_FLAG 1
#define PGEN_BUFALLOC_FLAG 2

#ifndef PGEN_PAGESIZE
#if !(defined(PAGESIZE) | defined(PAGE_SIZE))
#define PGEN_PAGESIZE 4096
#else
#if defined(PAGESIZE)
#define PGEN_PAGESIZE PAGESIZE
#else
#define PGEN_PAGESIZE PAGE_SIZE
#endif
#endif
#endif

#if (defined(__unix__) || (defined(__APPLE__) && defined(__MACH__)))
#if __STDC_VERSION__ >= 201112L
_Static_assert(ABSZ % PGEN_PAGESIZE == 0,
               "Buffer size must be a multiple of the page size.");
#endif

#include <sys/mman.h>
#include <unistd.h>
static inline char *_abufalloc(void) {
  char *b = (char *)mmap(NULL, ABSZ, PROT_READ | PROT_WRITE,
                         MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
  if (b == MAP_FAILED) {
    perror("mmap()");
    return NULL;
  }
  return b;
}
static inline void _abuffree(char *buf) { munmap(buf, ABSZ); }
#else
static inline char *_abufalloc(void) {
  char *b = (char *)malloc(ABSZ);
  if (!b) {
    perror("malloc()");
    return NULL;
  }
  return b;
}
#undef PGEN_BUFALLOC_FLAG
#define PGEN_BUFALLOC_FLAG PGEN_FREE_FLAG
static inline void _abuffree(char *buf) { free(buf); }
#endif

#if __STDC_VERSION__ >= 201112L
_Static_assert((MAL % 2) == 0, "Why would alignof(max_align_t) be odd? WTF?");
_Static_assert(ABSZ <= UINT32_MAX,
               "The arena buffer size must fit in uint32_t.");
#endif

static inline size_t pgen_align(size_t n, size_t align) {
  if (__builtin_popcount(align) == 1)
    return (n + align - 1) & -align;
  else
    return (n + align - (n % align));
}

typedef struct {
  char *buf;
  uint32_t cap;
  uint8_t freeflag;
} pgen_arena;

typedef struct {
  uint32_t arena_idx;
  uint32_t filled;
} pgen_allocator_rewind_t;

typedef struct {
  pgen_arena arenas[NUM_ARENAS];
  pgen_allocator_rewind_t rew;
} pgen_allocator;

typedef struct {
  pgen_allocator_rewind_t rewind;
  char *buf;
} pgen_allocator_ret_t;

static inline pgen_allocator pgen_allocator_new() {
  pgen_allocator alloc;
  alloc.rew.arena_idx = 0;
  alloc.rew.filled = 0;
  for (size_t i = 0; i < NUM_ARENAS; i++) {
    alloc.arenas[i].buf = NULL;
    alloc.arenas[i].cap = 0;
    alloc.arenas[i].freeflag = PGEN_IGNORE_FLAG;
  }
  alloc.arenas[0].buf = _abufalloc();
  return alloc;
}

static inline int pgen_allocator_launder(pgen_allocator *allocator,
                                         pgen_arena arena) {
  size_t aidx = allocator->rew.arena_idx + 1;
  if (aidx < NUM_ARENAS) {
    allocator->rew.arena_idx = aidx;
    allocator->arenas[aidx] = arena;
    return 1;
  } else {
    return 0;
  }
}

static inline void pgen_allocator_destroy(pgen_allocator *allocator) {
  for (size_t i = 0; i < NUM_ARENAS; i++) {
    pgen_arena a = allocator->arenas[i];
    if (a.buf) {
      if (a.freeflag == PGEN_FREE_FLAG)
        free(a.buf);
      else if (a.freeflag == PGEN_BUFALLOC_FLAG)
        _abuffree(a.buf);
      // Ignore PGEN_IGNORE_FLAG
    }
  }
}

#define PGEN_ALLOC_OF(allocator, type)                                         \
  pgen_alloc(allocator, sizeof(type), alignof(type))
static inline pgen_allocator_ret_t pgen_alloc(pgen_allocator *allocator,
                                              size_t n, size_t alignment) {

  pgen_allocator_ret_t ret;
  ret.rewind = allocator->rew;
  ret.buf = NULL;

  // Check if it will fit in the current arena
  // If it won't fit, use the next (empty) arena.
  // If it hasn't already been allocated, allocate it.
  int cont = 1;
  size_t bufcurrent = pgen_align(allocator->rew.filled, alignment);
  size_t bufnext = bufcurrent + n;
  do {
    size_t cap = allocator->arenas[allocator->rew.arena_idx].cap;
    // If we need a new arena
    if (bufnext > cap) {
      bufcurrent = 0;
      bufnext = n;
      // Make sure there's space
      if (allocator->rew.arena_idx + 1 >= NUM_ARENAS)
        return ret;

      // Allocate a new arena if necessary
      allocator->rew.arena_idx++;
      if (!allocator->arenas[allocator->rew.arena_idx].buf) {
        char *nb = _abufalloc();
        if (!nb)
          return ret;
        pgen_arena new_arena;
        new_arena.buf = nb;
        new_arena.cap = ABSZ;
        new_arena.freeflag = PGEN_BUFALLOC_FLAG;
        allocator->arenas[allocator->rew.arena_idx] = new_arena;
      }
    } else {
      cont = 0;
    }
  } while (cont);

  ret.buf = allocator->arenas[allocator->rew.arena_idx].buf + bufcurrent;
  allocator->rew.filled = bufnext; // idx already updated

  printf("Allocator: (%u, %u/%u)\n", allocator->rew.arena_idx,
         allocator->rew.filled,
         allocator->arenas[allocator->rew.arena_idx].cap);
  return ret;
}

#define PGEN_REWIND_START ((pgen_allocator_rewind_t){0, 0})
static inline void pgen_allocator_rewind(pgen_allocator *allocator,
                                         pgen_allocator_rewind_t to) {
  allocator->rew.arena_idx = to.arena_idx;
  allocator->rew.filled = to.filled;
}

int main(void) {

  alignas(128) char to_launder[1024 * 1024];

  
  pgen_allocator a = pgen_allocator_new();

  pgen_arena laundry;
  laundry.buf = to_launder;
  laundry.cap = 1024 * 1024 * 8;
  laundry.freeflag = PGEN_IGNORE_FLAG;
  pgen_allocator_launder(&a, laundry);

  while (1) {
    size_t allocsz = (ABSZ / 6) + 3;
    pgen_allocator_ret_t ret = pgen_alloc(&a, allocsz, alignof(max_align_t));
    if (!ret.buf)
      break;
  }
  pgen_allocator_destroy(&a);
}

#undef MAL
#undef ABSZ
#undef UNIXY
#endif /* PGEN_ARENA_INCLUDED */