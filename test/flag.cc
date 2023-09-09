#include "co/flag.h"
#include "co/cout.h"

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
    FLG_version = "v3.1.4";
    flag::alias("version", "v");
    auto args = flag::parse(argc, argv);

    co::print("boo: ", FLG_boo);
    co::print("x: ", FLG_x);
    co::print("y: ", FLG_y);
    co::print("z: ", FLG_z);
    co::print("n: ", FLG_n);

    co::print("i32: ", FLG_i32);
    co::print("i64: ", FLG_i64);
    co::print("u32: ", FLG_u32);
    co::print("u64: ", FLG_u64);

    co::print("dbl: ", FLG_dbl);
    co::print(FLG_s, "|", FLG_s.size());

    if (argc == 1) {
        co::print("\nYou may try running ", argv[0], " as below:");
        co::print(argv[0], "  -xz -i32 4k i64=8M u32=1g -s=xxx");
        co::print(argv[0], "  -v");
    }

    return 0;
}
