#pragma once

#include "Prelude.h"
#include "List.h"

typedef enum
{
    eSpeakerDevice = 0,
    eMicArrayDevice1,
    eMaxDeviceType,
} eDeviceType;


//
// Enumeration of the various types of pins implemented in this driver.
//
typedef enum
{
    NoPin,
    BridgePin,
    SystemRenderPin,
    SystemCapturePin,
} PINTYPE;

//
// Signal processing modes and default formats structs.
//
typedef struct _MODE_AND_DEFAULT_FORMAT
{
    GUID            Mode;
    KSDATAFORMAT*   DefaultFormat;
} MODE_AND_DEFAULT_FORMAT, *PMODE_AND_DEFAULT_FORMAT;

//
// PIN_DEVICE_FORMATS_AND_MODES
//
//  Used to specify a pin's type (e.g. system, offload, etc.), formats, and
//  modes. Conceptually serves similar purpose as the PCPIN_DESCRIPTOR to
//  define a pin, but is more specific to driver implementation.
//
//  Arrays of these structures follow the same order as the filter's
//  pin descriptor array so that KS pin IDs can serve as an index.
//
typedef struct _PIN_DEVICE_FORMATS_AND_MODES
{
    PINTYPE                             PinType;

    KSDATAFORMAT_WAVEFORMATEXTENSIBLE* WaveFormats;
    ULONG                               WaveFormatsCount;

    MODE_AND_DEFAULT_FORMAT*            ModeAndDefaultFormat;
    ULONG                               ModeAndDefaultFormatCount;

} PIN_DEVICE_FORMATS_AND_MODES, *PPIN_DEVICE_FORMATS_AND_MODES;

typedef struct _ENDPOINT_MINIPORT* PENDPOINT_MINIPORT;

typedef HRESULT (*PFNCREATEMINIPORT)(
    _Out_           PUNKNOWN*                               Unknown,
    _In_opt_        PUNKNOWN                                UnknownOuter,
    _In_            PUNKNOWN                                UnknownAdapter,
    _In_opt_        PVOID                                   DeviceContext,
    _In_            PENDPOINT_MINIPORT                      Miniport
);


//
// Endpoint miniport descriptor.
//
typedef struct _ENDPOINT_MINIPORT
{
    eDeviceType                     DeviceType;

    // Wave RT miniport.
    PWSTR                           WaveName;               // make sure this or the template name matches with SIMPLEAUDIOSAMPLE.<WaveName>.szPname in the inf's [Strings] section
    PFNCREATEMINIPORT               WaveCreateCallback;
    PCFILTER_DESCRIPTOR*            WaveDescriptor;

    USHORT                          DeviceMaxChannels;
    PIN_DEVICE_FORMATS_AND_MODES*   PinDeviceFormatsAndModes;
    ULONG                           PinDeviceFormatsAndModesCount;

    // General endpoint flags (one of more ENDPOINT_<flag-type>, see above)
    ULONG                           DeviceFlags;
} ENDPOINT_MINIPORT, *PENDPOINT_MINIPORT;

extern ENDPOINT_MINIPORT SpeakerMiniports;

struct Subdevice
{
    WCHAR                   Name[MAX_PATH];
    PUNKNOWN                PortInterface;
    PUNKNOWN                MiniportInterface;
    PADAPTERPOWERMANAGEMENT PowerInterface;

    Subdevice()
    {
        RtlZeroMemory(this, sizeof(*this));
    }
    ~Subdevice()
    {
        release(PortInterface);
        release(MiniportInterface);
        release(PowerInterface);
    }
};

class AdapterCommon :
    public IAdapterPowerManagement,
    public CUnknown
{
private:
    PSERVICEGROUP           m_pServiceGroupWave = NULL;
    PDEVICE_OBJECT          m_pDeviceObject = NULL;
    PDEVICE_OBJECT          m_pPhysicalDeviceObject = NULL;
    WDFDEVICE               m_WdfDevice = NULL;            // Wdf device.
    DEVICE_POWER_STATE      m_PowerState = PowerDeviceUnspecified;

    // PCSimpleAudioSampleHW   m_pHW;                  // Virtual Simple Audio Sample HW object
    PPORTCLSETWHELPER       m_pPortClsEtwHelper;

    static LONG             m_AdapterInstances;     // # of adapter objects.

    DWORD                   m_dwIdleRequests;

public:
    static NTSTATUS Create(_Out_ AdapterCommon*& Out, _In_opt_ PUNKNOWN UnknownOuter);

    //=====================================================================
    // Default CUnknown
    DECLARE_STD_UNKNOWN();
    DEFINE_STD_CONSTRUCTOR(AdapterCommon);
    ~AdapterCommon();

    //=====================================================================
    // Default IAdapterPowerManagement
    IMP_IAdapterPowerManagement;

    //=====================================================================
    // IAdapterCommon methods

    STDMETHODIMP_(NTSTATUS) Init(_In_ PDEVICE_OBJECT DeviceObject);

    STDMETHODIMP_(PDEVICE_OBJECT) GetDeviceObject()
    {
        return m_pDeviceObject;
    }
    STDMETHODIMP_(PDEVICE_OBJECT) GetPhysicalDeviceObject()
    {
        return m_pPhysicalDeviceObject;
    }
    STDMETHODIMP_(WDFDEVICE) GetWdfDevice()
    {
        return m_WdfDevice;
    }

    STDMETHODIMP_(void)     SetWaveServiceGroup
    (
        _In_  PSERVICEGROUP   ServiceGroup
    );

    STDMETHODIMP_(BOOL)     bDevSpecificRead();

    STDMETHODIMP_(void)     bDevSpecificWrite
    (
        _In_  BOOL            bDevSpecific
    );
    STDMETHODIMP_(INT)      iDevSpecificRead();

    STDMETHODIMP_(void)     iDevSpecificWrite
    (
        _In_  INT             iDevSpecific
    );
    STDMETHODIMP_(UINT)     uiDevSpecificRead();

    STDMETHODIMP_(void)     uiDevSpecificWrite
    (
        _In_  UINT            uiDevSpecific
    );

    STDMETHODIMP_(BOOL)     MixerMuteRead
    (
        _In_  ULONG           Index,
        _In_  ULONG           Channel
    );

    STDMETHODIMP_(void)     MixerMuteWrite
    (
        _In_  ULONG           Index,
        _In_  ULONG           Channel,
        _In_  BOOL            Value
    );

    STDMETHODIMP_(ULONG)    MixerMuxRead(void);

    STDMETHODIMP_(void)     MixerMuxWrite
    (
        _In_  ULONG           Index
    );

    STDMETHODIMP_(void)     MixerReset(void);

    STDMETHODIMP_(LONG)     MixerVolumeRead
    (
        _In_  ULONG           Index,
        _In_  ULONG           Channel
    );

    STDMETHODIMP_(void)     MixerVolumeWrite
    (
        _In_  ULONG           Index,
        _In_  ULONG           Channel,
        _In_  LONG            Value
    );

    STDMETHODIMP_(LONG)     MixerPeakMeterRead
    (
        _In_  ULONG           Index,
        _In_  ULONG           Channel
    );

    STDMETHODIMP_(NTSTATUS) WriteEtwEvent
    (
        _In_ EPcMiniportEngineEvent    miniportEventType,
        _In_ ULONGLONG      ullData1,
        _In_ ULONGLONG      ullData2,
        _In_ ULONGLONG      ullData3,
        _In_ ULONGLONG      ullData4
    );

    STDMETHODIMP_(VOID)     SetEtwHelper
    (
        PPORTCLSETWHELPER _pPortClsEtwHelper
    );

    STDMETHODIMP_(NTSTATUS) InstallSubdevice
    (
        _In_opt_        PIRP                                        Irp,
        _In_            PWSTR                                       Name,
        _In_            REFGUID                                     PortClassId,
        _In_            REFGUID                                     MiniportClassId,
        _In_opt_        PFNCREATEMINIPORT                           MiniportCreate,
        _In_opt_        PVOID                                       DeviceContext,
        _In_            PENDPOINT_MINIPORT                          Miniport,
        _In_opt_        PRESOURCELIST                               ResourceList,
        _In_            REFGUID                                     PortInterfaceId,
        _Out_opt_       PUNKNOWN*                                   OutPortInterface,
        _Out_opt_       PUNKNOWN*                                   OutPortUnknown,
        _Out_opt_       PUNKNOWN*                                   OutMiniportUnknown
    );

    STDMETHODIMP_(NTSTATUS) UnregisterSubdevice
    (
        _In_opt_ PUNKNOWN               UnknownPort
    );

    STDMETHODIMP_(NTSTATUS) InstallEndpointFilters
    (
        _In_opt_    PIRP                Irp,
        _In_        PENDPOINT_MINIPORT  Miniport,
        _In_opt_    PVOID               DeviceContext,
        _Out_opt_   PUNKNOWN*           UnknownWave,
        _Out_opt_   PUNKNOWN*           UnknownMiniportWave
    );

    STDMETHODIMP_(NTSTATUS) RemoveEndpointFilters
    (
        _In_        PENDPOINT_MINIPORT  Miniport,
        _In_opt_    PUNKNOWN            UnknownWave
    );

    STDMETHODIMP_(NTSTATUS) GetFilters
    (
        _In_        PENDPOINT_MINIPORT  Miniport,
        _Out_opt_   PUNKNOWN*            UnknownWavePort,
        _Out_opt_   PUNKNOWN*            UnknownWaveMiniport
    );

    STDMETHODIMP_(NTSTATUS) SetIdlePowerManagement
    (
        _In_        PENDPOINT_MINIPORT  Miniport,
        _In_        BOOL                bEnabled
    );

    STDMETHODIMP_(VOID) Cleanup();

private:

    List<Subdevice> m_Subdevices;

    bool GetCachedSubdevice(_In_ PWSTR Name, _Out_opt_ PUNKNOWN* OutUnknownPort, _Out_opt_ PUNKNOWN* OutUnknownMiniport);

    NTSTATUS CacheSubdevice
    (
        _In_ PWSTR Name,
        _In_ PUNKNOWN UnknownPort,
        _In_ PUNKNOWN UnknownMiniport
    );

    NTSTATUS RemoveCachedSubdevice
    (
        _In_ PWSTR Name
    );

    VOID EmptySubdeviceCache();

    /**
     * @brief Create the audio interface (in disabled mode).
     *
     * @param ReferenceString Name of the audio interface in the inf file.
     * @return NTSTATUS
     */
    NTSTATUS CreateAudioInterface(_In_ PCWSTR ReferenceString);
};

