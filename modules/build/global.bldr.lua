local cloned_dir = "vendor/"

if Project "luajit" then
    Dir(cloned_dir.."luajit")
    Include "src"
    Link { "src/luajit.lib", "src/lua51.lib" }
end

if Project "sol2" then
    Dir(cloned_dir.."sol2")
    Include "include"
end