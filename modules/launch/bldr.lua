if Project "nms-v2" then
    Compile {
        "src/indexer/**",
        "src/nomoreshortcuts/**",
    }
    Include {
        "src/indexer",
        "src/nomoreshortcuts",
        "src/nomoreshortcuts/core",
    }
    Import {
        "sqlite3",
        "nova",
    }

    Artifact { "out/NoMoreShortcuts", type = "Window" }
    Artifact { "out/nms", type = "Console" }
end