#include <nova/core/nova_Debug.hpp>
#include <file_searcher.hpp>

int main()
{
    std::string index_file = NOVA_FORMAT("{}\\.nms\\index.bin", getenv("USERPROFILE"));

    NOVA_LOG("Indexing filesystem to: {}", index_file);

    index_t index;

    index_filesystem(index);
    NOVA_LOG("Sorting...");
    sort_index(index);
    NOVA_LOG("Saving...");
    save_index(index, index_file.c_str());
    NOVA_LOG("Indexing complete, Press F5 in NoMoreShortcuts to reload index");
    NOVA_LOG("Press any key to close..");
    std::cin.get();
}
