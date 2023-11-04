if Begin "sol2" then
    Git { "https://github.com/ThePhD/sol2.git" }
    Include "include"
end

if Begin "luajit" then
    Git { "https://luajit.org/git/luajit.git" }
    Run { "msvcbuild.bat static", dir = "src" }
    Include { "src" }
    Link { "src/luajit.lib", "src/lua51.lib" }
end

if Begin "sqlite3" then
    Get {
        url = "https://www.sqlite.org/2023/sqlite-amalgamation-3430200.zip",
        flatten = "sqlite-amalgamation-3430200",
    }
    Compile "sqlite3.c"
    Include "."
end