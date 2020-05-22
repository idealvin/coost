target("libco")
    set_kind("static")
    set_basename("co")
    add_files("**.cc")

    -- define CODBG to enable debug log for co
    -- add_defines("CODBG")

    if is_plat("macosx", "linux") then
        add_files("co/context/context.S")
    end

    if is_plat("windows") then
        add_files("**.cpp")
        if is_arch("x64") then
            add_files("co/context/context_x64.asm")
        else
            add_files("co/context/context_x86.asm")
        end
    end

