// SPDX-License-Identifier: MIT
// Host-side test harness for gecko-core. Pure C++ (no Qt): loads the synthetic
// GeckoTest core through the real Emulator pipeline and verifies video, audio,
// input, save states, serialization and the JSON config layer.
//
// Build via CMake (tests/CMakeLists.txt -> gecko-host-tests).

#include "gecko/audio_sink.hpp"
#include "gecko/config.hpp"
#include "gecko/emulator.hpp"
#include "gecko/input_source.hpp"
#include "gecko/json.hpp"
#include "gecko/library.hpp"
#include "gecko/video_sink.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

using namespace gecko;
using namespace gecko::json;

namespace {

int g_failures = 0;
int g_checks = 0;

void check(bool condition, const char* fmt, ...) {
    ++g_checks;
    if (condition) {
        std::printf("  ok  ");
        va_list args;
        va_start(args, fmt);
        std::vprintf(fmt, args);
        va_end(args);
        std::printf("\n");
    } else {
        ++g_failures;
        std::printf("FAIL  ");
        va_list args;
        va_start(args, fmt);
        std::vprintf(fmt, args);
        va_end(args);
        std::printf("\n");
    }
}

// --- video sink collecting every frame ------------------------------------
class CollectingVideoSink : public VideoSink {
public:
    void frame(const VideoFrame& f) override {
        last_ = f;
        frames_.push_back(
            std::vector<uint32_t>(f.pixels, f.pixels + static_cast<size_t>(f.width) * f.height));
    }

    const VideoFrame& last() const { return last_; }
    size_t count() const { return frames_.size(); }
    const std::vector<uint32_t>& framePixels(size_t index) const { return frames_[index]; }

    uint64_t colorSum(size_t frameIndex) const {
        uint64_t sum = 0;
        for (uint32_t p : frames_[frameIndex])
            sum += (p & 0xFF) + ((p >> 8) & 0xFF) + ((p >> 16) & 0xFF);
        return sum;
    }

    uint32_t distinctColors(size_t frameIndex) const {
        std::vector<uint32_t> copy = frames_[frameIndex];
        std::sort(copy.begin(), copy.end());
        return static_cast<uint32_t>(std::unique(copy.begin(), copy.end()) - copy.begin());
    }

private:
    VideoFrame last_{};
    std::vector<std::vector<uint32_t>> frames_;
};

// --- audio sink counting samples ------------------------------------------
class CountingAudioSink : public AudioSink {
public:
    void setFormat(double rate) override { rate_ = rate; }
    size_t write(const int16_t* samples, size_t frameCount) override {
        samplesWritten_ += frameCount;
        if (frameCount >= 2)
            lastPeak_ = std::max(lastPeak_, std::abs(static_cast<int>(samples[0])));
        return frameCount;
    }
    size_t samplesWritten() const { return samplesWritten_; }
    double rate() const { return rate_; }

private:
    size_t samplesWritten_ = 0;
    double rate_ = 0.0;
    int lastPeak_ = 0;
};

// --- helpers ---------------------------------------------------------------
std::filesystem::path makeTempDir(const char* tag) {
    const auto base = std::filesystem::temp_directory_path();
    const auto dir =
        base / ("gecko-tests-" + std::string(tag) + "-" +
                std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) +
                "-" + std::to_string(std::rand()));
    std::filesystem::create_directories(dir);
    return dir;
}

// Finds the rightmost high-contrast column (the ball edge) in a frame.
uint32_t findBallColumn(const CollectingVideoSink* video, size_t frameIndex) {
    const unsigned w = video->last().width;
    const unsigned h = video->last().height;
    const auto& pixels = video->framePixels(frameIndex);
    uint32_t bestX = 0;
    int bestDelta = 0;
    for (unsigned y = 60; y + 1 < h; y += 7) {
        for (unsigned x = 1; x < w; ++x) {
            const uint32_t p0 = pixels[static_cast<size_t>(y) * w + x - 1];
            const uint32_t p1 = pixels[static_cast<size_t>(y) * w + x];
            const int delta = std::abs(static_cast<int>((p1 & 0xFF) - (p0 & 0xFF))) +
                              std::abs(static_cast<int>(((p1 >> 8) & 0xFF) - ((p0 >> 8) & 0xFF)));
            if (delta > bestDelta) {
                bestDelta = delta;
                bestX = x;
            }
        }
    }
    return bestX;
}

// --- JSON unit checks ---------------------------------------------------------
void testJson() {
    std::printf("[json]\n");
    std::string error;
    const Value v = parse(
        R"({"name":"gecko","count":3,"ok":true,"list":[1,2,3],"nested":{"a":"b"},)"
        R"("esc":"tab\tnewline\nquote\"unicode\u00e9"})",
        &error);
    check(error.empty() && v.isObject(), "parse basic object");
    check(v.find("name")->asString() == "gecko", "string member");
    check(v.find("count")->asInteger() == 3, "integer member");
    check(v.find("ok")->asBool() == true, "boolean member");
    check(v.find("list")->items().size() == 3, "array member");
    check(v.find("nested")->find("a")->asString() == "b", "nested object");
    check(v.find("esc")->asString() == "tab\tnewline\nquote\"unicode\xc3\xa9",
          "escapes + unicode");

    const std::string serialized = serialize(v);
    Value reparsed = parse(serialized, &error);
    check(error.empty() && reparsed.find("count")->asInteger() == 3, "round-trip");

    Value bad = parse("{\"a\":}", &error);
    check(bad.isNull() && !error.empty(), "truncated object rejected");

    Value trailing = parse("123 456", &error);
    check(trailing.isNull() && !error.empty(), "trailing garbage rejected");

    Value depth = parse(std::string(80, '['), &error);
    check(depth.isNull() && !error.empty(), "depth limit enforced");

    // Config round-trip on disk.
    const auto dir = makeTempDir("config");
    Config config(dir / "gecko.json");
    config.set("video.filter", "scanlines");
    config.set("video.scale", (int64_t)3);
    config.set("audio.volume", (int64_t)80);
    config.setStringList("library.directories", {"/roms/snes", "/roms/gba"});
    check(config.save(), "config save");

    Config reloaded(dir / "gecko.json");
    std::string loadError;
    check(reloaded.load(&loadError), "config load: %s", loadError.c_str());
    check(reloaded.getString("video.filter") == "scanlines", "config string round-trip");
    check(reloaded.getInt("video.scale") == 3, "config int round-trip");
    check(reloaded.getStringList("library.directories").size() == 2, "config list round-trip");
    check(reloaded.getString("missing.key", "fallback") == "fallback", "config default");
}

// --- library scanner ------------------------------------------------------------
void testLibrary() {
    std::printf("[library]\n");
    const auto dir = makeTempDir("library");
    std::filesystem::create_directories(dir / "roms/subdir");
    const char* files[] = {
        "Super_World_(USA).sfc", "game2.nes", "ignored.txt",
        "subdir/deep_game.gb",   "art_test.gba",
    };
    for (const char* f : files) {
        FILE* fp = std::fopen((dir / "roms" / f).string().c_str(), "wb");
        if (fp) std::fputc(0, fp);
        if (fp) std::fclose(fp);
    }
    FILE* art = std::fopen((dir / "roms" / "art_test.png").string().c_str(), "wb");
    if (art) std::fputc(0, art);
    if (art) std::fclose(art);

    Library lib;
    lib.scan({(dir / "roms").string()}, {{"sfc", "/cores/snes.so"}}, defaultRomExtensions());
    check(lib.count() == 4, "scanner finds 4 roms (got %zu)", lib.count());

    bool foundSfc = false, foundArt = false, foundCore = false, titleOk = false;
    for (const auto& g : lib.games()) {
        if (g.extension == "sfc") {
            foundSfc = true;
            foundCore = (g.corePath == "/cores/snes.so");
            titleOk = (g.title == "Super World");
        }
        if (g.extension == "gba" && !g.artworkPath.empty()) foundArt = true;
    }
    check(foundSfc, "sfc entry present");
    check(foundCore, "extension->core mapping applied");
    check(titleOk, "title prettified");
    check(foundArt, "artwork discovered next to ROM");
}

// --- emulator pipeline ---------------------------------------------------------
void testEmulatorPipeline(const std::filesystem::path& corePath) {
    std::printf("[emulator pipeline: %s]\n", corePath.filename().string().c_str());

    Emulator emu;
    auto video = std::make_unique<CollectingVideoSink>();
    auto audio = std::make_unique<CountingAudioSink>();
    auto input = std::make_unique<JoypadStateSource>();

    emu.setVideoSink(video.get());
    emu.setAudioSink(audio.get());
    emu.setInputSource(input.get());
    emu.setSystemDirectory(corePath.parent_path());
    emu.setSaveDirectory(corePath.parent_path());

    std::string error;
    check(emu.loadCore(corePath, &error), "core loads (%s)", error.c_str());
    check(emu.coreLoaded(), "coreLoaded()");
    check(emu.coreDetails().libraryName == "GeckoTest",
          "system info reported (%s)", emu.coreDetails().libraryName.c_str());

    const auto dir = makeTempDir("emu");
    const auto rom = dir / "demo.geckotest";
    {
        FILE* f = std::fopen(rom.string().c_str(), "wb");
        if (f) std::fputc('G', f);
        if (f) std::fclose(f);
    }

    check(emu.loadGame(rom, &error), "content loads (%s)", error.c_str());
    check(emu.avInfo().fps > 0 && emu.avInfo().baseWidth == 320, "av info %ux%u @ %.1f fps",
          emu.avInfo().baseWidth, emu.avInfo().baseHeight, emu.avInfo().fps);

    for (int i = 0; i < 90; ++i) emu.runFrame();
    check(video->count() == 90, "video frames received (%zu)", video->count());
    check(video->last().width == 320 && video->last().height == 240, "frame geometry 320x240");
    check(video->distinctColors(0) > 100, "frame 0 rich colors (%u)", video->distinctColors(0));
    check(video->colorSum(0) != video->colorSum(45), "frames animate (0 vs 45)");
    check(video->colorSum(0) != video->colorSum(89), "frames animate (0 vs 89)");

    // Input: from a clean reset, hold RIGHT for 20 frames -> the ball moves
    // right by a deterministic ~56 px (vel 2.0 -> 3.6, avg 2.8/frame).
    emu.reset();
    emu.runFrame();
    const size_t beforeIdx = video->count() - 1;
    const uint32_t before = findBallColumn(video.get(), beforeIdx);
    input->setButton(0, RETRO_DEVICE_ID_JOYPAD_RIGHT, true);
    for (int i = 0; i < 20; ++i) emu.runFrame();
    input->setButton(0, RETRO_DEVICE_ID_JOYPAD_RIGHT, false);
    const uint32_t after = findBallColumn(video.get(), video->count() - 1);
    check(after > before + 30, "input moves ball (%u -> %u)", before, after);
    const size_t savedIdx = video->count() - 1;

    // Bitmask query (host advertised GET_INPUT_BITMASKS).
    input->setButton(0, RETRO_DEVICE_ID_JOYPAD_A, true);
    input->setButton(0, RETRO_DEVICE_ID_JOYPAD_UP, true);
    const uint16_t mask = static_cast<uint16_t>(
        input->state(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_MASK));
    check(mask == ((1u << RETRO_DEVICE_ID_JOYPAD_A) | (1u << RETRO_DEVICE_ID_JOYPAD_UP)),
          "joypad bitmask query (0x%04x)", mask);
    input->clearAll();

    // Audio via core option.
    check(!emu.coreOptions().empty(), "core options populated (%zu)", emu.coreOptions().size());
    bool setSound = false;
    for (const auto& opt : emu.coreOptions())
        if (opt.key == "test_sound") setSound = true;
    check(setSound, "test_sound option present");
    emu.setCoreOptionValue("test_sound", "on");
    const size_t beforeSamples = audio->samplesWritten();
    for (int i = 0; i < 60; ++i) emu.runFrame();
    check(audio->samplesWritten() > beforeSamples, "audio samples flow (+%zu)",
          audio->samplesWritten() - beforeSamples);
    check(audio->rate() > 40000.0 && audio->rate() < 56000.0, "sample rate sane (%.0f)",
          audio->rate());

    // Save state round-trip. The simulation is fully deterministic, so after
    // restoring and re-running the same number of frames the ball must land
    // on exactly the same column as before the save.
    const auto statePath = dir / "slot1.state";
    check(emu.saveStateToFile(statePath), "save state");
    for (int i = 0; i < 120; ++i) emu.runFrame();
    const uint32_t moved = findBallColumn(video.get(), video->count() - 1);
    check(emu.loadStateFromFile(statePath), "load state");
    for (int i = 0; i < 120; ++i) emu.runFrame();
    const uint32_t restored = findBallColumn(video.get(), video->count() - 1);
    check(restored == moved, "state restores deterministic sim (%u vs %u)", moved, restored);
    check(moved != findBallColumn(video.get(), savedIdx),
          "120 frames advanced the sim before restore");

    emu.reset();
    check(emu.framesRun() > 0, "frame counter tracks (%llu)",
          static_cast<unsigned long long>(emu.framesRun()));

    emu.unloadGame();
    emu.unloadCore();
    check(!emu.coreLoaded(), "core unloaded");
}

} // namespace

int main(int argc, char** argv) {
    std::filesystem::path corePath;
    if (argc >= 2) {
        corePath = argv[1];
    } else {
        corePath = std::filesystem::path(argv[0]).parent_path() /
                   ("gecko_testcore" GECKO_CORE_SUFFIX);
    }

    std::printf("gecko host tests — core: %s\n", corePath.string().c_str());
    if (!std::filesystem::exists(corePath)) {
        std::printf("SKIP: test core not found at %s (build it first)\n", corePath.string().c_str());
        return 2;
    }

    testJson();
    testLibrary();
    testEmulatorPipeline(corePath);

    std::printf("\n%d checks, %d failures\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
