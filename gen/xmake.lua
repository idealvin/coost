target("gen")
    set_kind("binary")
    set_default(false)
    add_deps("libco")
    add_files("*.cc")
    set_rundir("$(projectdir)")

