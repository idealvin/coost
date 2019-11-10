#include "base/unitest.h"
#include "base/lru_map.h"

namespace test {

DEF_test(lru_map) {
    LruMap<int, int> m(4);
    m.insert(1, 1);
    m.insert(2, 2);
    m.insert(3, 3);
    m.insert(4, 4);

    EXPECT_EQ(m.size(), 4);
    EXPECT_EQ(m.find(1)->second, 1);

    m.insert(5, 5);
    EXPECT_EQ(m.size(), 4);
    EXPECT(m.find(2) == m.end());

    m.erase(5);
    EXPECT_EQ(m.size(), 3);

    auto it = m.find(1);
    m.erase(it);
    EXPECT_EQ(m.size(), 2);
    EXPECT(m.find(1) == m.end());

    m.clear();
    EXPECT(m.empty());
}

} // namespace test
