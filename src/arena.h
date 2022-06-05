
/* START OF AST ALLOCATOR */

#ifndef PGEN_ARENA_INCLUDED
#define PGEN_ARENA_INCLUDED
#include <limits.h>
#include <stdalign.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#define MAL alignof(max_align_t)
#define ABSZ (PGEN_PAGESIZE * 256)
#define NUM_ARENAS 1028

#if (defined(__unix__) || (defined(__APPLE__) && defined(__MACH__)))
#include <sys/mman.h>
#include <unistd.h>

#if !(defined(PAGESIZE) | defined(PAGE_SIZE))
#define PGEN_PAGESIZE 4096
#else
#if defined(PAGESIZE)
#define PGEN_PAGESIZE PAGESIZE
#else
#define PGEN_PAGESIZE PAGE_SIZE
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

static inline size_t _alignment_roundUp(size_t n) {
  if ((MAL % 2) == 0)
    return (n + MAL - 1) & -MAL;
  else
    return (n + MAL - (n % MAL));
}

typedef struct {
  uint current_arena_idx;
  uint current_filled;
  uint num_arenas;
  uint cap_arenas;
  char *arenas[NUM_ARENAS];
} pgen_allocator;

typedef struct {
  uint arena_idx;
  uint filled;
  char *buf;
} pgen_allocator_ret_t;

static inline pgen_allocator pgen_allocator_new() {
  size_t initial_cap = 64;
  pgen_allocator alloc;
  alloc.arenas[0] = _abufalloc();
  if (alloc.arenas[0])
    return alloc;
  alloc.current_arena_idx = 0;
  alloc.current_filled = 0;
  alloc.num_arenas = 1;
  alloc.cap_arenas = initial_cap;
  for (size_t i = 0; i < ABSZ; i += PGEN_PAGESIZE)
    *(alloc.arenas[0] + i) = 0;
  return alloc;
}

static inline void pgen_allocator_destroy(pgen_allocator *allocator) {
  for (size_t i = 0; i < allocator->num_arenas; i++)
    _abuffree(allocator->arenas[i]);
  allocator->current_arena_idx = UINT_MAX;
  allocator->current_filled = 0;
  allocator->num_arenas = 0;
  allocator->cap_arenas = 0;
}

static inline pgen_allocator_ret_t pgen_alloc(pgen_allocator *allocator,
                                              size_t n) {
  pgen_allocator_ret_t ret;
  ret.arena_idx = allocator->current_arena_idx;
  ret.filled = allocator->current_filled;

  return ret;
}

#define PGEN_ALLOC_OF(type)

int main(void) {
  printf("%zu\n", (size_t)(ABSZ * NUM_ARENAS));

  for (size_t i = 0; i < NUM_ARENAS; i++)
    pgen_allocator_new();

  puts("Done.");
  while (1)
    ;
}

#undef MAL
#undef ABSZ
#undef UNIXY
#endif /* PGEN_ARENA_INCLUDED */