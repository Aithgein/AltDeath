set_xmakever("3.0.0")

includes("lib/commonlibsse")

set_project("shades-respawn-addon")
set_version("0.2.0")
set_license("MIT")

set_languages("c++23")
set_warnings("allextra")
set_policy("package.requires_lock", true)

add_rules("mode.debug", "mode.releasedbg")
add_rules("plugin.vsxmake.autoupdate")
set_defaultmode("releasedbg")

set_config("commonlib_toml", false)

target("shades-respawn-addon")
    add_deps("commonlibsse")

    add_rules("commonlibsse.plugin", {
        name = "shades-respawn-addon",
        author = "Shades Respawn Addon contributors",
        description = "Teleports the player to the last successful sleep or wait location after Shades of Mortality resurrects them"
    })

    add_files("src/**.cpp")
    add_headerfiles("src/**.h")
    add_includedirs("src")
    set_pcxxheader("src/pch.h")
