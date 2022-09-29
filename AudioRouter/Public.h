/*++

Module Name:

    public.h

Abstract:

    This module contains the common declarations shared by driver
    and user applications.

Environment:

    user and kernel

--*/

//
// Define an Interface Guid so that apps can find the device and talk to it.
//

DEFINE_GUID (GUID_DEVINTERFACE_AudioRouter,
    0xc9b7d8ce,0x7a5f,0x4165,0xb0,0xf9,0xee,0x1a,0x68,0x3c,0xfb,0xd8);
// {c9b7d8ce-7a5f-4165-b0f9-ee1a683cfbd8}
