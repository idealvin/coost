## Code generator

Generate code from proto file.


### Proto

```proto
// hello_world.proto
package xx   // namespace xx

// rpc service
service HelloWorld {  
    hello
    world
}

// supported base types:
//   bool, int, int32, uint32, int64, uint64, double, string
object X {
    string api
    data {  // anonymous object, field name can be put ahead
        bool b
        int i
        double d
        [int] ai   // array of int
        ao [{      // array of anonymous object
            int v
            string s
        }]
    }
}
```

For field of array or anonymous object type, we can put field name ahead.


### Build

```sh
# flex -o genl.cc genl.ll
# byacc -o geny.cc -H geny.hh geny.yy
xmake b gen
```

### Usage

```sh
gen xx.proto
gen  a.proto  b.proto

# use std::vector, std::string instead of co::vector and fastring
gen xx.proto -std
```
