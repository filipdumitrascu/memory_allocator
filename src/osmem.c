// SPDX-License-Identifier: BSD-3-Clause

#include "helpers.h"

void *head_block;
size_t mmap_cmp = MMAP_THRESHOLD;

void *os_malloc(size_t size)
{
	size = ALIGN(size);

	if (!size)
		return NULL;

	struct block_meta *block = NULL;

	/**
	 * If there is no block on the heap.
	 */
	if (!head_block) {
		block = allocate_block(NULL, size);
		head_block = block;

	} else {
		coalesce_blocks();

		struct block_meta *last = head_block;

		block = find_best(&last, size);

		/**
		 * If the find_best function returned NULL, there is no free block
		 * where the input size can be placed, so the allocation takes place
		 * at the end of the heap.
		 */
		if (!block) {
			if (last->status == STATUS_FREE && size > last->size)
				block = expand_last(last, size);

			else
				block = allocate_block(last, size);

		} else {
			if (size + META_SIZE + 8 <= block->size)
				split_block(block, size);

			block->status = STATUS_ALLOC;
		}
	}

	return block + 1;
}

void os_free(void *ptr)
{
	if (!ptr)
		return;

	struct block_meta *current = (struct block_meta *)(ptr - META_SIZE);

	/**
	 * For mmaped zones a munmap is called and for sbrk zones
	 * only the status is changed for less syscalls.
	 */
	if (current->status == STATUS_MAPPED) {
		if (current->prev)
			current->prev->next = current->next;
		else
			head_block = current->next;

		if (current->next)
			current->next->prev = current->prev;

		int ok = munmap(current, current->size + META_SIZE);

		DIE(ok, "munmap syscall failed");

	} else {
		current->status = STATUS_FREE;
	}
}

void *os_calloc(size_t nmemb, size_t size)
{
	/**
	 * Changes the mmap landmark and calls a malloc
	 */
	mmap_cmp = getpagesize();
	void *ptr = os_malloc(nmemb * size);

	/**
	 * Specific to calloc, initializes the memory to 0
	 */
	if (ptr)
		memset(ptr, 0, size);

	mmap_cmp = MMAP_THRESHOLD;
	return ptr;
}

void *os_realloc(void *ptr, size_t size)
{
	size = ALIGN(size);

	if (!ptr)
		return os_malloc(size);

	if (!size) {
		os_free(ptr);
		return NULL;
	}

	struct block_meta *block = (struct block_meta *)(ptr - META_SIZE);

	if (block->status == STATUS_FREE)
		return NULL;

	if (block->status == STATUS_MAPPED) {
		block = os_malloc(size);
		os_free(ptr);
		return block;
	}

	/**
	 * If there is no space at this block
	 */
	if (size > block->size) {
		if (block->next) {
			struct block_meta *last = block;
			size_t preview = last->size;

			/**
			 * Calculate a preview size for the following
			 * free blocks for a coalesce
			 */
			while (last->next && last->next->status == STATUS_FREE) {
				preview += META_SIZE + last->next->size;
				last = last->next;
			}

			/**
			 * There is no way the realloc can be done at this address,
			 * so another address is searched and the memory is copied there.
			 */
			if (size > preview) {
				void *new_ptr = os_malloc(size);

				memcpy(new_ptr, ptr, block->size);
				os_free(ptr);
				return new_ptr;

			/**
			 * The realloc takes place at this address and if there is more
			 * space than required a split takes place also.
			 */
			} else {
				block->size = preview;
				block->next = last->next;

				if (last->next)
					last->next->prev = block;

				if (size + META_SIZE + 8 <= preview)
					split_block(block, size);
			}

		} else {
			expand_last(block, size);
		}

	} else {
		/**
		 * If there is space at this block, checks if a split is needed
		 */
		if (size + META_SIZE + 8 <= block->size)
			split_block(block, size);
	}

	return block + 1;
}
