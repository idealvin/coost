-- plat
set_config("plat", os.host())

-- project
set_project("co")

-- set xmake minimum version
set_xmakever("2.3.1")

-- set common flags
set_languages("c++11")
set_warnings("all")     -- -Wall
--set_symbols("debug")    -- dbg symbols
--add_rules("mode.debug", "mode.release")


if is_plat("windows") then
    set_optimize("fastest")  -- faster: -O2  fastest: -Ox  none: -O0
    add_cxflags("/EHsc")
    add_ldflags("/SAFESEH:NO")
    if is_mode("debug") then
        set_runtimes("MTd")
    else
        set_runtimes("MT")
    end
elseif is_plat("mingw") then
    add_ldflags("-static-libgcc -static-libstdc++ -Wl,-Bstatic -lstdc++ -lwinpthread -Wl,-Bdynamic", {force = true})
    set_optimize("faster")
else
    --set_optimize("none")   -- faster: -O2  fastest: -O3  none: -O0
    set_optimize("faster")   -- faster: -O2  fastest: -O3  none: -O0
    --add_cxflags("-Wno-narrowing", "-Wno-sign-compare", "-Wno-strict-aliasing")
    if is_plat("macosx", "iphoneos") then
        add_cxflags("-fno-pie")
    end
end


-- build with openssl 1.1.0+
option("with_openssl")
    set_default(false)
    set_showmenu(true)
    set_description("build with openssl, 1.1.0+ required")
option_end()

-- build with libcurl (openssl, zlib also required)
option("with_libcurl")
    set_default(false)
    set_showmenu(true)
    set_description("build with libcurl, required by http::Client")
option_end()

option("with_backtrace")
    set_default(false)
    set_showmenu(true)
    set_description("build with libbacktrace, for stack trace on linux/mac")
option_end()

-- build with -fPIC
option("fpic")
    set_default(false)
    set_showmenu(true)
    set_description("build with -fPIC")
    add_cxflags("-fPIC")
option_end()

option("disable_hook")
    set_default(false)
    set_showmenu(true)
    set_description("disable system API hook")
option_end()

option("cache_line_size")
    set_default("64")
    set_showmenu(true)
    set_description("set value of L1 cache line size")
option_end()

if has_config("with_libcurl") then
    add_requires("openssl >=1.1.0")
    add_requires("libcurl", {configs = {openssl = true, zlib = true}})
elseif has_config("with_openssl") then
    add_requires("openssl >=1.1.0")
end 

if has_config("with_backtrace") then
    add_requires("libbacktrace")
end


-- include dir
add_includedirs("include")

-- install header files
add_installfiles("(include/**)", {prefixdir = ""})
add_installfiles("*.md", {prefixdir = "include/co"})

-- include sub-projects
includes("src", "gen", "test", "unitest")
