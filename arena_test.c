#include <stdio.h>
#include <stdint.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

#define NUM_BFS 1280
char arena[4096 * 20];
uint64_t bfs[NUM_BFS];
_Static_assert(sizeof(unsigned long) == sizeof(uint64_t), "");
_Static_assert(ULONG_MAX == UINT64_MAX, "");

void init(void) {
    memset(bfs, ~(char)0, 1280);
}

char* alloc(void) {
    for (unsigned i = 0; i < NUM_BFS; i++) {
        if (bfs[i]) {
            int subidx = __builtin_ffsl((long)bfs[i]) - 1;
            bfs[i] ^= (1 << subidx);
            return arena + 64 * i + subidx;
        }
    }

    return NULL;
}

int main(void) {
    init();
    char* a = alloc();
    char* b = alloc();
    printf("%p\n%p\n%p\n", arena, a, b);
    return a == arena;
}
