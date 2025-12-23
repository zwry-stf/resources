workspace "resources"
    configurations { "Release" }
    platforms { "x86", "x64" }
    language "C++"
    cppdialect "C++23"
    staticruntime "Off"
    
    filter "platforms:x86"
        architecture "x86"
    filter "platforms:x64"
        architecture "x86_64"
    filter {}
    
    local build_root = "build/%{prj.name}"
    local int_root   = "build/%{prj.name}/%{cfg.buildcfg}/%{cfg.platform}"
    
    flags { "MultiProcessorCompile" }
    warnings "Extra"
    fatalwarnings { "All" }
    
    filter "action:vs*"
        buildoptions { "/sdl" }
    filter {}
    
    optimize "On"
    intrinsics "On"
    linktimeoptimization "On"
    defines { "NDEBUG" }

    filter "action:vs*"
        buildoptions { "/Gy" }
    filter {}

project "resources"
    kind "ConsoleApp"
    targetname "%{prj.name}_%{cfg.platform}"
    targetdir (build_root)
    objdir    (int_root)
    location "resources"
    
    files {
        "resources/include/resources/**.h",
        "resources/main.cpp",
    }

    includedirs {
        "resources/include"
        "resources/ext"
    }