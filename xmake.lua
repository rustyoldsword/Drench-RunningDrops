set_xmakever("3.0.0")

includes("lib/CommonLibSSE-NG")

set_project("Drench")
set_version("1.1.0")
set_license("GPL-3.0-or-later")  -- the Running Drops edition embeds DripSim (GPL-3.0-or-later)
set_languages("c++23")
set_warnings("allextra")
set_encodings("utf-8")

add_rules("mode.debug", "mode.releasedbg")

add_requires("doctest 2.4.11")

set_runtimes("MD")
set_allowedplats("windows")
set_allowedarchs("x64")
set_defaultplat("windows")
set_defaultarchs("x64")

if is_plat("windows") then
    add_defines("_SILENCE_ALL_MS_EXT_DEPRECATION_WARNINGS", "WIN32_LEAN_AND_MEAN", "NOMINMAX", "NOGDI", "UNICODE", "_UNICODE")
    add_cxxflags("cl::/permissive-", "cl::/Zc:preprocessor", "cl::/EHsc", "cl::/utf-8")
    add_cxxflags("cl::/wd4068", "cl::/wd4201", "cl::/wd4251", "cl::/wd4275", "cl::/wd4267", "cl::/wd4244", "cl::/wd4996", "cl::/wd4100")
    -- release hygiene: no local build paths in the binaries (__FILE__ from headers, PDB path in the debug directory)
    add_cxxflags("cl::/d1trimfile:" .. os.projectdir() .. "\\", {force = true})
    add_shflags("/PDBALTPATH:%_PDB%", {force = true})
end

-- game-independent rules, shared by the plugin (stage 2) and the offline tests
target("DrenchCore", function()
    set_kind("static")
    add_files("src/core/*.cpp")
    add_headerfiles("src/core/*.h")
    add_includedirs("src", {public = true})
end)

-- the embedded DripSim (GPL-3.0-or-later, src/drip/LICENSE-DripSim.txt): drops that run down the player's skin.
-- Its own precompiled header and include paths, so its headers (Core.h, PCH.h) never meet Drench's.
target("DrenchDrips", function()
    set_kind("static")
    add_deps("commonlibsse-ng")
    add_files("src/drip/*.cpp", "src/drip/core/*.cpp", "src/drip/rain/*.cpp", "src/drip/sweat/*.cpp")
    remove_files("src/drip/core/SystemInfo.cpp")  -- only fed DripSim's own bug report; needs the real DXGI headers
    add_includedirs("src/drip", "src/drip/core", "src/drip/api", "src/drip/rain", "src/drip/sweat")
    add_defines("DSW_KIND=0", "DRIPSIM_SOAK_FLOW=1", 'DS_VERSION="2.0.0 (57a5d11, in Drench)"')
    set_pcxxheader("src/drip/core/PCH.h")
end)

target("Drench", function()
    add_rules("commonlibsse-ng.plugin", {
        name = "Drench",
        author = "Drench",
        description = "Wet skin framework: sweat, rain and water written straight into the skin shader"
    })
    add_deps("DrenchCore", "DrenchDrips")
    add_files("src/plugin/**.cpp")
    add_headerfiles("src/plugin/**.h")
    add_includedirs("src", "src/plugin")
    add_includedirs("lib/SKSE-Menu-Framework-3-API")
    set_pcxxheader("src/plugin/PCH.h")
end)

target("tests", function()
    set_kind("binary")
    set_default(false)
    add_deps("DrenchCore")
    add_files("tests/*.cpp")
    add_packages("doctest")
end)
