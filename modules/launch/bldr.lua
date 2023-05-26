if Project "nms" then
    Compile "src/**"
    Include {
        "src/indexer",
        "src/nomoreshortcuts",
        "src/database",
    }
    Import {
        "sqlite3",
        "nova",
    }
    Artifact { "out/NoMoreShortcuts", type = "Window" }
    Artifact { "out/nms", type = "Console" }
end