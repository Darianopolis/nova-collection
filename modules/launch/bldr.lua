if Project "nms-search" then
    Compile { "src/nms-search/**" }
    Include "src"
    Import { "nova", "fs-indexer" }
    Embed { "assets/SEGOEUI.TTF", "assets/SEGUISB.TTF" }
    Artifact { "out/nms-search", type = "Window" }
end

if Project "nms-index" then
    Compile "src/nms-index/**"
    Import { "nova", "fs-indexer" }
    Artifact { "out/nms-index", type = "Console" }
end

if Project "nms-launch" then
    Compile "src/nms-launch/**"
    Import { "nova" }
    Artifact { "out/nms-launch", type = "Window" }
end
