if Project "nms" then
    Compile { "src/**" }
    Include {
        "src/indexer",
        "src/nomoreshortcuts",
    }
    Import {
        "nova",
        "stb",
    }
    Artifact { "out/NoMoreShortcuts", type = "Window" }
    Artifact { "out/nms", type = "Console" }
end