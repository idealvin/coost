target("libco")
    set_kind("$(kind)")
    set_basename("co")
    add_files("**.cc")

    add_options("codbg")
    add_options("with_openssl")
    add_options("with_libcurl")

    if is_plat("windows") then
        add_files("**.cpp")
        if is_arch("x64") then
            add_files("co/context/context_x64.asm")
        else
            add_files("co/context/context_x86.asm")
        end
    else
        add_files("co/context/context.S")
    end

