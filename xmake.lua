-- project
set_project("co")

-- set xmake minimum version
set_xmakever("2.2.5")

-- set common flags
set_languages("c++11")
set_optimize("faster")  -- -O2
set_warnings("all")     -- -Wall
set_symbols("debug")    -- dbg symbols

if is_plat("macosx", "linux") then
    add_cxflags("-g3", "-Wno-narrowing")
    if is_plat("macosx") then
        add_cxflags("-fno-pie")
    end
    add_syslinks("pthread", "dl")
end

if is_plat("windows") then
    --add_cxflags("-Ox", "-fp:fast", "-EHsc")
    add_cxflags("-MD", "-EHsc")
end

-- install header files
add_installfiles("(base/*.h)", {prefixdir = "include"})
add_installfiles("(base/co/*.h)", {prefixdir = "include"})
add_installfiles("(base/hash/*.h)", {prefixdir = "include"})
add_installfiles("(base/unix/*.h)", {prefixdir = "include"})
add_installfiles("(base/win/*.h)", {prefixdir = "include"})
add_installfiles("(base/stack_trace/stack_trace.h)", {prefixdir = "include"})
add_installfiles("LICENSE", "readme*", {prefixdir = "include/base"})

-- include sub-projects
includes("base", "rpcgen", "test", "unitest/base")
