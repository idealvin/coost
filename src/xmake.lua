target("libco")
    set_kind("static")
    set_basename("co")
    add_files("**.cc")

    if not is_plat("windows") then
        add_options("fpic")
    end

    add_options("debug_coroutine")
    if has_config("debug_coroutine") then
        add_defines("CO_DEBUG_COROUTINE")
    end

    if is_plat("windows", "mingw") then
        add_defines("WIN32_LEAN_AND_MEAN")
        add_defines("_WINSOCK_DEPRECATED_NO_WARNINGS")
        add_files("StackWalker.cpp")
        if is_plat("windows") then
            if is_arch("x64") then
                add_files("co/context/context_x64.asm")
            elseif is_arch("x86") then
                add_files("co/context/context_x86.asm")
            elseif is_arch("arm64") then
                add_files("co/context/context_arm64.asm")
            else
                print("arch not supported")
            end
        else
            add_defines("__MINGW_USE_VC2005_COMPAT=1") -- use 64bit time_t
            add_files("co/context/context.S")
            add_syslinks("ws2_32", { public = true })
        end
    else
        if is_os("ios") then
            add_defines("OS_IOS")
        end
        add_files("co/context/context.S")
        --if is_plat("macosx", "iphoneos") then
        --    add_files("hook/fishhook/fishhook.c")
        --end
        add_options("with_backtrace")
        if has_config("with_backtrace") then
            add_defines("WITH_BACKTRACE")
            add_syslinks("backtrace", { public = true })
        end
        if not is_plat("android") then
            add_syslinks("pthread", { public = true })
            add_syslinks("dl")
        end
    end
