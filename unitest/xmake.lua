target("unitest")
    set_kind("binary")
    set_default(false)
    add_deps("libco")
    add_files("*.cc")
    if is_plat("macosx") then
        after_build(function (target)
            local binary = target:targetfile()
            os.execv("dsymutil", {binary, "-o", binary .. ".dSYM"})
        end)
    end

