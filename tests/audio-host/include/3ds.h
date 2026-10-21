#pragma once
#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <map>

using Result = int32_t;
#define R_FAILED(result) ((result) < 0)
enum { NDSP_OUTPUT_STEREO, NDSP_INTERP_LINEAR, NDSP_FORMAT_MONO_PCM16 };
enum { NDSP_WBUF_FREE, NDSP_WBUF_QUEUED, NDSP_WBUF_PLAYING, NDSP_WBUF_DONE };
struct ndspWaveBuf {
    int16_t* data_pcm16 = nullptr;
    unsigned nsamples = 0;
    unsigned status = NDSP_WBUF_FREE;
};
namespace audioMock {
inline Result initResult = 0;
inline bool initialized = false;
inline int initCalls = 0, exitCalls = 0, allocCalls = 0, failAllocation = 0;
inline int freeCalls = 0, flushCalls = 0, addCalls = 0, clearCalls = 0;
inline uint64_t now = 1000;
inline std::array<ndspWaveBuf*, 4> queued{};
inline std::map<void*, size_t> allocations;
inline void reset() {
    assert(allocations.empty() && !initialized);
    initResult = 0; initCalls = exitCalls = allocCalls = failAllocation = 0;
    freeCalls = flushCalls = addCalls = clearCalls = 0;
    now = 1000; queued.fill(nullptr);
}
inline void finish(int channel) {
    assert(queued[channel]);
    queued[channel]->status = NDSP_WBUF_DONE;
    queued[channel] = nullptr;
}
}
inline Result ndspInit() {
    ++audioMock::initCalls;
    audioMock::initialized = !R_FAILED(audioMock::initResult);
    return audioMock::initResult;
}
inline void ndspExit() {
    assert(audioMock::initialized);
    audioMock::initialized = false;
    ++audioMock::exitCalls;
    audioMock::queued.fill(nullptr);
}
inline void ndspSetOutputMode(int) {}
inline void ndspSetMasterVol(float) {}
inline void ndspChnSetInterp(int, int) {}
inline void ndspChnSetRate(int, float) {}
inline void ndspChnSetFormat(int, int) {}
inline void ndspChnSetMix(int, float*) {}
inline void* linearAlloc(size_t bytes) {
    ++audioMock::allocCalls;
    if (audioMock::failAllocation == audioMock::allocCalls) return nullptr;
    void* p = std::malloc(bytes);
    if (p) audioMock::allocations.emplace(p, bytes);
    return p;
}
inline void linearFree(void* p) {
    // Sample memory must outlive the DSP worker and queued hardware access.
    assert(!audioMock::initialized);
    assert(audioMock::allocations.erase(p) == 1);
    ++audioMock::freeCalls;
    std::free(p);
}
inline Result DSP_FlushDataCache(void* p, size_t bytes) {
    assert(audioMock::allocations.at(p) == bytes);
    ++audioMock::flushCalls;
    return 0;
}
inline void ndspChnReset(int channel) { audioMock::queued[channel] = nullptr; }
inline bool ndspChnIsPlaying(int channel) {
    // Like libctru, QUEUED does not imply that the DSP has begun playback.
    return audioMock::queued[channel] && audioMock::queued[channel]->status == NDSP_WBUF_PLAYING;
}
inline void ndspChnWaveBufClear(int channel) {
    ++audioMock::clearCalls;
    if (audioMock::queued[channel]) audioMock::queued[channel]->status = NDSP_WBUF_DONE;
    audioMock::queued[channel] = nullptr;
}
inline void ndspChnWaveBufAdd(int channel, ndspWaveBuf* wave) {
    assert(audioMock::initialized && !audioMock::queued[channel]);
    assert(wave->status == NDSP_WBUF_FREE || wave->status == NDSP_WBUF_DONE);
    assert(wave->nsamples > 0 && audioMock::allocations.count(wave->data_pcm16));
    wave->status = NDSP_WBUF_QUEUED;
    audioMock::queued[channel] = wave;
    ++audioMock::addCalls;
}
inline uint64_t osGetTime() { return audioMock::now; }
