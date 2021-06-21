target("gen")
    set_kind("binary")
    set_default(false)
    add_options("disable_hook")
    add_deps("libco")
    add_files("*.cc")
    set_rundir("$(projectdir)")

