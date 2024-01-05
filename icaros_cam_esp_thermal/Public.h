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

DEFINE_GUID (GUID_DEVINTERFACE_Icaros_KMD_ESP_Thermal,
    1273437320u,
    63784u,
    18120u,
    169u, 27u, 166u, 215u, 198u, 100u, 227u, 162u );
