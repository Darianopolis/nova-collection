if Project "nms-v2" then
    Compile {
        "src/common/**",
        "src/indexer/**",
        "src/overlay/*", -- Don't compile examples
        "src/overlay/directx/*",
        "src/nomoreshortcuts/**",
    }
    Include {
        "src/common",
        "src/indexer",
        "src/overlay",
        "src/nomoreshortcuts",
        "src/nomoreshortcuts/core",
    }
    Import {
        "sqlite3",
    }

    Artifact { "out/NoMoreShortcuts", type = "Window" }
end