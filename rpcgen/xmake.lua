target("rpcgen")
    set_kind("binary")
    set_default(false)
    add_deps("base")
    add_files("*.cc")
    set_rundir("$(projectdir)")

