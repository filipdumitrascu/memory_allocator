/* SPDX-License-Identifier: BSD-3-Clause */

#pragma once

#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <stdint.h>

#include "osmem.h"
#include "block_meta.h"

#define ALIGNMENT 8
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~(ALIGNMENT-1))
#define MMAP_THRESHOLD 131072
#define META_SIZE ALIGN(sizeof(struct block_meta))

extern void *head_block; // the head of the list
extern size_t mmap_cmp; // the landmark for mmap for malloc or calloc
extern int heap_prealloc; // if the heap prealloc is done

/**
 * Coalesces all free blocks.
 */
void coalesce_blocks(void);

/**
 * Returns the best-fit free block meta address
 * where the new block can be allocated.
 */
struct block_meta *find_best(struct block_meta **last, size_t size);

/**
 * Allocates the current block in the memory and returns its block
 * meta address.
 */
struct block_meta *allocate_block(struct block_meta* block, size_t size);

/**
 * Allocates only the difference between the input size and the current size
 * of the last block and returns its block meta address.
 */
struct block_meta *expand_last(struct block_meta *last, size_t size);

/**
 * Splits the current block, creating a new free one next to it
 * and returns the block meta address of the allocated one.
 */
void split_block(struct block_meta *block, size_t size);
