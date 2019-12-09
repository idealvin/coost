set_kind("binary")
add_cxxflags("-std=c++11")
set_optimize("faster")  -- -O2
set_warnings("all")     -- -Wall
set_targetdir("../build")
add_includedirs("..")
add_linkdirs("../lib")
add_links("base")

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

target("co")
    add_files("co_test.cc")

target("fast")
    add_files("fast_test.cc")

target("flag")
    add_files("flag_test.cc")

target("hash")
    add_files("hash_test.cc")

target("json")
    add_files("json_test.cc")

target("rapidjson")
    add_files("rapidjson_test.cc")

target("log")
    add_files("log_test.cc")

target("rpc")
    add_files("rpc_test.cc")

target("str")
    add_files("str_test.cc")

target("time")
    add_files("time_test.cc")

target("tw")
    add_files("tw_test.cc")

target("xx")
    add_files("xx_test.cc")
