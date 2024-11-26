#include "AdapterCommon.h"
#include "AdapterCommon.tmh"

#include "SpeakerWavTable.h"

#include <ntstrsafe.h>
#include <wdfminiport.h>

void log_err_adapter(const char* call, const char* func, unsigned int line, NTSTATUS status)
{
    TraceEvents(TRACE_LEVEL_ERROR, TRACE_ADAPTER, "%s in %s (%s:%d) failed with status %!STATUS!", call, func, __FILE__, line, status);
}
#define TRY(expr) { NTSTATUS status = (expr); if (!NT_SUCCESS(status)) { log_err_adapter(#expr, __FUNCTION__, __LINE__, status); return status; } }
#define TRY_CLEAN(expr, clean) { NTSTATUS status = (expr); if (!NT_SUCCESS(status)) { log_err_adapter(#expr, __FUNCTION__, __LINE__, status); clean; return status; } }

LONG AdapterCommon::m_AdapterInstances = 0;


NTSTATUS AdapterCommon::Create(_Out_ AdapterCommon*& p, _In_opt_ PUNKNOWN UnknownOuter)
{
    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_ADAPTER, "<%!FUNC!>");
    //
    // This sample supports only one instance of this object.
    // (b/c of CSaveData's static members and Bluetooth HFP logic).
    //
    if (InterlockedCompareExchange(&AdapterCommon::m_AdapterInstances, 1, 0) != 0)
    {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_ADAPTER, "NewAdapterCommon failed, only one instance is allowed");
        return STATUS_DEVICE_BUSY;
    }

    TRY(allocate(p));
    new ((void*)p) AdapterCommon(UnknownOuter);
    p->AddRef();

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_ADAPTER, "</%!FUNC!>");

    return STATUS_SUCCESS;
}

AdapterCommon::~AdapterCommon()
{
    PAGED_CODE();
    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_ADAPTER, "<%!FUNC!>");

    // CSaveData::DestroyWorkItems();
    release(m_pPortClsEtwHelper);
    release(m_pServiceGroupWave);

    if (m_WdfDevice)
    {
        WdfObjectDelete(m_WdfDevice);
        m_WdfDevice = NULL;
    }

    InterlockedDecrement(&AdapterCommon::m_AdapterInstances);
    ASSERT(AdapterCommon::m_AdapterInstances == 0);

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_ADAPTER, "</%!FUNC!>");
}

/**
 * @brief QueryInterface routine for AdapterCommon
 *
 * @param Interface
 * @param Object
 * @return STDMETHODIMP
 */
STDMETHODIMP AdapterCommon::NonDelegatingQueryInterface(_In_ REFIID Interface, _COM_Outptr_ PVOID* Object)
{
    PAGED_CODE();

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_ADAPTER, "<%!FUNC!>");

    if (IsEqualGUIDAligned(Interface, IID_IUnknown)
            || IsEqualGUIDAligned(Interface, IID_IAdapterPowerManagement))
    {
        this->AddRef();
        *Object = this;
    }
    else
    {
        *Object = NULL;
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_ADAPTER, "NonDelegatingQueryInterface failed, no interface");
        return STATUS_INVALID_PARAMETER;
    }

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_ADAPTER, "</%!FUNC!>");

    return STATUS_SUCCESS;
} // NonDelegatingQueryInterface

/**
 * @brief Power state changed
 *
 * @param NewState The requested, new power state for the device.
 *
 */
STDMETHODIMP_(void) AdapterCommon::PowerChangeState(_In_ POWER_STATE NewState)
{
    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_ADAPTER, "<%!FUNC!>");

    // Notify all registered miniports of a power state change
    for (auto& miniport : m_Subdevices)
    {
        if (miniport.PowerInterface)
        {
            miniport.PowerInterface->PowerChangeState(NewState);
        }
    }

    // is this actually a state change??
    //
    if (NewState.DeviceState != m_PowerState)
    {
        // switch on new state
        //
        switch (NewState.DeviceState)
        {
        case PowerDeviceD0:
        case PowerDeviceD1:
        case PowerDeviceD2:
        case PowerDeviceD3:
            m_PowerState = NewState.DeviceState;
            TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_ADAPTER, "Entering D%u", ULONG(m_PowerState) - ULONG(PowerDeviceD0));
            break;
        default:
            TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_ADAPTER, "Unknown Device Power State");
            break;
        }
    }

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_ADAPTER, "</%!FUNC!>");
} // PowerStateChange

/**
 * @brief Query to see if the device can change to this power state
 *
 * @param NewStateQuery The requested, new power state for the device
 */
STDMETHODIMP_(NTSTATUS) AdapterCommon::QueryPowerChangeState(_In_ POWER_STATE NewStateQuery)
{
    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_ADAPTER, "<%!FUNC!>");

    // query each miniport for it's power state, we're finished if even one indicates
    // it cannot go to this power state.
    for (auto& miniport : m_Subdevices)
    {
        TRY(miniport.PowerInterface->QueryPowerChangeState(NewStateQuery));
    }

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_ADAPTER, "</%!FUNC!>");

    return STATUS_SUCCESS;
} // QueryPowerChangeState

/**
 * @brief Called at startup to get the caps for the device.
 *
 * This structure provides the system with the mappings between system power state and device
 * power state. This typically will not need modification by the driver.
 *
 * @param PowerDeviceCaps The device's capabilities.
 */
STDMETHODIMP_(NTSTATUS) AdapterCommon::QueryDeviceCapabilities(_Inout_updates_bytes_(sizeof(DEVICE_CAPABILITIES)) PDEVICE_CAPABILITIES PowerDeviceCaps)
{
    UNREFERENCED_PARAMETER(PowerDeviceCaps);

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_ADAPTER, "<%!FUNC!/>");

    return STATUS_SUCCESS;
} // QueryDeviceCapabilities

NTSTATUS AdapterCommon::Init(_In_ PDEVICE_OBJECT DeviceObject)
{
    PAGED_CODE();

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_ADAPTER, "<%!FUNC!>");

    ASSERT(DeviceObject);

    m_pServiceGroupWave     = NULL;
    m_pDeviceObject         = DeviceObject;
    m_PowerState            = PowerDeviceD0;
    // m_pHW                   = NULL;
    m_pPortClsEtwHelper     = NULL;

    //
    // Get the PDO.
    //
    m_pPhysicalDeviceObject = NULL;
    // TRY(PcGetPhysicalDeviceObject(DeviceObject, &m_pPhysicalDeviceObject));

    //
    // Create a WDF miniport to represent the adapter. Note that WDF miniports
    // are NOT audio miniports. An audio adapter is associated with a single WDF
    // miniport. This driver uses WDF to simplify the handling of the Bluetooth
    // SCO HFP Bypass interface.
    //
    m_WdfDevice             = NULL;
    // TRY(WdfDeviceMiniportCreate(WdfGetDriver(),
    //                             WDF_NO_OBJECT_ATTRIBUTES,
    //                             DeviceObject,           // FDO
    //                             NULL,                   // Next device.
    //                             NULL,                   // PDO
    //                             &m_WdfDevice));

    // // Initialize HW.
    // //
    // m_pHW = new (POOL_FLAG_NON_PAGED, SIMPLEAUDIOSAMPLE_POOLTAG)  CSimpleAudioSampleHW;
    // if (!m_pHW)
    // {
    //     DPF(D_TERSE, ("Insufficient memory for Simple Audio Sample HW"));
    //     ntStatus = STATUS_INSUFFICIENT_RESOURCES;
    // }
    // IF_FAILED_JUMP(ntStatus, Done);

    // m_pHW->MixerReset();

    //
    // Initialize SaveData class.
    //
    // CSaveData::SetDeviceObject(DeviceObject);   //device object is needed by CSaveData
    // TRY(CSaveData::InitializeWorkItems(DeviceObject));

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_ADAPTER, "</%!FUNC!>");

    return STATUS_SUCCESS;
}

VOID AdapterCommon::Cleanup()
{
    PAGED_CODE();

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_ADAPTER, "<%!FUNC!>");

    m_Subdevices.clear();

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_ADAPTER, "</%!FUNC!>");
}


STDMETHODIMP_(NTSTATUS) AdapterCommon::InstallEndpointFilters(
    _In_opt_    PIRP                Irp,
    _In_        PENDPOINT_MINIPORT  Miniport,
    _In_opt_    PVOID               DeviceContext,
    _Out_opt_   PUNKNOWN*           UnknownWave,
    _Out_opt_   PUNKNOWN*           UnknownMiniportWave
)
{
    PAGED_CODE();
    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_ADAPTER, "<%!FUNC!>");

    PUNKNOWN temp_UnknownWave;
    PUNKNOWN temp_UnknownMiniportWave;

    if (!GetCachedSubdevice(Miniport->WaveName, &temp_UnknownWave, &temp_UnknownMiniportWave))
    {
        TRY(InstallSubdevice(Irp,
                             Miniport->WaveName, // make sure this name matches with SIMPLEAUDIOSAMPLE.<WaveName>.szPname in the inf's [Strings] section
                             CLSID_PortWaveRT,
                             CLSID_PortWaveRT,
                             Miniport->WaveCreateCallback,
                             DeviceContext,
                             Miniport,
                             NULL,
                             IID_IPortWaveRT,
                             NULL,
                             &temp_UnknownWave,
                             &temp_UnknownMiniportWave
                            ));

        TRY_CLEAN(CacheSubdevice(Miniport->WaveName, temp_UnknownWave, temp_UnknownMiniportWave), { UnregisterSubdevice(temp_UnknownWave); });
    }

    if (UnknownWave)
    {
        *UnknownWave = temp_UnknownWave;
    }
    else
    {
        temp_UnknownWave->Release();
    }

    if (UnknownMiniportWave)
    {
        *UnknownMiniportWave = temp_UnknownMiniportWave;
    }
    else
    {
        temp_UnknownMiniportWave->Release();
    }

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_ADAPTER, "</%!FUNC!>");

    return STATUS_SUCCESS;
}

bool AdapterCommon::GetCachedSubdevice(_In_ PWSTR Name, _Out_opt_ PUNKNOWN* OutUnknownPort, _Out_opt_ PUNKNOWN* OutUnknownMiniport) _Success_(TRUE)
{
    PAGED_CODE();
    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_ADAPTER, "<%!FUNC!>");

    if (OutUnknownPort)
    {
        *OutUnknownPort = NULL;
    }
    if (OutUnknownMiniport)
    {
        *OutUnknownMiniport = NULL;
    }

    for (auto& miniport : m_Subdevices)
    {
        if (wcscmp(Name, miniport.Name) == 0)
        {
            if (OutUnknownPort)
            {
                *OutUnknownPort = miniport.PortInterface;
                if (*OutUnknownPort)
                {
                    (*OutUnknownPort)->AddRef();
                }
            }
            if (OutUnknownMiniport)
            {
                *OutUnknownMiniport = miniport.MiniportInterface;
                if (*OutUnknownMiniport)
                {
                    (*OutUnknownMiniport)->AddRef();
                }
            }
            return true;
        }
    }

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_ADAPTER, "</%!FUNC!>");

    return false;
}

NTSTATUS AdapterCommon::CacheSubdevice(_In_ PWSTR Name, _In_ PUNKNOWN UnknownPort, _In_ PUNKNOWN UnknownMiniport)
{
    PAGED_CODE();
    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_ADAPTER, "<%!FUNC!>");

    ListNode<Subdevice>* node;
    TRY(m_Subdevices.add(node));
    auto& subdevice = node->m_data;

    TRY_CLEAN(RtlStringCchCopyW(subdevice.Name, SIZEOF_ARRAY(subdevice.Name), Name), { m_Subdevices.remove(node); });
    subdevice.PortInterface = UnknownPort;
    subdevice.PortInterface->AddRef();
    subdevice.MiniportInterface = UnknownMiniport;
    subdevice.MiniportInterface->AddRef();

    // write PowerInterface from miniport, if available
    UnknownMiniport->QueryInterface(IID_IAdapterPowerManagement, (PVOID*) & (subdevice.PowerInterface));

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_ADAPTER, "</%!FUNC!>");

    return STATUS_SUCCESS;
}

/**
 * This function creates and registers a subdevice consisting of a port
 * driver, a minport driver and a set of resources bound together.  It will
 * also optionally place a pointer to an interface on the port driver in a
 * specified location before initializing the port driver.  This is done so
 * that a common ISR can have access to the port driver during
 * initialization, when the ISR might fire.
 *
 * @param Irp pointer to the irp object.
 * @param Name name of the miniport. Passes to PcRegisterSubDevice
 * @param PortClassId port class id. Passed to PcNewPort.
 * @param MiniportClassId miniport class id. Passed to PcNewMiniport.
 * @param MiniportCreate pointer to a miniport creation function. If NULL, PcNewMiniport is used.
 * @param DeviceContext deviceType specific.
 * @param Miniport endpoint configuration info.
 * @param ResourceList pointer to the resource list.
 * @param PortInterfaceId GUID that represents the port interface.
 * @param OutPortInterface pointer to store the port interface
 * @param OutPortUnknown pointer to store the unknown port interface.
 * @param OutMiniportUnknown pointer to store the unknown miniport interface
 */
STDMETHODIMP_(NTSTATUS) AdapterCommon::InstallSubdevice(
    _In_opt_        PIRP                                    Irp,
    _In_            PWSTR                                   Name,
    _In_            REFGUID                                 PortClassId,
    _In_            REFGUID                                 MiniportClassId,
    _In_opt_        PFNCREATEMINIPORT                       MiniportCreate,
    _In_opt_        PVOID                                   DeviceContext,
    _In_            PENDPOINT_MINIPORT                      Miniport,
    _In_opt_        PRESOURCELIST                           ResourceList,
    _In_            REFGUID                                 PortInterfaceId,
    _Out_opt_       PUNKNOWN*                               OutPortInterface,
    _Out_opt_       PUNKNOWN*                               OutPortUnknown,
    _Out_opt_       PUNKNOWN*                               OutMiniportUnknown
)
{
    PAGED_CODE();
    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_ADAPTER, "<%!FUNC!>");

    ASSERT(Name != NULL);
    ASSERT(m_pDeviceObject != NULL);

    TRY(CreateAudioInterface(Name));

    // Create the port driver object
    PPORT port = NULL;
    PUNKNOWN miniport = NULL;

#define CLEAN_CODE { release(port); release(miniport); }

    // TRY(PcNewPort(&port, PortClassId));
    UNREFERENCED_PARAMETER(PortClassId);

    // Create the miniport object
    if (MiniportCreate)
    {
        TRY_CLEAN(MiniportCreate(&miniport, NULL, this, DeviceContext, Miniport), CLEAN_CODE);
    }
    else
    {
        // TRY_CLEAN(PcNewMiniport((PMINIPORT*) &miniport, MiniportClassId), CLEAN_CODE);
        UNREFERENCED_PARAMETER(MiniportClassId);
    }

    // Init the port driver and miniport in one go.
    //
#pragma warning(push)
    // IPort::Init's annotation on ResourceList requires it to be non-NULL.  However,
    // for dynamic devices, we may no longer have the resource list and this should
    // still succeed.
    //
#pragma warning(disable:6387)
    TRY_CLEAN(port->Init(m_pDeviceObject, Irp, miniport, this, ResourceList), CLEAN_CODE);
#pragma warning (pop)

    // Register the subdevice (port/miniport combination).
    // TRY_CLEAN(PcRegisterSubdevice(m_pDeviceObject, Name, port), CLEAN_CODE);

    // Deposit the port interfaces if it's needed.
    if (OutPortUnknown)
    {
        port->QueryInterface(IID_IUnknown, (PVOID*)OutPortUnknown);
    }

    if (OutPortInterface)
    {
        port->QueryInterface(PortInterfaceId, (PVOID*) OutPortInterface);
    }

    if (OutMiniportUnknown)
    {
        miniport->QueryInterface(IID_IUnknown, (PVOID*)OutMiniportUnknown);
    }

    CLEAN_CODE;
#undef CLEAN_CODE

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_ADAPTER, "</%!FUNC!>");

    return STATUS_SUCCESS;
} // InstallSubDevice

/**
 * @brief Unregisters and releases the specified subdevice.
 *
 * @param UnknownPort Wave or topology port interface.
 */
STDMETHODIMP_(NTSTATUS) AdapterCommon::UnregisterSubdevice(_In_opt_ PUNKNOWN UnknownPort)
{
    PAGED_CODE();
    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_ADAPTER, "<%!FUNC!>");

    ASSERT(m_pDeviceObject != NULL);


    if (!UnknownPort)
    {
        return STATUS_SUCCESS;
    }

    // Get the IUnregisterSubdevice interface.
    PUNREGISTERSUBDEVICE unregisterSubdevice = NULL;
    TRY(UnknownPort->QueryInterface(IID_IUnregisterSubdevice, (PVOID*)&unregisterSubdevice));

    // Unregister the port object.
    TRY_CLEAN(unregisterSubdevice->UnregisterSubdevice(m_pDeviceObject, UnknownPort), { release(unregisterSubdevice); });

    unregisterSubdevice->Release();

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_ADAPTER, "</%!FUNC!>");

    return STATUS_SUCCESS;
}

NTSTATUS AdapterCommon::CreateAudioInterface(_In_ PCWSTR ReferenceString)
{
    PAGED_CODE();
    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_ADAPTER, "<%!FUNC!>");

    UNICODE_STRING  referenceString;
    UNICODE_STRING  symbolicLinkName;

    RtlInitUnicodeString(&referenceString, ReferenceString);
    RtlInitUnicodeString(&symbolicLinkName, NULL);

    TRY_CLEAN(IoRegisterDeviceInterface(GetPhysicalDeviceObject(), &KSCATEGORY_AUDIO, &referenceString, &symbolicLinkName), { RtlFreeUnicodeString(&symbolicLinkName); });

    RtlFreeUnicodeString(&symbolicLinkName);

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_ADAPTER, "</%!FUNC!>");

    return STATUS_SUCCESS;
}



NTSTATUS CreateMiniportWaveRTSimpleAudioSample(
    _Out_           PUNKNOWN*                               Unknown,
    _In_opt_        PUNKNOWN                                UnknownOuter,
    _In_            PUNKNOWN                                UnknownAdapter,
    _In_opt_        PVOID                                   DeviceContext,
    _In_            PENDPOINT_MINIPORT                      Miniport
)
{
    PAGED_CODE();

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_ADAPTER, "<%!FUNC!>");

    ASSERT(Unknown);
    ASSERT(Miniport);

    UNREFERENCED_PARAMETER(Unknown);
    UNREFERENCED_PARAMETER(UnknownOuter);
    UNREFERENCED_PARAMETER(UnknownAdapter);
    UNREFERENCED_PARAMETER(DeviceContext);
    UNREFERENCED_PARAMETER(Miniport);
    *Unknown = NULL;

    // CMiniportWaveRT* obj = new (PoolFlags, MINWAVERT_POOLTAG) CMiniportWaveRT
    // (
    //     UnknownAdapter,
    //     Miniport,
    //     DeviceContext
    // );
    // if (NULL == obj)
    // {
    //     return STATUS_INSUFFICIENT_RESOURCES;
    // }

    // obj->AddRef();
    // *Unknown = reinterpret_cast<IUnknown*>(obj);

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_ADAPTER, "</%!FUNC!>");

    return STATUS_SUCCESS;
}
NTSTATUS
PropertyHandler_WaveFilter
(
    _In_ PPCPROPERTY_REQUEST      PropertyRequest
)
/*++

Routine Description:

  Redirects general property request to miniport object

Arguments:

  PropertyRequest -

Return Value:

  NT status code.

--*/
{
    PAGED_CODE();

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_ADAPTER, "<%!FUNC!>");

    UNREFERENCED_PARAMETER(PropertyRequest);
    NTSTATUS ntStatus = STATUS_NOT_IMPLEMENTED;

    // NTSTATUS            ntStatus = STATUS_INVALID_DEVICE_REQUEST;
    // CMiniportWaveRT*    pWaveHelper = reinterpret_cast<CMiniportWaveRT*>(PropertyRequest->MajorTarget);

    // if (pWaveHelper == NULL)
    // {
    //     return STATUS_INVALID_PARAMETER;
    // }

    // pWaveHelper->AddRef();


    // if (IsEqualGUIDAligned(*PropertyRequest->PropertyItem->Set, KSPROPSETID_Pin))
    // {
    //     switch (PropertyRequest->PropertyItem->Id)
    //     {
    //     case KSPROPERTY_PIN_PROPOSEDATAFORMAT:
    //         ntStatus = pWaveHelper->PropertyHandlerProposedFormat(PropertyRequest);
    //         break;

    //     case KSPROPERTY_PIN_PROPOSEDATAFORMAT2:
    //         ntStatus = pWaveHelper->PropertyHandlerProposedFormat2(PropertyRequest);
    //         break;

    //     default:
    //         DPF(D_TERSE, ("[PropertyHandler_WaveFilter: Invalid Device Request]"));
    //     }
    // }

    // pWaveHelper->Release();

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_ADAPTER, "</%!FUNC!>");

    return ntStatus;
} // PropertyHandler_WaveFilter



ENDPOINT_MINIPORT SpeakerMiniports =
{
    eSpeakerDevice,
    L"WaveSpeaker",                                // make sure this or the template name matches with KSNAME_WaveSpeaker in the inf's [Strings] section
    CreateMiniportWaveRTSimpleAudioSample,
    &SpeakerWaveMiniportFilterDescriptor,
    SPEAKER_DEVICE_MAX_CHANNELS,
    SpeakerPinDeviceFormatsAndModes,
    SIZEOF_ARRAY(SpeakerPinDeviceFormatsAndModes),
    0,                                             // Endpoint flags
};

