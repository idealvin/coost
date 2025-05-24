target("libco")
    set_kind("static")
    set_basename("co")
    add_files("**.cc")

    if is_plat("linux") then
        add_options("with_backtrace")
    end
    add_options("co_debug_log")
    if has_config("co_debug_log") then
        add_defines("CO_DEBUG_LOG")
    end

    if is_plat("windows", "mingw") then
        add_defines("WIN32_LEAN_AND_MEAN")
        add_defines("_WINSOCK_DEPRECATED_NO_WARNINGS")
        if is_plat("mingw") then
            add_defines("__MINGW_USE_VC2005_COMPAT=1") -- use 64bit time_t
        end
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
            add_syslinks("ws2_32", { public = true })
            add_files("co/context/context.S")
        end
    else
        add_cxflags("-Wno-strict-aliasing")
        if is_os("ios") then
            add_defines("OS_IOS")
        end
        if not is_plat("android") then
            if is_plat("linux") then
                if has_config("with_backtrace") then
                    add_defines("WITH_BACKTRACE")
                    add_syslinks("backtrace", { public = true })
                end
            end
            add_syslinks("pthread", { public = true })
            --add_syslinks("dl")
        end
        add_files("co/context/context.S")
    end
