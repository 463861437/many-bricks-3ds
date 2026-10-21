#include "audio.hpp"
#include "game.hpp"
#include <3ds.h>
#include <iostream>

static int checks = 0;
#define CHECK(condition) do { if (!(condition)) { \
    std::cerr << "Audio check failed at " << __LINE__ << ": " << #condition << '\n'; \
    std::abort(); } ++checks; } while (0)

int main() {
    using namespace bricks;
    audioMock::reset();
    CHECK(audioInit());
    CHECK(audioMock::allocations.size() == 6 && audioMock::flushCalls == 6);
    audioPlay(S_POWER, true);
    CHECK(audioMock::addCalls == 1 && audioMock::queued[0]);
    auto* first = audioMock::queued[0];
    auto* firstSample = first->data_pcm16;
    CHECK(!ndspChnIsPlaying(0) && first->status == NDSP_WBUF_QUEUED);
    audioPlay(S_LAUNCH, true);
    CHECK(audioMock::queued[0] == first && first->data_pcm16 == firstSample);
    CHECK(audioMock::queued[1] && audioMock::addCalls == 2);
    audioPlay(S_LOST, true);
    audioPlay(S_WIN, true);
    CHECK(audioMock::queued[2] && audioMock::queued[3] && audioMock::addCalls == 4);
    const int clearsBeforeFull = audioMock::clearCalls;
    audioPlay(S_POWER, true);
    CHECK(audioMock::addCalls == 4 && audioMock::clearCalls == clearsBeforeFull);
    audioMock::queued[0]->status = NDSP_WBUF_PLAYING;
    audioMock::finish(1);
    audioPlay(S_WIN, true);
    CHECK(audioMock::addCalls == 5 && audioMock::queued[1]);
    CHECK(audioMock::queued[0] == first && first->status == NDSP_WBUF_PLAYING);
    audioPlay(S_LOST, false); audioPlay(0, true);
    CHECK(audioMock::addCalls == 5);
    audioStop();
    for (auto* wave : audioMock::queued) CHECK(wave == nullptr);
    CHECK(audioMock::allocations.size() == 6);
    audioMock::now = 2000;
    audioPlay(S_BRICK, true); const int firstHitCount = audioMock::addCalls;
    audioMock::now = 2020; audioPlay(S_BRICK, true);
    CHECK(audioMock::addCalls == firstHitCount);
    audioMock::now = 2050; audioPlay(S_BRICK, true);
    CHECK(audioMock::addCalls == firstHitCount + 1);
    audioExit();
    CHECK(audioMock::exitCalls == 1 && audioMock::freeCalls == 6 && audioMock::allocations.empty());
    audioExit(); audioStop(); audioPlay(S_POWER, true);
    CHECK(audioMock::exitCalls == 1 && audioMock::freeCalls == 6);

    audioMock::reset(); audioMock::initResult = -1;
    CHECK(!audioInit()); audioPlay(S_POWER, true); audioExit();
    CHECK(audioMock::initCalls == 1 && audioMock::exitCalls == 0 && audioMock::allocCalls == 0);

    for (int failure = 1; failure <= 6; ++failure) {
        audioMock::reset(); audioMock::failAllocation = failure;
        CHECK(!audioInit());
        CHECK(audioMock::allocCalls == failure && audioMock::freeCalls == failure - 1);
        CHECK(audioMock::exitCalls == 1 && audioMock::allocations.empty());
        audioPlay(S_POWER, true); audioExit();
        CHECK(audioMock::addCalls == 0 && audioMock::exitCalls == 1);
    }
    audioMock::reset();
    CHECK(audioInit()); audioPlay(S_POWER, true);
    CHECK(audioMock::addCalls == 1);
    audioExit();
    CHECK(audioMock::allocations.empty());
    std::cout << checks << " audio lifecycle and queue checks passed\n";
}
