#pragma once

struct mempool;

void* mempool_new(mempool*& rpm, size_t size);
void mempool_free(mempool* m);

