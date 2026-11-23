#include "common/ringBuffer.h"

#include <cstdio>
#include <cstdlib>

namespace {

  enum class Audio { None, DialTone, BusyTone, Greeting };

  void expect(bool condition, const char *message) {
    if (!condition) {
      std::fprintf(stderr, "%s\n", message);
      std::exit(EXIT_FAILURE);
    }
  }

  void preservesPlaybackOrderWhenFull() {
    RingBuffer<Audio, 2> pending;
    expect(pending.push(Audio::DialTone), "Could not queue the dial tone.");
    expect(pending.push(Audio::BusyTone), "Could not queue the busy tone.");
    expect(!pending.push(Audio::Greeting), "A full audio queue accepted another sound.");
    expect(pending.pop() == Audio::DialTone, "The full queue lost the first sound.");
    expect(pending.push(Audio::Greeting), "Playback did not make room for another sound.");
    expect(pending.pop() == Audio::BusyTone, "The new sound overtook the pending busy tone.");
    expect(pending.pop() == Audio::Greeting, "The new sound was lost after reusing a slot.");
  }

  void discardsPendingSoundsWhenCleared() {
    RingBuffer<Audio, 3> pending;
    expect(pending.push(Audio::DialTone), "Could not queue the dial tone.");
    expect(pending.push(Audio::BusyTone), "Could not queue the busy tone.");
    pending.clear();

    expect(pending.empty(), "Clearing the audio queue left sounds pending.");
    expect(pending.push(Audio::Greeting), "The cleared queue refused a new sound.");
    expect(pending.front() == Audio::Greeting, "Playback would resume with stale audio.");
  }

} // namespace

int main() {
  preservesPlaybackOrderWhenFull();
  discardsPendingSoundsWhenCleared();
  std::puts("Passed 2 modem audio queue tests.");
}
