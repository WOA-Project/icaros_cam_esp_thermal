#pragma once

#include "Driver.h"

void EspDbgPrintlEx(
    _In_ int zone,
    _In_ PCSTR componentName,
    _In_z_ _Printf_format_string_ PCSTR format,
    ...);