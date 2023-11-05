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
end

if Project "nms-cli" then
    Import "nms"
    Artifact { "out/nms", type = "Console" }
end

if Project "nms-app" then
    Import "nms"
    Artifact { "out/NoMoreShortcuts", type = "Window" }
end