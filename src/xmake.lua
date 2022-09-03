option("libbacktrace")
    add_cincludes("backtrace.h")
option_end()

option("cxxabi")
    add_cxxincludes("cxxabi.h")
option_end()


target("libco")
    set_kind("$(kind)")
    set_basename("co")
    add_files("**.cc")
    add_options("with_openssl")
    add_options("with_libcurl")
    if not is_plat("windows") then
        add_options("fpic")
    end

    if has_config("with_libcurl") then
        add_defines("HAS_LIBCURL")
        add_defines("HAS_OPENSSL")
        add_packages("libcurl")
        add_packages("openssl")
    elseif has_config("with_openssl") then
        add_defines("HAS_OPENSSL")
        add_packages("openssl")
    end 

    if is_kind("shared") then
        set_symbols("debug", "hidden")
        add_defines("BUILDING_CO_SHARED")
        set_configvar("COOST_SHARED", 1)
    else
        set_configvar("COOST_SHARED", 0)
    end
    add_configfiles("../include/co/config.h.in", {filename = "../include/co/config.h"})

    if is_plat("windows", "mingw") then
        add_defines("WIN32_LEAN_AND_MEAN")
        add_defines("_WINSOCK_DEPRECATED_NO_WARNINGS")
        add_files("log/StackWalker.cpp")
        add_files("co/detours/creatwth.cpp")
        add_files("co/detours/detours.cpp")
        add_files("co/detours/image.cpp")
        add_files("co/detours/modules.cpp")
        add_files("co/detours/disasm.cpp")
        if is_plat("windows") then
            if is_arch("x64") then
                add_files("co/context/context_x64.asm")
            else
                add_files("co/context/context_x86.asm")
            end
        else
            add_defines("__MINGW_USE_VC2005_COMPAT=1") -- use 64bit time_t
            add_syslinks("ws2_32", { public = true })
            add_files("co/context/context.S")
        end
    else
        add_cxflags("-Wno-strict-aliasing")
        if has_config("libbacktrace") then
            add_defines("HAS_BACKTRACE_H")
            add_syslinks("backtrace", { public = true })
        end
        if has_config("cxxabi") then
            add_defines("HAS_CXXABI_H")
        end
        if not is_plat("android") then
            add_syslinks("pthread", { public = true })
            add_syslinks("dl")
        end
        add_files("co/context/context.S")
    end

    if is_plat("macosx", "iphoneos") then
        add_files("co/fishhook/fishhook.c")
    end
