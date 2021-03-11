-- plat
set_config("plat", os.host())

-- project
set_project("co")

-- set xmake minimum version
set_xmakever("2.2.5")

-- set common flags
set_languages("c++11")
set_optimize("faster")  -- -O2
set_warnings("all")     -- -Wall
set_symbols("debug")    -- dbg symbols

-- check sse/avx
includes("check_cincludes.lua")
check_cincludes("AVX", "immintrin.h")
check_cincludes("SSE2", "emmintrin.h")
check_cincludes("SSE42", "nmmintrin.h")
add_vectorexts("sse2","avx")


if is_plat("macosx", "linux") then
    add_cxflags("-g3", "-Wno-narrowing")
    if is_plat("macosx") then
        add_cxflags("-fno-pie")
    end
    add_syslinks("pthread", "dl")
end

if is_plat("windows") then
    add_cxflags("-MD", "-EHsc")
end


-- include dir
add_includedirs("include")

-- install header files
add_installfiles("(include/**)", {prefixdir = ""})
add_installfiles("*.md", {prefixdir = "include/co"})

-- include sub-projects
includes("src", "gen", "test", "unitest")
