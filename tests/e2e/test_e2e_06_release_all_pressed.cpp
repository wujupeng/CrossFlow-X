#include <cstdio>
#include <cstdlib>
#include <thread>

#include "integration/disconnect_resync_coordinator.hpp"
#include "integration/e2e_evidence_collector.hpp"
#include "s05_transport/transport_impl.hpp"
#include "common/domain.hpp"
#include "common/platform_ports.hpp"
#include "loopback_transport.hpp"

#define CHECK(cond) do { if (!(cond)) { fprintf(stderr, "CHECK FAILED: %s:%d: %s\n", __FILE__, __LINE__, #cond); std::exit(1); } } while(0)

namespace cfx {


class StubClock : public IMonotonicClock {
public:
    u64 nowUs() override { return counter_.fetch_add(1, std::memory_order_relaxed); }
private:
    std::atomic<u64> counter_{1000};
};

static void test_e2e_06_release_all_pressed() {
    printf("[TEST] E2E-06: release all pressed\n");

    LoopbackTransportPair pair(11251, 11252);

    StubClock clock;

    E2EEvidenceCollector evidence;

    PressedStateSnapshot snapshot{};
    snapshot.modifiers = {true, true, false, false, false};
    snapshot.pressedMouseButtons.setPressed(MouseButton::Left);
    snapshot.pressedKeys.setPressed(16);
    snapshot.pressedKeys.setPressed(17);

    CHECK(snapshot.pressedMouseButtons.anyPressed());
    CHECK(snapshot.pressedKeys.anyPressed());

    u32 releasedCount = 0;
    DisconnectResyncConfig config{};

    DisconnectResyncCoordinator coordinator(
        pair.client,
        config,
        [&](const PressedStateSnapshot& snap) -> ReleaseResult {
            u32 count = 0;
            if (snap.pressedMouseButtons.anyPressed()) count += snap.pressedMouseButtons.count();
            if (snap.pressedKeys.anyPressed()) count += snap.pressedKeys.count();
            if (snap.modifiers.shift) count++;
            if (snap.modifiers.ctrl) count++;
            releasedCount = count;
            return {count, 50};
        },
        [&]() -> PressedStateSnapshot { return snapshot; },
        [&]() -> void {},
        [&]() -> bool { return true; }
    );

    coordinator.start();

    evidence.recordDisconnectEvidence(clock.nowUs(), "release_all_pressed_test");

    pair.client.disconnect();

    CHECK(releasedCount > 0);
    CHECK(releasedCount >= 4);

    evidence.recordReconnectEvidence(clock.nowUs(), "all_pressed_released");

    PressedStateSnapshot clearedSnapshot{};
    CHECK(!clearedSnapshot.pressedMouseButtons.anyPressed());
    CHECK(!clearedSnapshot.pressedKeys.anyPressed());

    auto records = evidence.allRecords();
    CHECK(records.size() == 2);

    coordinator.stop();
}

}  // namespace cfx

int main() {
    printf("=== test_e2e_06_release_all_pressed ===\n");
    cfx::test_e2e_06_release_all_pressed();
    printf("=== ALL PASS ===\n");
    return 0;
}
