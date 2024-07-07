// SPDX-License-Identifier: BSD-3-Clause

#include "helpers.h"

int heap_prealloc;

struct block_meta *allocate_block(struct block_meta *last, size_t size)
{
	struct block_meta *new_block = NULL;

	if (META_SIZE + size >= mmap_cmp) {
		new_block = mmap(NULL, size + META_SIZE, PROT_READ |
						 PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
		DIE(new_block == MAP_FAILED, "mmap syscall failed");

		new_block->status = STATUS_MAPPED;
		new_block->next = NULL;

	} else {
		if (!heap_prealloc) {
			new_block = sbrk(MMAP_THRESHOLD);
			DIE(new_block == (void *) -1, "sbrk syscall failed");
			heap_prealloc = 1;

			/**
			 * If in the preallocation is space for the following block,
			 * allocates it there and splits the rest.
			 */
			if (META_SIZE + size + META_SIZE + 8 <= MMAP_THRESHOLD) {
				new_block->size = MMAP_THRESHOLD;
				split_block(new_block, size);
			}

		} else {
			new_block = sbrk(size + META_SIZE);
			DIE(new_block == (void *) -1, "sbrk syscall failed");
			new_block->next = NULL;
		}

		new_block->status = STATUS_ALLOC;
	}

	if (last) {
		last->next = new_block;
		new_block->prev = last;

	} else {
		new_block->prev = NULL;
	}

	new_block->size = size;

	return new_block;
}

void coalesce_blocks(void)
{
	struct block_meta *first = head_block;

	/**
	 * Block by block, if two adjacent ones are free, search for more
	 * free blocks and coalesce them
	 */
	while (first && first->next) {
		if ((first->status || first->next->status) == STATUS_FREE) {
			struct block_meta *last = first->next;

			for (; last && last->status == STATUS_FREE; last = last->next)
				first->size += META_SIZE + last->size;

			first->next = last;

			if (last)
				last->prev = first;

			first = last;

		} else {
			first = first->next;
		}
	}
}

struct block_meta *find_best(struct block_meta **last, size_t size)
{
	struct block_meta *current = *last, *best = NULL;
	size_t best_size = SIZE_MAX;

	/**
	 * Search for the smallest free block
	 * where the input size can be placed.
	 */
	for (; current; current = current->next) {
		*last = current;
		if (current->status == STATUS_FREE && current->size >= size) {
			if (current->size < best_size) {
				best_size = current->size;
				best = current;
			}
		}
	}

	return best;
}

struct block_meta *expand_last(struct block_meta *last, size_t size)
{
	/**
	 * Increases the size of the last block on the heap
	 */
	if (size >= MMAP_THRESHOLD) {
		last = mmap(NULL, size + META_SIZE, PROT_READ |
					PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
		DIE(last == MAP_FAILED, "mmap syscall failed");
		last->status = STATUS_MAPPED;

	} else {
		void *ok = sbrk(size - last->size);

		DIE(ok == (void *) -1, "sbrk syscall failed");
		last->status = STATUS_ALLOC;
	}

	last->size = size;

	return last;
}

void split_block(struct block_meta *block, size_t size)
{
	/**
	 * Splits the block making the current one smaller and
	 * the after split block is declared free.
	 */
	struct block_meta *after_split = (struct block_meta *)
									((char *)block + META_SIZE + size);

	after_split->size = block->size - META_SIZE - size;
	after_split->prev = block;
	after_split->next = block->next;
	after_split->status = STATUS_FREE;

	if (block->next)
		block->next->prev = after_split;

	block->next = after_split;
	block->size = size;
}
