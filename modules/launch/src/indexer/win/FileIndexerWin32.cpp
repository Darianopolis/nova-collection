#include "FileIndexer.hpp"

#include <nova/core/nova_Core.hpp>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

struct WinIndexer
{
    wchar_t path[32767];
    char utf8Buffer[1024];
    WIN32_FIND_DATA result;
    size_t count = 0;

    WinIndexer(const wchar_t* root)
    {
        wcscpy(path, root);
    }

    static void Index(Node* node, const wchar_t* root)
    {
        WinIndexer s(root);
        std::wcout << L"Indexing " << root << L'\n';
        s.Search(node, wcslen(root), 0);
    }

    void Search(Node* node, size_t offset, uint8_t depth)
    {
        path[offset] = '\\';
        path[offset + 1] = '*';
        path[offset + 2] = '\0';

        // std::wcout << L"in = " << path << L"\n";

        HANDLE findHandle = FindFirstFileEx(
            path,
            FindExInfoBasic,
            &result,
            FindExSearchNameMatch,
            nullptr,
            FIND_FIRST_EX_LARGE_FETCH);

        if (findHandle == INVALID_HANDLE_VALUE)
        {
            // NOVA_LOG("Invalid handle!");
            return;
        }

        do
        {
            size_t len = wcslen(result.cFileName);
            // Ignore current (.) and parent (..) directories!
            if (len == 0 || (result.cFileName[0] == '.' && (len == 1 || (len == 2 && result.cFileName[1] == '.'))))
                continue;

            BOOL usedDefault = FALSE;
            size_t utf8Len = WideCharToMultiByte(
                CP_UTF8,
                0,
                result.cFileName,
                (int)len,
                utf8Buffer,
                (int)sizeof(utf8Buffer) - 1,
                nullptr,
                &usedDefault);

            if (utf8Len == 0)
            {
                std::wcout << "Failed to convert " << result.cFileName << L"!\n";
                continue;
            }

            char* name = new char[utf8Len + 1];
            memcpy(name, utf8Buffer, utf8Len);
            name[utf8Len] = '\0';
            Node* child = new Node(name, (uint8_t)utf8Len, node, depth);
            node->AddChild(child);

            if (++count % 10000 == 0)
            {
                std::wcout << L"files = " << count << L", path = " << path << L"\n";
            }

            if (result.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            {
                memcpy(&path[offset + 1], result.cFileName, 2 * len);
                Search(child, offset + len + 1, depth == 255 ? 255 : depth + 1);
                // _search(nullptr, offset + len + 1, depth == 255 ? 255 : depth + 1);
            }
        }
        while (FindNextFile(findHandle, &result));

        FindClose(findHandle);
    }
};

Node* IndexDrive(char driverLetter)
{
    NOVA_LOG("Indexing drive: {}", driverLetter);
    char upper = (char)std::toupper(driverLetter);

    NOVA_LOG("Drive letter = {}", upper);

    auto* node = new Node(new char[] { upper, ':', '\\', '\0' }, 3, nullptr, 0);
    wchar_t init[] { L'\\', L'\\', L'?', L'\\', static_cast<wchar_t>(upper), L':', };
    WinIndexer::Index(node, init);

    NOVA_LOG("Indexed!");

    return node;

    // auto node = new Node(new char[] { "D:\\" }, 3, nullptr, 0);
    // WinIndexer::index(node, L"\\\\?\\D:");
}