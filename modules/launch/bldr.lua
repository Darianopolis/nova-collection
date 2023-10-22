if Project "nms" then
    Compile { "src/**" }
    Include {
        "src/nomoreshortcuts",
    }
    Import {
        "nova",
        "stb",
        "index",
    }
    Artifact { "out/NoMoreShortcuts", type = "Window" }
    Artifact { "out/nms", type = "Console" }
end