target("unitest")
    set_kind("binary")
    add_cxxflags("-std=c++11")
    set_optimize("faster")  -- -O2
    set_warnings("all")     -- -Wall
    set_targetdir("../../build")
    add_includedirs("../..")
    add_linkdirs("../../lib")
    add_links("base")
    add_files("*.cc")

    if is_plat("macosx", "linux") then
        add_cxflags("-g3")
        if is_plat("macosx") then
            add_cxflags("-fno-pie")
        end
        add_links("pthread", "dl")
        after_build(function ()
            os.run("rm -rf build")
        end)
    end

    if is_plat("windows") then
        add_cxflags("-Ox", "-fp:fast", "-EHsc")
        after_build(function ()
            os.run("cmd /k rd /s /q build; exit")
        end)
    end
