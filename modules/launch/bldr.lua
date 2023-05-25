if Project "nms-v2" then
    Compile {
        "src/common/**",
        "src/indexer/**",
        "src/nomoreshortcuts/**",
    }
    Include {
        "src/common",
        "src/indexer",
        "src/nomoreshortcuts",
        "src/nomoreshortcuts/core",
    }
    Import {
        "sqlite3",
        "nova",
    }

    Artifact { "out/NoMoreShortcuts", type = "Window" }
    -- Artifact { "out/main", type = "Console" }
end