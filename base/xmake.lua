target("base")
    set_kind("static")
    add_includedirs("..", {interface = true})
    add_files("**.cc")

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

