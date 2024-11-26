#pragma once

#define _NEW_DELETE_OPERATORS_

#include <ntddk.h>
#include <wdf.h>
#include <initguid.h>
#include <portcls.h>
#include <stdunk.h>

#include "Device.h"
#include "Queue.h"
#include "Public.h"
#include "Trace.h"

#define AUDIO_ROUTER_POOL_TAG 'oRuA'

inline void* operator new (size_t size, void* p)
{
    UNREFERENCED_PARAMETER(size);
    return p;
}

void operator delete (void* p, size_t size);

template<typename T>
inline NTSTATUS allocate(T*& p)
{
    p = (T*)ExAllocatePoolWithTag(NonPagedPool, sizeof(T), AUDIO_ROUTER_POOL_TAG);
    if (!p)
    {
        return STATUS_INSUFFICIENT_RESOURCES;
    }
    return STATUS_SUCCESS;
}

template<typename T>
inline void deallocate(T*& p)
{
    if (p != NULL)
    {
        ExFreePoolWithTag(p, AUDIO_ROUTER_POOL_TAG);
        p = NULL;
    }
}

template<typename T>
inline void release(T*& p)
{
    if (p != NULL)
    {
        p->Release();
        p = NULL;
    }
}
