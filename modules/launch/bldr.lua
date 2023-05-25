if Project "nms-v2" then
    Compile {
        "src/common/**",
        "src/indexer/**",
        "src/overlay/*", -- Don't compile examples
        "src/overlay/directx/*",
        "src/nomoreshortcuts/**",
        -- "src/overlay/example/BoxDemo.cpp"
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
        "nova",
    }

    Artifact { "out/NoMoreShortcuts", type = "Window" }
    -- Artifact { "out/main", type = "Console" }
end

if Project "nms-v2-box" then
    Compile {
        "src/boxtest/**",

        "src/nomoreshortcuts/core/Platform.cpp",
    }
    Include {
        "src/common",
        "src/indexer",
        "src/overlay",
        "src/nomoreshortcuts",
        "src/nomoreshortcuts/core",
    }
    Import {
        "nova",
    }
    Artifact { "out/main", type = "Console" }
end