
/* START OF AST ALLOCATOR */

#ifndef PGEN_ARENA_INCLUDED
#define PGEN_ARENA_INCLUDED
#include <limits.h>
#include <stdalign.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#define MAL alignof(max_align_t)
#define ABSZ (PGEN_PAGESIZE * 1024)
#define NUM_ARENAS 256
// Can hold 1 GiB total. That's ~1.08 GB.

#if (defined(__unix__) || (defined(__APPLE__) && defined(__MACH__)))
#include <sys/mman.h>
#include <unistd.h>

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

static inline char *_abufalloc(void) {
#if __STDC_VERSION__ >= 201112L
  _Static_assert(ABSZ % PGEN_PAGESIZE == 0,
                 "Buffer size must be a multiple of the page size.");
#endif
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
static inline void _abuffree(char *buf) {}
#endif

#if __STDC_VERSION__ >= 201112L
_Static_assert((MAL % 2) == 0, "Why would alignof(max_align_t) be odd? WTF?");
_Static_assert(ABSZ <= UINT_MAX, "The arena buffer size must fit in uint.");
#endif

static inline size_t pgen_align(size_t n, size_t align) {
  if (__builtin_popcount(align) == 1)
    return (n + align - 1) & -align;
  else
    return (n + align - (n % align));
}

typedef struct {
  uint arena_idx;
  uint filled;
  char *arenas[NUM_ARENAS];
} pgen_allocator;

typedef struct {
  uint arena_idx;
  uint filled;
} pgen_allocator_rewind_t;

typedef struct {
  pgen_allocator_rewind_t rewind;
  char *buf;
} pgen_allocator_ret_t;

static inline pgen_allocator pgen_allocator_new() {
  pgen_allocator alloc;
  alloc.arena_idx = 0;
  alloc.filled = 0;
  for (size_t i = 0; i < NUM_ARENAS; i++)
    alloc.arenas[i] = NULL;
  alloc.arenas[0] = _abufalloc();
  return alloc;
}

static inline void pgen_allocator_destroy(pgen_allocator *allocator) {
  for (size_t i = 0; i < NUM_ARENAS; i++)
    if (allocator->arenas[i])
      _abuffree(allocator->arenas[i]);
    else
      break;
}

#define PGEN_ALLOC_OF(allocator, type)                                         \
  pgen_alloc(allocator, sizeof(type), alignof(type))
static inline pgen_allocator_ret_t pgen_alloc(pgen_allocator *allocator,
                                              size_t n, size_t alignment) {
  pgen_allocator_rewind_t rew;
  rew.arena_idx = allocator->arena_idx;
  rew.filled = allocator->filled;
  pgen_allocator_ret_t ret;
  ret.rewind = rew;
  ret.buf = NULL;

  if (n > ABSZ) {
    return ret;
  }

  // Check if it will fit in the current arena
  // If it won't fit, use the next (empty) arena.
  // If it hasn't already been allocated, allocate it.
  size_t bufcurrent = pgen_align(allocator->filled, alignment);
  size_t bufnext = bufcurrent + n;
  if (bufnext > ABSZ) {
    bufcurrent = 0; // already aligned
    bufnext = n;
    allocator->arena_idx += 1;
    if (allocator->arena_idx == NUM_ARENAS)
      return ret;
    if (!allocator->arenas[allocator->arena_idx]) {
      char *nb = _abufalloc();
      if (!nb)
        return ret;
      allocator->arenas[allocator->arena_idx] = nb;
    }
  }

  ret.buf = allocator->arenas[allocator->arena_idx] + bufcurrent;
  allocator->filled = bufnext;
  return ret;
}

#define PGEN_REWIND_START ((pgen_allocator_rewind_t){0, 0})
static inline void pgen_allocator_rewind(pgen_allocator *allocator,
                                         pgen_allocator_rewind_t to) {
  allocator->arena_idx = to.arena_idx;
  allocator->filled = to.filled;
}

int main(void) {
  size_t allocsz = (ABSZ / 5) + 3;

  pgen_allocator a = pgen_allocator_new();
  while (1) {
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