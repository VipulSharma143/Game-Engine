#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>

#include "TestFramework.hpp"
#include "engine/platform/FrameDump.hpp"
#include "engine/platform/HeadlessPlatform.hpp"
#include "engine/platform/ScriptedInput.hpp"

TEST(scripted_input_plays_segments_then_stops) {
    const ge::ScriptedInput s({{2, 1.0, 0.0, 0.0}, {3, 0.0, 0.0, -0.5}});
    CHECK_EQ(s.total_frames(), 5u);
    CHECK_NEAR(s.sample(0).forward, 1.0, 1e-12);
    CHECK_NEAR(s.sample(1).forward, 1.0, 1e-12);
    CHECK_NEAR(s.sample(2).forward, 0.0, 1e-12);
    CHECK_NEAR(s.sample(2).turn, -0.5, 1e-12);
    CHECK_NEAR(s.sample(4).turn, -0.5, 1e-12);
    CHECK_NEAR(s.sample(5).turn, 0.0, 1e-12);
    CHECK_NEAR(s.sample(1000).forward, 0.0, 1e-12);
    CHECK_EQ(ge::ScriptedInput().total_frames(), 0u);
}

TEST(headless_platform_counts_frames_and_polls_script) {
    ge::HeadlessPlatform p(ge::ScriptedInput({{1, 1.0, 0.0, 0.0}}));
    CHECK(p.init(ge::EngineConfig{}).ok);
    ge::InputState in;
    p.poll_input(in);
    CHECK_NEAR(in.forward, 1.0, 1e-12);
    p.poll_input(in);
    CHECK_NEAR(in.forward, 0.0, 1e-12);
    ge::Framebuffer fb(8, 8);
    fb.clear(3);
    p.present(fb, 0);
    p.present(fb, 1);
    CHECK_EQ(p.frames_presented(), 2u);
    CHECK_EQ(p.last_checksum(), fb.checksum());
}

TEST(ppm_writer_produces_valid_file) {
    ge::Framebuffer fb(4, 2);
    fb.clear(ge::pack_rgb(10, 20, 30));
    const std::string path = (std::filesystem::temp_directory_path() / "ge_test_frame.ppm").string();
    CHECK(ge::write_ppm(fb, path).ok);
    std::ifstream f(path, std::ios::binary);
    std::string magic;
    int w = 0, h = 0, maxv = 0;
    f >> magic >> w >> h >> maxv;
    CHECK_EQ(magic, std::string("P6"));
    CHECK_EQ(w, 4);
    CHECK_EQ(h, 2);
    CHECK_EQ(maxv, 255);
    f.get();
    char rgb[3] = {};
    f.read(rgb, 3);
    CHECK_EQ(static_cast<unsigned char>(rgb[0]), 10);
    CHECK_EQ(static_cast<unsigned char>(rgb[2]), 30);
    f.close();
    std::remove(path.c_str());
}

TEST(ppm_writer_reports_unwritable_path) {
    ge::Framebuffer fb(2, 2);
    CHECK(!ge::write_ppm(fb, "/nonexistent-dir-ge/x.ppm").ok);
}

TEST(ascii_writer_maps_brightness) {
    ge::Framebuffer fb(16, 8);
    fb.clear(ge::pack_rgb(0, 0, 0));
    for (int y = 0; y < 8; ++y)
        for (int x = 8; x < 16; ++x) fb.set_pixel(x, y, ge::pack_rgb(255, 255, 255));
    std::ostringstream out;
    ge::write_ascii(fb, out, 8);
    const std::string text = out.str();
    CHECK(!text.empty());
    CHECK_EQ(text.front(), ' ');
    CHECK(text.find('@') != std::string::npos);
    std::ostringstream none;
    ge::write_ascii(ge::Framebuffer(), none, 8);
    CHECK(none.str().empty());
}

TEST(input_edges_clear_but_held_input_stays_and_carried_edges_add_up) {
    ge::InputState in;
    in.forward = 1.0; in.fire = true;                       // held: not an edge
    in.use = in.edit_select = in.edit_save = true;
    in.edit_palette = 2; in.edit_rotate = -1; in.look = 4.0;
    in.weapon_slot = 3; in.weapon_cycle = 2;
    ge::InputState carried;
    carried.add_edges(in);
    carried.add_edges(in);                                   // two frames without a step
    CHECK(carried.use && carried.edit_select && carried.edit_save && !carried.edit_insert);
    CHECK_EQ(carried.edit_palette, 4);
    CHECK_EQ(carried.edit_rotate, -2);
    CHECK_EQ(carried.weapon_slot, 3);
    CHECK_EQ(carried.weapon_cycle, 4);
    ge::InputState later; later.weapon_slot = 1;
    carried.add_edges(later);
    CHECK_EQ(carried.weapon_slot, 1);                        // the latest choice wins
    CHECK_NEAR(carried.look, 8.0, 0.0);
    CHECK_NEAR(carried.forward, 0.0, 0.0);                   // levels are not carried over
    in.clear_edges();
    CHECK(!in.use && !in.edit_select && !in.edit_save && in.edit_palette == 0 && in.edit_rotate == 0);
    CHECK(in.weapon_slot == 0 && in.weapon_cycle == 0);
    CHECK_NEAR(in.look, 0.0, 0.0);
    CHECK_NEAR(in.forward, 1.0, 0.0);
    CHECK(in.fire);
}
