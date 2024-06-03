#include <nova/core/nova_Core.hpp>
#include <nova/core/win32/nova_Win32.hpp>
#include "shellapi.h"

using namespace nova::types;

i32 WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, i32)
{
    int argc;
    LPWSTR *argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    NOVA_DEFER(&) { LocalFree(argv); };
    ::ShellExecuteW(nullptr, argv[1], argv[2], nullptr, nullptr, SW_SHOW);
}