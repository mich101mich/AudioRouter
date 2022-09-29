/*++

Module Name:

    device.h

Abstract:

    This file contains the device definitions.

Environment:

    Kernel-mode Driver Framework

--*/

#include "public.h"

EXTERN_C_START

//
// The device context performs the same job as
// a WDM device extension in the driver frameworks
//
typedef struct _DEVICE_CONTEXT
{
    ULONG PrivateDeviceData;  // just a placeholder

} DEVICE_CONTEXT, *PDEVICE_CONTEXT;

//
// This macro will generate an inline function called DeviceGetContext
// which will be used to get a pointer to the device context memory
// in a type safe manner.
//
WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(DEVICE_CONTEXT, DeviceGetContext)

/**
 * @brief Worker routine called to create a device and its software resources.
 *
 * @param DeviceInit Pointer to an opaque init structure. Memory for this
 * structure will be freed by the framework when the WdfDeviceCreate
 * succeeds. So don't access the structure after that point.
 * @return NTSTATUS
 */
NTSTATUS AudioRouterCreateDevice(_Inout_ PWDFDEVICE_INIT DeviceInit);

EXTERN_C_END
