#ifndef DEBUG_MEMORY_H
#define DEBUG_MEMORY_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct BlockInfo {
    void *address;
    size_t size;
    const char *file;
    int line;
    const char *func;
    struct BlockInfo *next;
} BlockInfo;

static BlockInfo *allocated_blocks = NULL;

static void add_block(void *ptr, size_t size, const char *file, int line, const char *func) {
    BlockInfo *new_block = (BlockInfo*)malloc(sizeof(BlockInfo));
    new_block->address = ptr;
    new_block->size = size;
    new_block->file = file;
    new_block->line = line;
    new_block->func = func;
    new_block->next = allocated_blocks;
    allocated_blocks = new_block;
}

static void remove_block(void *ptr) {
    BlockInfo **current = &allocated_blocks;
    while (*current) {
        if ((*current)->address == ptr) {
            BlockInfo *to_free = *current;
            *current = (*current)->next;
            free(to_free);
            break;
        }
        current = &((*current)->next);
    }
}

static int is_allocated(void *ptr) {
    BlockInfo *current = allocated_blocks;
    while (current) {
        if (current->address == ptr) {
            return 1;
        }
        current = current->next;
    }
    return 0;
}

void* debug_malloc(size_t size, const char *file, int line, const char *func) {
    void *ptr = malloc(size);
    if (ptr) {
        if (size != 0) {
            add_block(ptr, size, file, line, func);
        }
        printf("malloc: allocated %zu bytes at %p (file: %s, line: %d, func: %s)\n", size, ptr, file, line, func);
    } else {
        printf("malloc: failed to allocate %zu bytes (file: %s, line: %d, func: %s)\n", size, file, line, func);
    }
    return ptr;
}

void debug_free(void *ptr, const char *file, int line, const char *func) {
    if (ptr) {
        if (!is_allocated(ptr)) {
            printf("free warning: attempt to free unallocated or already freed memory at %p (file: %s, line: %d, func: %s)\n", ptr, file, line, func);
            return;
        }
        remove_block(ptr);
        printf("free: deallocated memory at %p (file: %s, line: %d, func: %s)\n", ptr, file, line, func);
        free(ptr);
    } else {
        printf("free warning: attempt to deallocate null pointer (file: %s, line: %d, func: %s)\n", file, line, func);
    }
}

void* debug_realloc(void *ptr, size_t size, const char *file, int line, const char *func) {
    if (ptr && size == 0) {
        printf("realloc: freeing memory at %p (file: %s, line: %d, func: %s)\n", ptr, file, line, func);
        debug_free(ptr, file, line, func);
        return NULL;
    }
    if (!ptr && size == 0) {
        printf("realloc warning: reallocating null pointer with zero size (file: %s, line: %d, func: %s)\n", file, line, func);
    }
    void *new_ptr = realloc(ptr, size);
    if (new_ptr) {
        if (ptr) {
            remove_block(ptr);
        }
        if (size != 0) {
            add_block(new_ptr, size, file, line, func);
        }
        printf("realloc: reallocated %zu bytes at %p (file: %s, line: %d, func: %s)\n", size, new_ptr, file, line, func);
    } else {
        printf("realloc: failed to reallocate %zu bytes (file: %s, line: %d, func: %s)\n", size, file, line, func);
    }
    return new_ptr;
}

void* debug_calloc(size_t nmemb, size_t size, const char *file, int line, const char *func) {
    if (nmemb == 0 || size == 0) {
        printf("calloc warning: zero size allocation (file: %s, line: %d, func: %s)\n", file, line, func);
    }
    void *ptr = calloc(nmemb, size);
    if (ptr) {
        if (nmemb != 0 && size != 0) {
            add_block(ptr, nmemb * size, file, line, func);
        }
        printf("calloc: allocated %zu bytes at %p (file: %s, line: %d, func: %s)\n", nmemb * size, ptr, file, line, func);
    } else {
        printf("calloc: failed to allocate %zu bytes (file: %s, line: %d, func: %s)\n", nmemb * size, file, line, func);
    }
    return ptr;
}

void view_heap_status() {
    if (allocated_blocks == NULL) {
        printf("Heap status: No allocated memory.\n");
        return;
    }

    BlockInfo *current = allocated_blocks;
    printf("Heap status:\n");
    while (current) {
        printf("  - %zu bytes allocated at %p (file: %s, line: %d, func: %s)\n",
               current->size, current->address, current->file, current->line, current->func);
        current = current->next;
    }
}

#define malloc(size) debug_malloc(size, __FILE__, __LINE__, __func__)
#define free(ptr) debug_free(ptr, __FILE__, __LINE__, __func__)
#define realloc(ptr, size) debug_realloc(ptr, size, __FILE__, __LINE__, __func__)
#define calloc(nmemb, size) debug_calloc(nmemb, size, __FILE__, __LINE__, __func__)

#endif // DEBUG_MEMORY_H

