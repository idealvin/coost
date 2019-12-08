target("base")
    set_kind("static")
    add_cxxflags("-std=c++11")
    set_optimize("faster")
    set_targetdir("../lib")
    add_includedirs("..")
    add_files("**.cc")
    if is_plat("macosx", "linux") then
        add_cxflags("-g3", "-Wall")
        add_files("co/context/context.S")
    end 
    if is_plat("macosx") then
        add_cxflags("-fno-pie")
    end
    if is_plat("windows") and is_mode("release") then
        add_cxflags("-Ox", "-fp:fast")
        add_files("**.cpp")
        if is_arch("x64") then
            add_files("co/context/context_x64.asm")
        else
            add_files("co/context/context_x86.asm")
        end
    end
