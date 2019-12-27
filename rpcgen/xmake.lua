target("rpcgen")
    set_kind("binary")
    add_deps("base")
    add_files("*.cc")
    set_rundir("$(projectdir)")

