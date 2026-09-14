#include "co/flag.h"
#include "co/print.h"

DEF_bool(boo, false, "bool flag");
DEF_bool(x, false, "bool x");
DEF_bool(y, false, "bool y");
DEF_bool(z, false, "bool z");
DEF_int32(n, 0, "int32");

DEF_int32(i32, -32, "int32");
DEF_int64(i64, -64, "int64");
DEF_uint32(u32, 32, "uint32");
DEF_uint64(u64, 64, "uint64");

DEF_double(dbl, 3.14, "double");
DEF_string(s, "hello world", "string");

int main(int argc, char** argv) {
    flag::set_program_version("v3.1.4");
    flag::alias("version", "v");
    auto args = flag::parse(argc, argv);

    co::println("boo: ", FLG_boo);
    co::println("x: ", FLG_x);
    co::println("y: ", FLG_y);
    co::println("z: ", FLG_z);
    co::println("n: ", FLG_n);

    co::println("i32: ", FLG_i32);
    co::println("i64: ", FLG_i64);
    co::println("u32: ", FLG_u32);
    co::println("u64: ", FLG_u64);

    co::println("dbl: ", FLG_dbl);
    co::println("s: ", FLG_s, "|", FLG_s.size());

    if (argc == 1) {
        co::println("\nYou may try running with following args:");
        co::println("  --help");
        co::println("  -v");
        co::println("  -xz -i32 4k -i64 8M -u32 1g -s xxx");
    }

    return 0;
}
