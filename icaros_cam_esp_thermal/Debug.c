#include "Debug.h"

void EspDbgPrintlEx(
    _In_ int zone,
    _In_ PCSTR componentName,
    _In_z_ _Printf_format_string_ PCSTR format,
    ...)
{
    va_list varg_r3 = ""; // [sp+24h] [bp+14h] BYREF

    va_start(varg_r3, format);

    if (KeGetCurrentIrql() != 2 && format)
    {
        if (componentName != NULL)
        {
            DbgPrintEx(81, zone, "[%s] ", componentName);
            vDbgPrintEx(81, zone, format, varg_r3);
            DbgPrintEx(81, zone, "\n");
        }
    }
}