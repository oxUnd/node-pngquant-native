#include "mempool.h"
#include <stdlib.h>
#include <assert.h>

#define MEMPOOL_RESERVED ((sizeof(mempool)+15) & ~0xF)
#define MEMPOOL_SIZE (1<<19)

struct mempool {
	mempool* next;
	size_t used;
};

void* mempool_new(mempool*& rpm, size_t size)
{
	assert(size < MEMPOOL_SIZE-MEMPOOL_RESERVED);
	
	if (rpm && (rpm->used+size) <= MEMPOOL_SIZE) {
		size_t prevused = rpm->used;
		rpm->used += (size+15) & ~0xF;
		return ((char*)rpm) + prevused;
	}

	mempool* old = rpm ? rpm : NULL;
	char* mem = (char*) calloc(MEMPOOL_SIZE, 1);

	rpm = (mempool*)mem;
	rpm->used = MEMPOOL_RESERVED;
	rpm->next = old;

	return mempool_new(rpm, size);
}

void mempool_free(mempool* m)
{
	while (m) {
		mempool* next = m->next;
		free(m);
		m = next;
	}
}

