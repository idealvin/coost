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
    auto args = flag::init(argc, argv);

    COUT << "boo: " << FLG_boo;
    COUT << "x: " << FLG_x;
    COUT << "y: " << FLG_y;
    COUT << "z: " << FLG_z;
    COUT << "n: " << FLG_n;

    COUT << "i32: " << FLG_i32;
    COUT << "i64: " << FLG_i64;
    COUT << "u32: " << FLG_u32;
    COUT << "u64: " << FLG_u64;

    COUT << "dbl: " << FLG_dbl;
    COUT << FLG_s << "|" << FLG_s.size();

    if (argc == 1) {
        COUT << "\nYou may try running " << argv[0] << " as below:";
        COUT << argv[0] << "  -xz i32=4k i64=8M u32=1g -s=xxx";
    }

    return 0;
}
