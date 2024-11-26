#include "Prelude.h"

void operator delete (void* p, size_t size)
{
    UNREFERENCED_PARAMETER(size);
    ExFreePoolWithTag(p, AUDIO_ROUTER_POOL_TAG);
}
