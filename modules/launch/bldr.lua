if Project "nms-v2" then
    Compile "src/**"
    Include {
        "src/indexer",
        "src/nomoreshortcuts",
    }
    Import {
        "sqlite3",
        "nova",
    }
    Artifact { "out/NoMoreShortcuts", type = "Window" }
    Artifact { "out/nms", type = "Console" }
end