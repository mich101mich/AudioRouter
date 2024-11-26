/*++

Module Name:

    driver.c

Abstract:

    This file contains the driver entry points and callbacks.

Environment:

    Kernel-mode Driver Framework

--*/

#include "Driver.h"
#include "AdapterCommon.h"
#include "Driver.tmh"

#ifdef ALLOC_PRAGMA
#pragma alloc_text (INIT, DriverEntry)
#pragma alloc_text (PAGE, AudioRouterEvtDeviceAdd)
#pragma alloc_text (PAGE, AudioRouterEvtDriverContextCleanup)
#endif

void log_err_driver(const char* call, const char* func, unsigned int line, NTSTATUS status)
{
    TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER, "%s in %s (%s:%d) failed with status %!STATUS!", call, func, __FILE__, line, status);
}
#define TRY(expr) { NTSTATUS status = (expr); if (!NT_SUCCESS(status)) { log_err_driver(#expr, __FUNCTION__, __LINE__, status); return status; } }
#define TRY_CLEAN(expr, clean) { NTSTATUS status = (expr); if (!NT_SUCCESS(status)) { log_err_driver(#expr, __FUNCTION__, __LINE__, status); clean; return status; } }

/**
 * @brief initializes the driver
 *
 * DriverEntry initializes the driver and is the first routine called by the
 * system after the driver is loaded. DriverEntry specifies the other entry
 * points in the function driver, such as EvtDevice and DriverUnload.
 *
 * @param DriverObject the instance of the function driver that is loaded
 * into memory. DriverEntry must initialize members of DriverObject before it
 * returns to the caller. DriverObject is allocated by the system before the
 * driver is loaded, and it is released by the system after the system unloads
 * the function driver from memory.
 * @param RegistryPath the driver specific path in the Registry.
 * The function driver can use the path to store driver related data between
 * reboots. The path does not store hardware instance specific data.
 * @return NTSTATUS
 */
NTSTATUS DriverEntry(_In_ PDRIVER_OBJECT DriverObject, _In_ PUNICODE_STRING RegistryPath)
{
    WPP_INIT_TRACING(DriverObject, RegistryPath);

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "<%!FUNC! RegistryPath=%wZ>", RegistryPath);

    // Register a cleanup callback so that we can call WPP_CLEANUP when
    // the framework driver object is deleted during driver unload.
    WDF_OBJECT_ATTRIBUTES attributes;
    WDF_OBJECT_ATTRIBUTES_INIT(&attributes);
    attributes.EvtCleanupCallback = AudioRouterEvtDriverContextCleanup;

    WDF_DRIVER_CONFIG config;
    WDF_DRIVER_CONFIG_INIT(&config, WDF_NO_EVENT_CALLBACK);
    config.DriverInitFlags |= WdfDriverInitNoDispatchOverride;
    config.DriverPoolTag = AUDIO_ROUTER_POOL_TAG;

    TRY_CLEAN(WdfDriverCreate(DriverObject, RegistryPath, &attributes, &config, WDF_NO_HANDLE), { WPP_CLEANUP(DriverObject); });

    TRY_CLEAN(PcInitializeAdapterDriver(DriverObject, RegistryPath, AudioRouterAddDevice), { WPP_CLEANUP(DriverObject); });

    DriverObject->MajorFunction[IRP_MJ_PNP] = PnpHandler;

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "</%!FUNC!>");

    return STATUS_SUCCESS;
}

/**
 * @brief called by the framework in response to AddDevice
 *
 * EvtDeviceAdd is called by the framework in response to AddDevice
 * call from the PnP manager. We create and initialize a device object to
 * represent a new instance of the device.
 *
 * @param Driver Handle to a framework driver object created in DriverEntry
 * @param DeviceInit Pointer to a framework-allocated WDFDEVICE_INIT structure.
 * @return NTSTATUS
 */
NTSTATUS AudioRouterEvtDeviceAdd(_In_ WDFDRIVER Driver, _Inout_ PWDFDEVICE_INIT DeviceInit)
{
    UNREFERENCED_PARAMETER(Driver);

    PAGED_CODE();

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "<%!FUNC!>");

    TRY(AudioRouterCreateDevice(DeviceInit));

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "</%!FUNC!>");

    return STATUS_SUCCESS;
}

/**
 * @brief Free all the resources allocated in DriverEntry.
 *
 * @param DriverObject handle to a WDF Driver object.
 */
VOID AudioRouterEvtDriverContextCleanup(_In_ WDFOBJECT DriverObject)
{
    PAGED_CODE();

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "<%!FUNC!/>");

    // Stop WPP Tracing
    WPP_CLEANUP(WdfDriverWdmGetDriverObject((WDFDRIVER)DriverObject));
}

//
// This is the structure of the portclass FDO device extension Nt has created
// for us.  We keep the adapter common object here.
//
typedef struct _PortClassDeviceContext              // 32       64      Byte offsets for 32 and 64 bit architectures
{
    ULONG_PTR m_pulReserved1[2];                    // 0-7      0-15    First two pointers are reserved.
    PDEVICE_OBJECT m_DoNotUsePhysicalDeviceObject;  // 8-11     16-23   Reserved pointer to our Physical Device Object (PDO).
    PVOID m_pvReserved2;                            // 12-15    24-31   Reserved pointer to our Start Device function.
    PVOID m_pvReserved3;                            // 16-19    32-39   "Out Memory" according to DDK.
    AdapterCommon* m_pCommon;                       // 20-23    40-47   Pointer to our adapter common object.
    PVOID m_pvUnused1;                              // 24-27    48-55   Unused space.
    PVOID m_pvUnused2;                              // 28-31    56-63   Unused space.

    // Anything after above line should not be used.
    // This actually goes on for (64*sizeof(ULONG_PTR)) but it is all opaque.
} PortClassDeviceContext;

/**
 * The Plug & Play subsystem is handing us a brand new PDO, for which we
 * (by means of INF registration) have been asked to provide a driver.
 *
 * We need to determine if we need to be in the driver stack for the device.
 * Create a function device object to attach to the stack
 * Initialize that device object
 *
 * @param DriverObject pointer to a driver object
 * @param PhysicalDeviceObject pointer to a device object created by the underlying bus driver.
 * @return NTSTATUS
 */
NTSTATUS AudioRouterAddDevice(_In_ PDRIVER_OBJECT DriverObject, _In_ PDEVICE_OBJECT PhysicalDeviceObject)
{
    PAGED_CODE();

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "<%!FUNC!>");

    ULONG maxObjects = 2;

    // Tell the class driver to add the device.
    TRY(PcAddAdapterDevice(DriverObject, PhysicalDeviceObject,
                           AudioRouterStartDevice, maxObjects, 0));
    UNREFERENCED_PARAMETER(DriverObject);
    UNREFERENCED_PARAMETER(PhysicalDeviceObject);

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "</%!FUNC!>");

    return STATUS_SUCCESS;
}

NTSTATUS AudioRouterStartDevice(_In_ PDEVICE_OBJECT DeviceObject, _In_ PIRP Irp, _In_ PRESOURCELIST ResourceList)
{
    PAGED_CODE();

    UNREFERENCED_PARAMETER(DeviceObject);
    UNREFERENCED_PARAMETER(Irp);
    UNREFERENCED_PARAMETER(ResourceList);

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "<%!FUNC!>");

    // AdapterCommon* adapter = NULL;
    // TRY(AdapterCommon::Create(adapter, NULL));

    // TRY(adapter->Init(DeviceObject));

    // TRY(PcRegisterAdapterPowerManagement(PUNKNOWN(adapter), DeviceObject));

    // TRY(adapter->InstallEndpointFilters(Irp, &SpeakerMiniports, NULL, NULL, NULL));

    // auto ext = static_cast<PortClassDeviceContext*>(DeviceObject->DeviceExtension);
    // ext->m_pCommon = adapter;

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "</%!FUNC!>");

    return STATUS_SUCCESS;
}

/**
 * @brief Handles PnP IRPs
 *
 * @param _DeviceObject Functional Device object pointer.
 * @param _Irp The Irp being passed
 * @return NTSTATUS
 */
NTSTATUS PnpHandler(_In_ PDEVICE_OBJECT DeviceObject, _Inout_ PIRP Irp)
{
    PAGED_CODE();

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "<%!FUNC!>");

    ASSERT(DeviceObject);
    ASSERT(Irp);

    //
    // Check for the REMOVE_DEVICE irp.  If we're being unloaded,
    // uninstantiate our devices and release the adapter common
    // object.
    //
    PIO_STACK_LOCATION stack = IoGetCurrentIrpStackLocation(Irp);

    // PortClassDeviceContext* ext;

    switch (stack->MinorFunction)
    {
    case IRP_MN_REMOVE_DEVICE:
    case IRP_MN_SURPRISE_REMOVAL:
    case IRP_MN_STOP_DEVICE:
        // ext = static_cast<PortClassDeviceContext*>(DeviceObject->DeviceExtension);

        // if (ext->m_pCommon != NULL)
        // {
        //     ext->m_pCommon->Cleanup();

        //     ext->m_pCommon->Release();
        //     ext->m_pCommon = NULL;
        // }
        break;

    default:
        break;
    }

    TRY(PcDispatchIrp(DeviceObject, Irp));

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "</%!FUNC!>");

    return STATUS_SUCCESS;
}

NTSTATUS
_IRQL_requires_max_(DISPATCH_LEVEL)
PowerControlCallback
(
    _In_        LPCGUID PowerControlCode,
    _In_opt_    PVOID   InBuffer,
    _In_        SIZE_T  InBufferSize,
    _Out_writes_bytes_to_(OutBufferSize, *BytesReturned) PVOID OutBuffer,
    _In_        SIZE_T  OutBufferSize,
    _Out_opt_   PSIZE_T BytesReturned,
    _In_opt_    PVOID   Context
)
{
    UNREFERENCED_PARAMETER(PowerControlCode);
    UNREFERENCED_PARAMETER(InBuffer);
    UNREFERENCED_PARAMETER(InBufferSize);
    UNREFERENCED_PARAMETER(OutBuffer);
    UNREFERENCED_PARAMETER(OutBufferSize);
    UNREFERENCED_PARAMETER(BytesReturned);
    UNREFERENCED_PARAMETER(Context);

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "<%!FUNC!/>");

    return STATUS_NOT_IMPLEMENTED;
}
