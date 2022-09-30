/*++

Module Name:

    queue.h

Abstract:

    This file contains the queue definitions.

Environment:

    Kernel-mode Driver Framework

--*/

EXTERN_C_START

//
// This is the context that can be placed per queue
// and would contain per queue information.
//
typedef struct _QUEUE_CONTEXT
{
    BYTE* data;
    size_t offset;
    size_t remaining;
} QUEUE_CONTEXT, *PQUEUE_CONTEXT;

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(QUEUE_CONTEXT, QueueGetContext)

const ULONG QUEUE_POOL_TAG = 'data';

/**
 * The I/O dispatch callbacks for the frameworks device object
 * are configured in this function.
 *
 * A single default I/O Queue is configured for parallel request
 * processing, and a driver context memory allocation is created
 * to hold our structure QUEUE_CONTEXT.
 *
 * @param Device Handle to a framework device object.
 * @return NTSTATUS
 */
NTSTATUS AudioRouterQueueInitialize(_In_ WDFDEVICE Device);

//
// Events from the IoQueue object
//
EVT_WDF_IO_QUEUE_IO_DEFAULT AudioRouterEvtIoDefault;
EVT_WDF_IO_QUEUE_IO_DEVICE_CONTROL AudioRouterEvtIoDeviceControl;
EVT_WDF_IO_QUEUE_IO_READ AudioRouterEvtIoRead;
EVT_WDF_IO_QUEUE_IO_WRITE AudioRouterEvtIoWrite;
EVT_WDF_IO_QUEUE_IO_STOP AudioRouterEvtIoStop;

EXTERN_C_END
