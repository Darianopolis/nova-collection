#include "fs_index.hpp"

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

    static void index(Node* node, const wchar_t* root)
    {
        WinIndexer s(root);
        std::wcout << "Indexing " << root << L"\n";
        s._search(node, wcslen(root), 0);
    }

    void _search(Node* node, size_t offset, uint8_t depth)
    {
        path[offset] = '\\';
        path[offset + 1] = '*';
        path[offset + 2] = '\0';

        // std::wcout << L"in = " << path << L"\n";

        HANDLE find_handle = FindFirstFileEx(
            path,
            FindExInfoBasic,
            &result,
            FindExSearchNameMatch,
            nullptr,
            FIND_FIRST_EX_LARGE_FETCH);

        if (find_handle == INVALID_HANDLE_VALUE)
            return;

        do
        {
            size_t len = wcslen(result.cFileName);
            // Ignore current (.) and parent (..) directories!
            if (len == 0 || (result.cFileName[0] == '.' && (len == 1 || (len == 2 && result.cFileName[1] == '.'))))
                continue;

            bool used_default = false;
            size_t utf8_len = WideCharToMultiByte(
                CP_UTF8,
                0,
                result.cFileName,
                len,
                utf8Buffer,
                sizeof(utf8Buffer) - 1,
                nullptr,
                (LPBOOL)&used_default);

            if (utf8_len == 0)
            {
                std::wcout << "Failed to convert " << result.cFileName << L"!\n";
                continue;
            }

            char *name = new char[utf8_len + 1];
            memcpy(name, utf8Buffer, utf8_len);
            name[utf8_len] = '\0';
            Node *child = new Node(name, utf8_len, node, depth);
            node->add_child(child);

            if (++count % 10000 == 0)
            {
                std::wcout << L"files = " << count << L", path = " << path << L"\n";
            }

            if (result.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            {
                memcpy(&path[offset + 1], result.cFileName, 2 * len);
                _search(child, offset + len + 1, depth == 255 ? 255 : depth + 1);
                // _search(nullptr, offset + len + 1, depth == 255 ? 255 : depth + 1);
            }
        }
        while (FindNextFile(find_handle, &result));

        FindClose(find_handle);
    }
};

Node* index_drive(char drive_letter)
{
    char upper = std::toupper(drive_letter);

    std::cout << "Drive letter = " << upper << '\n';

    auto *node = new Node(new char[] { upper, ':', '\\', '\0' }, 3, nullptr, 0);
    wchar_t init[] { L'\\', L'\\', L'?', L'\\', static_cast<wchar_t>(upper), L':', };
    WinIndexer::index(node, init);

    std::cout << "Indexed!\n";

    return node;

    // auto node = new Node(new char[] { "D:\\" }, 3, nullptr, 0);
    // WinIndexer::index(node, L"\\\\?\\D:");
}