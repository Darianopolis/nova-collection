if Project "bldr" then
    Compile "src/**"
    Include "src"
    -- Import {
    --     "luajit",
    --     "sol2",
    -- }
    Artifact { "out/bldr", type = "Console" }
end