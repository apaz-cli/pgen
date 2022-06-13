
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
static inline char *_pgen_abufalloc(void) {
  char *b = (char *)mmap(NULL, ABSZ, PROT_READ | PROT_WRITE,
                         MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
  if (b == MAP_FAILED) {
    perror("mmap()");
    return NULL;
  }
  return b;
}
static inline void _pgen_abuffree(void *buf) {
  if (munmap(buf, ABSZ) == -1)
    perror("munmap()");
}
#else
static inline char *_pgen_abufalloc(void) {
  char *b = (char *)malloc(ABSZ);
  if (!b) {
    perror("malloc()");
    return NULL;
  }
  return b;
}
#define _pgen_abuffree free
#endif

#if __STDC_VERSION__ >= 201112L
_Static_assert((MAL % 2) == 0, "Why would alignof(max_align_t) be odd? WTF?");
_Static_assert(ABSZ <= UINT32_MAX,
               "The arena buffer size must fit in uint32_t.");
#endif

static inline size_t pgen_align(size_t n, size_t align) {
  return (n + align - (n % align));
}

typedef struct {
  void (*freefn)(void *ptr);
  char *buf;
  uint32_t cap;
} pgen_arena;

typedef struct {
  uint32_t arena_idx;
  uint32_t filled;
} pgen_allocator_rewind_t;

typedef struct {
  pgen_allocator_rewind_t rew;
  pgen_arena arenas[NUM_ARENAS];
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
    alloc.arenas[i].freefn = NULL;
    alloc.arenas[i].buf = NULL;
    alloc.arenas[i].cap = 0;
  }
  return alloc;
}

static inline int pgen_allocator_launder(pgen_allocator *allocator,
                                         pgen_arena arena) {
  for (size_t i = 0; i < NUM_ARENAS; i++) {
    if (!allocator->arenas[i].buf) {
      allocator->arenas[i] = arena;
      return 1;
    }
  }
  return 0;
}

static inline void pgen_allocator_destroy(pgen_allocator *allocator) {
  for (size_t i = 0; i < NUM_ARENAS; i++) {
    pgen_arena a = allocator->arenas[i];
    if (a.freefn)
      a.freefn(a.buf);
  }
}

#define PGEN_ALLOC_OF(allocator, type)                                         \
  pgen_alloc(allocator, sizeof(type), alignof(type))
static inline pgen_allocator_ret_t pgen_alloc(pgen_allocator *allocator,
                                              size_t n, size_t alignment) {

  pgen_allocator_ret_t ret;
  ret.rewind = allocator->rew;
  ret.buf = NULL;

  // Find the arena to allocate on and where we are inside it.
  size_t bufcurrent = pgen_align(allocator->rew.filled, alignment);
  size_t bufnext = bufcurrent + n;
  while (1) {
    // If we need a new arena
    if (bufnext > allocator->arenas[allocator->rew.arena_idx].cap) {
      bufcurrent = 0;
      bufnext = n;

      // Make sure there's a spot for it
      if (allocator->rew.arena_idx + 1 >= NUM_ARENAS)
        return ret;

      // Allocate a new arena if necessary
      if (allocator->arenas[allocator->rew.arena_idx].buf)
        allocator->rew.arena_idx++;
      if (!allocator->arenas[allocator->rew.arena_idx].buf) {
        char *nb = _pgen_abufalloc();
        if (!nb)
          return ret;
        pgen_arena new_arena;
        new_arena.freefn = _pgen_abuffree;
        new_arena.buf = nb;
        new_arena.cap = ABSZ;
        allocator->arenas[allocator->rew.arena_idx] = new_arena;
      }
    } else {
      break;
    }
  }

  ret.buf = allocator->arenas[allocator->rew.arena_idx].buf + bufcurrent;
  allocator->rew.filled = bufnext;

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

#endif /* PGEN_ARENA_INCLUDED */