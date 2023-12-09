if Project "luajit" then
     Dir "build/vendor/luajit"
     Include "src"
     Link { "src/luajit.lib", "src/lua51.lib" }
end

if Project "sol2" then
    Dir "build/vendor/sol2"
    Include "include"
end

if Project "bldr" then
    Compile "src/**"
    Include "src"
    Import {
        "luajit",
        "sol2",
    }
    Artifact { "bin/bldr", type = "Console" }
end