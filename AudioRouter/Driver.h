/*++

Module Name:

    driver.h

Abstract:

    This file contains the driver definitions.

Environment:

    Kernel-mode Driver Framework

--*/
#pragma once

#include "Prelude.h"

EXTERN_C_START

//
// WDFDRIVER Events
//

DRIVER_INITIALIZE DriverEntry;
EVT_WDF_DRIVER_DEVICE_ADD AudioRouterEvtDeviceAdd;
EVT_WDF_OBJECT_CONTEXT_CLEANUP AudioRouterEvtDriverContextCleanup;

DRIVER_ADD_DEVICE AudioRouterAddDevice;
NTSTATUS AudioRouterStartDevice(PDEVICE_OBJECT, PIRP, PRESOURCELIST);
_Dispatch_type_(IRP_MJ_PNP) DRIVER_DISPATCH PnpHandler;

EXTERN_C_END
