-- project
set_project("co")

-- set xmake minimum version
set_xmakever("2.2.5")

-- set common flags
set_languages("c++11")
set_optimize("faster")  -- -O2
set_warnings("all")     -- -Wall

if is_plat("macosx", "linux") then
    add_cxflags("-g3", "-Wno-narrowing")
    if is_plat("macosx") then
        add_cxflags("-fno-pie")
    end
    add_syslinks("pthread", "dl")
end

if is_plat("windows") then
    add_cxflags("-Ox", "-fp:fast", "-EHsc")
end

-- include sub-projects
includes("base", "rpcgen", "test", "unitest/base")
