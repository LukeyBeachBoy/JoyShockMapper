#include "TritonGripSettings.h"
#include <cassert>
#include <limits>
#include <iostream>
int main()
{
    using namespace triton_grip;
    assert(threshold(-1) == -1 && hysteresis(-1) == -1);
    assert(threshold(0) == 25 && threshold(32767) == 400);
    assert(hysteresis(32767) == 100 && hysteresis(0) == 0);
    assert(threshold(std::numeric_limits<float>::infinity()) == -1);
    assert(hysteresis(std::numeric_limits<float>::quiet_NaN()) == -1);
    auto values = pending(threshold(400), hysteresis(95), -1, -1);
    assert(values.size() == 2);
    assert(values[0].first == 0x22 && values[0].second == 400);
    assert(values[1].first == 0x23 && values[1].second == 95);
    assert(pending(400, 95, 400, 95).empty());
    assert(pending(-1, -1, 400, 95).empty());
    auto onlyGuard = pending(-1, 90, 400, 95);
    assert(onlyGuard.size() == 1 && onlyGuard[0].first == 0x23 && onlyGuard[0].second == 90);
    auto onlyRange = pending(300, -1, 400, 95);
    assert(onlyRange.size() == 1 && onlyRange[0].first == 0x22 && onlyRange[0].second == 300);
    assert(pending(400, 95, -1, -1).size() == 2); // failure/reconnect retries
    std::cout << "PASS: Triton IDs, independent guard, limits, unset, unchanged, reconnect\n";
}
