if Project "nms-v2" then
    Compile {
        "src/common/**",
        "src/indexer/**",
        "src/overlay/*", -- Don't compile examples
        "src/overlay/directx/*",
        "src/winwrap/**",
        "src/nomoreshortcuts/**",
    }
    Include {
        "src/common",
        "src/indexer",
        "src/overlay",
        "src/winwrap",
        "src/nomoreshortcuts",
        "src/nomoreshortcuts/core",
    }
    Import {
        "sqlite3",
    }
end

if Project "nms-v2-search" then
    Compile "src/nomoreshortcuts/nms_search"
    Import "nms-v2"
    Artifact { "out/NoMoreShortcuts", type = "Window" }
end

if Project "nms-v2-update" then
    Compile "src/nomoreshortcuts/nms_update"
    Import "nms-v2"
    Artifact { "bin/nms", type = "Console" }
end