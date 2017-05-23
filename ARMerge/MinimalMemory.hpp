#pragma once

namespace Minimal
{
void   Memory_Initialize();

void * malloc(size_t N);
void * realloc(void *memBlock, size_t N);

void * __cdecl memset(void *_dst, int _val, size_t _size);

}
