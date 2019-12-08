target("unitest")
    set_kind("binary")
    add_cxxflags("-std=c++11")
    set_optimize("faster")
    set_targetdir("../../build")
    add_includedirs("../..")
    add_linkdirs("../../lib")
    add_links("base")
    add_files("*.cc")
    if is_plat("macosx", "linux") then
        add_cxflags("-g3", "-Wall")
        add_ldflags("-lpthread -ldl")
    end
    if is_plat("macosx") then
        add_cxflags("-fno-pie")
    end
    if is_plat("windows") then
        add_cxflags("-Ox", "-fp:fast")
    end
