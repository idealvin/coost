target("base")
    set_kind("static")
    add_cxxflags("-std=c++11")
    set_optimize("faster")  -- -O2
    set_warnings("all")     -- -Wall
    set_targetdir("../lib")
    add_files("**.cc")

    if is_plat("macosx", "linux") then
        add_cxflags("-g3")
        if is_plat("macosx") then
            add_cxflags("-fno-pie")
        end
        add_files("co/context/context.S")
    end

    if is_plat("windows") then
        add_cxflags("-Ox", "-fp:fast", "-EHsc")
        add_files("**.cpp")
        if is_arch("x64") then
            add_files("co/context/context_x64.asm")
        else
            add_files("co/context/context_x86.asm")
        end
    end

    after_build(function ()
        os.rm("build")
    end)
