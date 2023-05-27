if Project "nms" then
    Compile { "src/**" }
    Include {
        "src/indexer",
        "src/nomoreshortcuts",
    }
    Import { "nova" }
    Artifact { "out/NoMoreShortcuts", type = "Window" }
    Artifact { "out/nms", type = "Console" }
end