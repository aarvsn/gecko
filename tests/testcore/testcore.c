/*
 * GeckoTest — a synthetic libretro core used to validate Gecko's frontend
 * pipeline end to end. It is a real, loadable libretro core that:
 *
 *   - renders a 320x240 animated test pattern (bouncing ball, moving
 *     gradient, color bars mode) in XRGB8888
 *   - reacts to joypad input (D-pad moves the ball, B/A change colors)
 *   - produces a 48 kHz stereo sine wave
 *   - supports save states and core options (pattern, sound)
 *
 * Build: tests/testcore/CMakeLists.txt -> gecko_testcore(.so/.dll/.dylib)
 */
#include "libretro.h"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cstdlib>

static retro_environment_t env_cb;
static retro_video_refresh_t video_cb;
static retro_audio_sample_batch_t audio_batch_cb;
static retro_input_poll_t input_poll_cb;
static retro_input_state_t input_state_cb;

static const unsigned W = 320;
static const unsigned H = 240;
static uint32_t frame[W * H];

struct State {
    double ballX = 160.0, ballY = 120.0;
    double velX = 2.0, velY = 1.6;
    unsigned frames = 0;
    uint32_t hue = 0;
    bool audio = false;
};

static State st;
static double audioPhase = 0.0;

static const char* kPatternChoices[] = {"bouncing-ball", "color-bars", "grid", NULL};
static int patternIndex = 0;

static bool environment(unsigned cmd, void* data) {
    switch (cmd) {
        case RETRO_ENVIRONMENT_GET_OVERSCAN:
            *static_cast<bool*>(data) = false;
            return true;
        case RETRO_ENVIRONMENT_GET_CAN_DUPE:
            *static_cast<bool*>(data) = true;
            return true;
        case RETRO_ENVIRONMENT_SET_PIXEL_FORMAT:
            return true; // we always emit XRGB8888
        case RETRO_ENVIRONMENT_GET_VARIABLE: {
            auto* v = static_cast<struct retro_variable*>(data);
            if (!v || !v->key) return false;
            if (std::strcmp(v->key, "test_pattern") == 0) {
                v->value = kPatternChoices[patternIndex];
                return true;
            }
            if (std::strcmp(v->key, "test_sound") == 0) {
                v->value = st.audio ? "on" : "off";
                return true;
            }
            return false;
        }
        case RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE:
            *static_cast<bool*>(data) = false;
            return true;
        case RETRO_ENVIRONMENT_GET_LOG_INTERFACE:
            return true;
        case RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY:
            *static_cast<const char**>(data) = ".";
            return true;
        case RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY:
            *static_cast<const char**>(data) = ".";
            return true;
        default:
            return false;
    }
}

void retro_set_environment(retro_environment_t cb) {
    env_cb = cb;
    bool no_game = false;
    cb(RETRO_ENVIRONMENT_SET_SUPPORT_NO_GAME, &no_game);
    // Publish core options the way real cores do (SET_VARIABLES is legacy but
    // still the most widely supported path).
    static struct retro_variable vars[] = {
        {"test_pattern", "Test pattern; bouncing-ball|color-bars|grid"},
        {"test_sound", "Audio tone; off|on"},
        {NULL, NULL},
    };
    cb(RETRO_ENVIRONMENT_SET_VARIABLES, vars);
}

void retro_set_video_refresh(retro_video_refresh_t cb) { video_cb = cb; }
void retro_set_audio_sample(retro_audio_sample_t cb) { (void)cb; }
void retro_set_audio_sample_batch(retro_audio_sample_batch_t cb) { audio_batch_cb = cb; }
void retro_set_input_poll(retro_input_poll_t cb) { input_poll_cb = cb; }
void retro_set_input_state(retro_input_state_t cb) { input_state_cb = cb; }

void retro_init(void) {
    st.ballX = 160; st.ballY = 120;
    st.velX = 2.0; st.velY = 1.6;
    st.frames = 0;
    st.hue = 0;
    st.audio = false;
}

void retro_deinit(void) {}

unsigned retro_api_version(void) { return RETRO_API_VERSION; }

void retro_get_system_info(struct retro_system_info* info) {
    std::memset(info, 0, sizeof(*info));
    info->library_name = "GeckoTest";
    info->library_version = "1.0.0";
    info->need_fullpath = false;
    info->valid_extensions = "test|geckotest";
    info->block_extract = false;
}

void retro_get_system_av_info(struct retro_system_av_info* info) {
    std::memset(info, 0, sizeof(*info));
    info->timing.fps = 60.0;
    info->timing.sample_rate = 48000.0;
    info->geometry.base_width = W;
    info->geometry.base_height = H;
    info->geometry.max_width = W;
    info->geometry.max_height = H;
    info->geometry.aspect_ratio = 4.0f / 3.0f;
}

void retro_set_controller_port_device(unsigned port, unsigned device) { (void)port; (void)device; }

static void render_bouncing_ball() {
    // Moving gradient background (proves frame animation).
    const uint8_t shift = static_cast<uint8_t>(st.frames & 0xFF);
    for (unsigned y = 0; y < H; ++y) {
        for (unsigned x = 0; x < W; ++x) {
            const uint8_t r = static_cast<uint8_t>((x + shift) & 0x3F);
            const uint8_t g = static_cast<uint8_t>((y * 2 + shift) & 0x7F);
            const uint8_t b = static_cast<uint8_t>(160 + ((x / 16) & 0x3F));
            frame[y * W + x] = 0xFF000000u | (static_cast<uint32_t>(r) << 16) |
                               (static_cast<uint32_t>(g) << 8) | static_cast<uint32_t>(b);
        }
    }
    // Ball (reacts to D-pad input, proves the input pipeline).
    const uint32_t ballColor = 0xFF000000u | (st.hue << 16) | (0xFF - st.hue);
    const int radius = 18;
    for (int dy = -radius; dy <= radius; ++dy) {
        for (int dx = -radius; dx <= radius; ++dx) {
            const int px = static_cast<int>(st.ballX) + dx;
            const int py = static_cast<int>(st.ballY) + dy;
            if (px < 0 || py < 0 || px >= static_cast<int>(W) || py >= static_cast<int>(H)) continue;
            if (dx * dx + dy * dy <= radius * radius) frame[py * W + px] = ballColor;
        }
    }
}

static void render_color_bars() {
    // SMPTE-style bars with an animated white sweep line.
    static const uint32_t colors[7] = {
        0xFFC0C0C0, 0xFFC0C000, 0xFF00C0C0, 0xFF00C000,
        0xFFC000C0, 0xFFC00000, 0xFF0000C0,
    };
    const unsigned barWidth = W / 7;
    for (unsigned x = 0; x < W; ++x) {
        const uint32_t c = colors[(x / barWidth) % 7];
        for (unsigned y = 0; y < H; ++y) frame[y * W + x] = c;
    }
    const unsigned markerY = (st.frames * 2) % H;
    for (unsigned x = 0; x < W; ++x) frame[markerY * W + x] = 0xFFFFFFFFu;
}

static void render_grid() {
    for (unsigned y = 0; y < H; ++y) {
        for (unsigned x = 0; x < W; ++x) {
            const bool line = (x % 32 == 0) || (y % 32 == 0);
            const bool center = x >= 156 && x < 164 && y >= 116 && y < 124;
            frame[y * W + x] = center ? 0xFFFF4040u : (line ? 0xFF303030u : 0xFFE8E8E8u);
        }
    }
}

void retro_run(void) {
    input_poll_cb();

    // Re-read core options when the frontend flags them as changed (the way
    // real cores consume GET_VARIABLE_UPDATE).
    bool updated = false;
    if (env_cb(RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE, &updated) && updated) {
        struct retro_variable v;
        v.key = "test_sound";
        v.value = NULL;
        if (env_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &v) && v.value)
            st.audio = (std::strcmp(v.value, "on") == 0);
        v.key = "test_pattern";
        v.value = NULL;
        if (env_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &v) && v.value) {
            for (int i = 0; kPatternChoices[i]; ++i)
                if (std::strcmp(v.value, kPatternChoices[i]) == 0) patternIndex = i;
        }
    }

    // Input → simulation (validates the frontend's input plumbing end to end).
    const bool up = input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP);
    const bool down = input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN);
    const bool left = input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT);
    const bool right = input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT);
    const bool a = input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_A);
    const bool b = input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_B);

    if (up) st.velY -= 0.08;
    if (down) st.velY += 0.08;
    if (left) st.velX -= 0.08;
    if (right) st.velX += 0.08;
    if (a) st.hue = (st.hue + 7) & 0xFF;
    if (b) st.hue = (st.hue - 7) & 0xFF;

    st.ballX += st.velX;
    st.ballY += st.velY;
    if (st.ballX < 20) { st.ballX = 20; st.velX = -st.velX; }
    if (st.ballX > W - 20) { st.ballX = W - 20; st.velX = -st.velX; }
    if (st.ballY < 20) { st.ballY = 20; st.velY = -st.velY; }
    if (st.ballY > H - 20) { st.ballY = H - 20; st.velY = -st.velY; }

    switch (patternIndex) {
        case 1: render_color_bars(); break;
        case 2: render_grid(); break;
        default: render_bouncing_ball(); break;
    }

    // Audio: 440 Hz sine on demand (proves the audio pipeline).
    if (st.audio) {
        int16_t samples[480]; // 240 stereo frames per call
        for (int i = 0; i < 240; ++i) {
            const double s = std::sin(audioPhase) * 8192.0;
            samples[2 * i] = static_cast<int16_t>(s);
            samples[2 * i + 1] = static_cast<int16_t>(s);
            audioPhase += 2.0 * 3.14159265358979 * 440.0 / 48000.0;
        }
        audio_batch_cb(samples, 240);
    }

    video_cb(frame, W, H, W * sizeof(uint32_t));
    st.frames++;
}

void retro_reset(void) {
    st.ballX = 160; st.ballY = 120;
    st.velX = 2.0; st.velY = 1.6;
    st.frames = 0;
}

bool retro_load_game(const struct retro_game_info* info) {
    (void)info; // the pattern runs without content too
    enum retro_pixel_format fmt = RETRO_PIXEL_FORMAT_XRGB8888;
    env_cb(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &fmt);
    return true;
}

void retro_unload_game(void) {}
unsigned retro_get_region(void) { return RETRO_REGION_NTSC; }
bool retro_load_game_special(unsigned type, const struct retro_game_info* info, size_t num) {
    (void)type; (void)info; (void)num;
    return false;
}

size_t retro_serialize_size(void) { return sizeof(State); }

bool retro_serialize(void* data, size_t size) {
    if (size < sizeof(State)) return false;
    std::memcpy(data, &st, sizeof(State));
    return true;
}

bool retro_unserialize(const void* data, size_t size) {
    if (size < sizeof(State)) return false;
    std::memcpy(&st, data, sizeof(State));
    return true;
}

void* retro_get_memory_data(unsigned id) { (void)id; return NULL; }
size_t retro_get_memory_size(unsigned id) { (void)id; return 0; }
void retro_cheat_reset(void) {}
void retro_cheat_set(unsigned index, bool enabled, const char* code) {
    (void)index; (void)enabled; (void)code;
}
