if Project "bldr" then
    Compile "src/**"
    Include "src"
    Artifact { "out/bldr", type = "Console" }
end