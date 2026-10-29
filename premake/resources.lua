resource_library = resource_library or {}

function resource_library.add_decoder(opts, groups)
    local base = opts.base
    local zstd = path.join(base, "resources/ext/zstd/lib")

    project "resource_zstd"
        kind "StaticLib"
        targetname "%{prj.name}_%{cfg.buildcfg}_%{cfg.platform}"
        targetdir(opts.build_root)
        objdir(opts.int_root)
        location(path.join(base, "out/zstd"))
        opts.settings(groups)
        language "C"
        warnings "Off"
        defines { "ZSTD_DISABLE_ASM", "ZSTD_LEGACY_SUPPORT=0" }
        files { path.join(zstd, "common/*.c"), path.join(zstd, "decompress/*.c") }
        includedirs { zstd }

    project "resource_decoder"
        kind "StaticLib"
        targetname "%{prj.name}_%{cfg.buildcfg}_%{cfg.platform}"
        targetdir(opts.build_root)
        objdir(opts.int_root)
        location(path.join(base, "out/decoder"))
        opts.settings(groups)
        files { path.join(base, "resources/include/resources/**.h"), path.join(base, "resources/src/parser.cpp") }
        includedirs { path.join(base, "resources/include"), zstd }
        links { "resource_zstd" }
end