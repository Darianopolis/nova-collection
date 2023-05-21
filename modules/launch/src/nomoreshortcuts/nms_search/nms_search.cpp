#define NOMINMAX
#include "Windows.h"

#include "NoMoreShortcuts.hpp"
#include <filesystem>

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow) {
// int main() {
    try
    {
        // AttachConsole(GetCurrentProcessId());
        // AllocConsole();
        // freopen("CONOUT$", "w", stdout);
        return AppMain();
    }
    catch (std::exception& e)
    {
        std::cout << "Error: " << e.what() << '\n';
    }
    catch (...)
    {
        std::cout << "Something went wrong!";
    }
    return 1;
}

// int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow) {}