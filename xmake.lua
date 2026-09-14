set_config("plat", os.host())
set_project("co")
set_xmakever("2.5.6")

set_languages("c++17")
set_warnings("all")     -- -Wall
set_symbols("debug")    -- dbg symbols

if is_plat("windows") then
    set_optimize("fastest")
    add_cxflags("/EHsc")
    add_ldflags("/SAFESEH:NO")
elseif is_plat("mingw") then
    add_ldflags("-static-libgcc -static-libstdc++ -Wl,-Bstatic -lstdc++ -lwinpthread -Wl,-Bdynamic", {force = true})
    set_optimize("faster")
    add_cxflags("-fno-strict-aliasing")
    add_cxflags("-Wno-narrowing", "-Wno-sign-compare", "-Wno-strict-aliasing")
else
    set_optimize("faster")  -- faster: -O2  fastest: -O3  none: -O0
    add_cxflags("-fno-strict-aliasing")
    add_cxflags("-Wno-narrowing", "-Wno-sign-compare", "-Wno-strict-aliasing")
    if is_plat("macosx", "iphoneos") then
        add_cxflags("-fno-pie")
    end
end

option("with_backtrace")
    set_default(false)
    set_showmenu(true)
    set_description("build with libbacktrace")
option_end()

option("debug_coroutine")
    set_default(false)
    set_showmenu(true)
    set_description("print debug log for coroutine schedulers")
option_end()

-- build with -fPIC
option("fpic")
    set_default(false)
    set_showmenu(true)
    set_description("build with -fPIC")
    add_cxflags("-fPIC")
option_end()

-- include dir
add_includedirs("include")
if is_plat("macosx") then
    add_sysincludedirs("/usr/local/include")
    add_linkdirs("/usr/local/lib")
end

-- include sub-projects
includes("src", "benchmark", "gen", "test", "unitest")
