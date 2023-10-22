#include <sol/sol.hpp>

#include <iostream>

int main() {
    sol::state lua;
    int x = 0;
    lua.set_function("beep", [&]{ ++x; });
    lua.script("beep()");
    std::cout << x << '\n';
}