-- plat
set_config("plat", os.host())

-- project
set_project("co")

-- set xmake minimum version
set_xmakever("2.2.6")

-- set common flags
set_languages("c++11")
set_warnings("all")     -- -Wall
set_symbols("debug")    -- dbg symbols


if is_plat("windows") then
    set_optimize("fastest")  -- faster: -O2  fastest: -Ox  none: -O0
    add_defines("WIN32_LEAN_AND_MEAN")
    add_cxflags("-EHsc")
    if is_mode("debug") then
        set_runtimes("MTd")
    else
        set_runtimes("MT")
    end
else
    set_optimize("faster")   -- faster: -O2  fastest: -O3  none: -O0
    add_defines("_FILE_OFFSET_BITS=64")
    add_cxflags("-g3", "-Wno-narrowing", "-Wno-sign-compare", "-Wno-class-memaccess", "-Wno-strict-aliasing")
    if is_plat("macosx") then
        add_cxflags("-fno-pie")
    end
    if not is_plat("android") then
        add_syslinks("pthread", "dl")
    end
    if is_plat("android", "iphoneos") then
        add_defines("_CO_DISABLE_HOOK")
    end
end

option("with_openssl")
    set_default(false)
    set_showmenu(true)
    set_description("build with openssl, required by SSL features")
    add_defines("CO_SSL")
    add_defines("HAS_OPENSSL")
option_end()

option("with_libcurl")
    set_default(false)
    set_showmenu(true)
    set_description("build with libcurl, required by http::Client")
    add_defines("HAS_LIBCURL")
option_end()


if has_config("with_libcurl") then
    add_requires("openssl >=1.1.0")
    add_requires("libcurl", {configs = {openssl = true, zlib = true}})
elseif has_config("with_openssl") then
    add_requires("openssl >=1.1.0")
end 


-- include dir
add_includedirs("include")

-- install header files
add_installfiles("(include/**)", {prefixdir = ""})
add_installfiles("*.md", {prefixdir = "include/co"})

-- include sub-projects
includes("src", "gen", "test", "unitest", "benchmark")
