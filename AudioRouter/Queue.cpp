/*++

Module Name:

    queue.c

Abstract:

    This file contains the queue entry points and callbacks.

Environment:

    Kernel-mode Driver Framework

--*/

#include "Queue.h"
#include "Queue.tmh"

#include "Prelude.h"

#ifdef ALLOC_PRAGMA
#pragma alloc_text (PAGE, AudioRouterQueueInitialize)
#endif

void log_err_queue(const char* call, const char* func, unsigned int line, NTSTATUS status)
{
    TraceEvents(TRACE_LEVEL_ERROR, TRACE_QUEUE, "%s in %s (%s:%d) failed with status %!STATUS!", call, func, __FILE__, line, status);
}
#define TRY(expr) { NTSTATUS status = (expr); if (!NT_SUCCESS(status)) { log_err_queue(#expr, __FUNCTION__, __LINE__, status); return status; } }
#define TRY_CLEAN(expr, clean) { NTSTATUS status = (expr); if (!NT_SUCCESS(status)) { log_err_queue(#expr, __FUNCTION__, __LINE__, status); clean; return status; } }

void ClearContext(PQUEUE_CONTEXT context)
{
    if (context->data != NULL)
    {
        ExFreePoolWithTag(context->data, QUEUE_POOL_TAG);
        context->data = NULL;
    }
    context->offset = 0;
    context->remaining = 0;
}

VOID AudioRouterEvtQueueContextCleanup(_In_ WDFOBJECT Object)
{
    PQUEUE_CONTEXT context = QueueGetContext(Object);
    ClearContext(context);
}

NTSTATUS AudioRouterQueueInitialize(_In_ WDFDEVICE Device)
{
    PAGED_CODE();

    // Configure a default queue so that requests that are not
    // configure-forwarded using WdfDeviceConfigureRequestDispatching to goto
    // other queues get dispatched here.
    WDF_IO_QUEUE_CONFIG queueConfig;
    WDF_IO_QUEUE_CONFIG_INIT_DEFAULT_QUEUE(&queueConfig, WdfIoQueueDispatchSequential);
    queueConfig.DefaultQueue = TRUE;
    queueConfig.EvtIoDefault = AudioRouterEvtIoDefault;
    queueConfig.EvtIoDeviceControl = AudioRouterEvtIoDeviceControl;
    queueConfig.EvtIoStop = AudioRouterEvtIoStop;
    queueConfig.EvtIoRead = AudioRouterEvtIoRead;
    queueConfig.EvtIoWrite = AudioRouterEvtIoWrite;

    WDF_OBJECT_ATTRIBUTES queueAttributes;
    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&queueAttributes, QUEUE_CONTEXT);
    queueAttributes.EvtCleanupCallback = AudioRouterEvtQueueContextCleanup;

    WDFQUEUE queue;
    TRY(WdfIoQueueCreate(Device, &queueConfig, &queueAttributes, &queue));

    PQUEUE_CONTEXT context = QueueGetContext(queue);
    context->data = NULL;
    ClearContext(context);

    return STATUS_SUCCESS;
}

/**
 * @brief This event is invoked when the framework receives IRP_MJ_DEVICE_CONTROL request.
 *
 * @param Queue Handle to the framework queue object that is associated with the I/O request.
 * @param Request Handle to a framework request object.
 * @param OutputBufferLength Size of the output buffer in bytes
 * @param InputBufferLength Size of the input buffer in bytes
 * @param IoControlCode I/O control code.
 */
VOID AudioRouterEvtIoDeviceControl(
    _In_ WDFQUEUE Queue,
    _In_ WDFREQUEST Request,
    _In_ size_t OutputBufferLength,
    _In_ size_t InputBufferLength,
    _In_ ULONG IoControlCode
)
{
    TraceEvents(TRACE_LEVEL_INFORMATION,
                TRACE_QUEUE,
                "%!FUNC! Queue 0x%p, Request 0x%p OutputBufferLength %d InputBufferLength %d IoControlCode %d",
                Queue, Request, (int) OutputBufferLength, (int) InputBufferLength, IoControlCode);

    WdfRequestComplete(Request, STATUS_SUCCESS);
}

NTSTATUS AudioRouterEvtIoWrite_impl(_In_ WDFQUEUE Queue, _In_ WDFREQUEST Request, _In_ size_t Length)
{
    PQUEUE_CONTEXT context = QueueGetContext(Queue);
    size_t new_len = context->remaining + Length;
    BYTE* new_data = (BYTE*)ExAllocatePoolWithTag(NonPagedPool, new_len, QUEUE_POOL_TAG);
    if (new_data == NULL)
    {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_QUEUE, "%!FUNC! ExAllocatePoolWithTag failed");
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    if (context->remaining > 0)
    {
        RtlCopyMemory(new_data, context->data + context->offset, context->remaining);
        ClearContext(context);
    }

    WDFMEMORY memory;
    TRY(WdfRequestRetrieveInputMemory(Request, &memory));
    TRY(WdfMemoryCopyToBuffer(memory, 0, new_data + context->remaining, Length));

    context->data = new_data;
    context->remaining = new_len;
    context->offset = 0;

    size_t bytes_written = Length;
    WdfRequestSetInformation(Request, bytes_written);
    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_QUEUE, "%!FUNC! Wrote %d/%d bytes", (int) bytes_written, (int) Length);

    return STATUS_SUCCESS;
}

VOID AudioRouterEvtIoWrite(_In_ WDFQUEUE Queue, _In_ WDFREQUEST Request, _In_ size_t Length)
{
    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_QUEUE, "<%!FUNC! Length=%d>", (int) Length);

    NTSTATUS status = AudioRouterEvtIoWrite_impl(Queue, Request, Length);

    WdfRequestComplete(Request, status);

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_QUEUE, "</%!FUNC!>");
}

NTSTATUS AudioRouterEvtIoRead_impl(_In_ WDFQUEUE Queue, _In_ WDFREQUEST Request, _In_ size_t Length)
{
    PQUEUE_CONTEXT context = QueueGetContext(Queue);
    if (context->data == NULL)
    {
        TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_QUEUE, "%!FUNC! data is NULL");
        return STATUS_END_OF_FILE;
    }

    size_t bytes_read = min(context->remaining, Length);

    WDFMEMORY memory;
    TRY(WdfRequestRetrieveOutputMemory(Request, &memory));
    TRY(WdfMemoryCopyFromBuffer(memory, 0, context->data + context->offset, bytes_read));

    context->offset += bytes_read;
    context->remaining -= bytes_read;

    if (context->remaining == 0)
    {
        ClearContext(context);
    }

    WdfRequestSetInformation(Request, bytes_read);
    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_QUEUE, "%!FUNC! Read %d/%d bytes. %d remaining in buffer", (int) bytes_read, (int) Length, (int) context->remaining);

    return STATUS_SUCCESS;
}

VOID AudioRouterEvtIoRead(_In_ WDFQUEUE Queue, _In_ WDFREQUEST Request, _In_ size_t Length)
{
    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_QUEUE, "<%!FUNC! Length=%d>", (int) Length);

    NTSTATUS status = AudioRouterEvtIoRead_impl(Queue, Request, Length);

    WdfRequestComplete(Request, status);

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_QUEUE, "</%!FUNC!>");
}

/**
 * @brief This event is invoked for a power-managed queue before the device leaves the working state (D0).
 *
 * @param Queue Handle to the framework queue object that is associated with the I/O request.
 * @param Request Handle to a framework request object.
 * @param ActionFlags A bitwise OR of one or more WDF_REQUEST_STOP_ACTION_FLAGS-typed flags
 * that identify the reason that the callback function is being called
 * and whether the request is cancelable.
 */
VOID AudioRouterEvtIoStop(
    _In_ WDFQUEUE Queue,
    _In_ WDFREQUEST Request,
    _In_ ULONG ActionFlags
)
{
    TraceEvents(TRACE_LEVEL_INFORMATION,
                TRACE_QUEUE,
                "%!FUNC! Queue 0x%p, Request 0x%p ActionFlags %d",
                Queue, Request, ActionFlags);

    // PQUEUE_CONTEXT context = QueueGetContext(Queue);
    // ClearContext(context);

    //
    // In most cases, the EvtIoStop callback function completes, cancels, or postpones
    // further processing of the I/O request.
    //
    // Typically, the driver uses the following rules:
    //
    // - If the driver owns the I/O request, it calls WdfRequestUnmarkCancelable
    //   (if the request is cancelable) and either calls WdfRequestStopAcknowledge
    //   with a Requeue value of TRUE, or it calls WdfRequestComplete with a
    //   completion status value of STATUS_SUCCESS or STATUS_CANCELLED.
    //
    //   Before it can call these methods safely, the driver must make sure that
    //   its implementation of EvtIoStop has exclusive access to the request.
    //
    //   In order to do that, the driver must synchronize access to the request
    //   to prevent other threads from manipulating the request concurrently.
    //   The synchronization method you choose will depend on your driver's design.
    //
    //   For example, if the request is held in a shared context, the EvtIoStop callback
    //   might acquire an internal driver lock, take the request from the shared context,
    //   and then release the lock. At this point, the EvtIoStop callback owns the request
    //   and can safely complete or requeue the request.
    //
    // - If the driver has forwarded the I/O request to an I/O target, it either calls
    //   WdfRequestCancelSentRequest to attempt to cancel the request, or it postpones
    //   further processing of the request and calls WdfRequestStopAcknowledge with
    //   a Requeue value of FALSE.
    //
    // A driver might choose to take no action in EvtIoStop for requests that are
    // guaranteed to complete in a small amount of time.
    //
    // In this case, the framework waits until the specified request is complete
    // before moving the device (or system) to a lower power state or removing the device.
    // Potentially, this inaction can prevent a system from entering its hibernation state
    // or another low system power state. In extreme cases, it can cause the system
    // to crash with bugcheck code 9F.
    //
}

VOID AudioRouterEvtIoDefault(_In_ WDFQUEUE Queue, _In_ WDFREQUEST Request)
{
    UNREFERENCED_PARAMETER(Queue);

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_QUEUE, "<%!FUNC!>");

    WDF_REQUEST_PARAMETERS params;
    WdfRequestGetParameters(Request, &params);

    NTSTATUS status = STATUS_SUCCESS;

    switch (params.Type)
    {
    case WdfRequestTypeCreate:
        TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_QUEUE, "WdfRequestTypeCreate");
        break;
    case WdfRequestTypeRead:
        TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_QUEUE, "WdfRequestTypeRead");
        status = AudioRouterEvtIoRead_impl(Queue, Request, params.Parameters.Read.Length);
        break;
    case WdfRequestTypeWrite:
        TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_QUEUE, "WdfRequestTypeWrite");
        status = AudioRouterEvtIoWrite_impl(Queue, Request, params.Parameters.Write.Length);
        break;
    case WdfRequestTypeDeviceControl:
        TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_QUEUE, "WdfRequestTypeDeviceControl");
        break;
    default:
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_QUEUE, "WdfRequestTypeUnknown: %x", (int)params.Type);
    }

    WdfRequestComplete(Request, status);

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_QUEUE, "</%!FUNC!>");
}
