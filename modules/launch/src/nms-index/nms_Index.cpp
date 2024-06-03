#include <nova/core/nova_Core.hpp>
#include <file_searcher.hpp>

int main()
{
    std::string index_file = nova::Fmt("{}\\.nms\\index.bin", getenv("USERPROFILE"));

    nova::Log("Indexing filesystem to: {}", index_file);

    index_t index;

    index_filesystem(index);
    nova::Log("Sorting...");
    sort_index(index);
    nova::Log("Saving...");
    save_index(index, index_file.c_str());
    nova::Log("Indexing complete, Press F5 in NoMoreShortcuts to reload index");
    nova::Log("Press any key to close..");
    std::cin.get();
}
