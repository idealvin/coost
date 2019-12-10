target("rpcgen")
    set_kind("binary")
    add_cxxflags("-std=c++11")
    set_optimize("faster")  -- -O2
    set_warnings("all")     -- -Wall
    set_targetdir("../build")
    add_includedirs("..")
    add_linkdirs("../lib")
    add_links("base")
    add_files("*.cc")

    if is_plat("macosx", "linux") then
        add_cxflags("-g3")
        if is_plat("macosx") then
            add_cxflags("-fno-pie")
        end
        add_syslinks("pthread", "dl")
    end

    if is_plat("windows") then
        add_cxflags("-Ox", "-fp:fast", "-EHsc")
    end

    after_build(function ()
        os.rm("build")
    end)
