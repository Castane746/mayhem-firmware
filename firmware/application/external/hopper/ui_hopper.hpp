/*
 * Copyright (C) 2015 Jared Boone, ShareBrained Technology, Inc.
 * Copyright (C) 2016 Furrtek
 * Copyright (C) 2020 euquiq
 * copyleft mr.r0b0t from the F society
 *
 * This file is part of PortaPack.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street,
 * Boston, MA 02110-1301, USA.
 */

#include "ui.hpp"
#include "ui_language.hpp"
#include "ui_widget.hpp"
#include "ui_navigation.hpp"
#include "ui_tabview.hpp"
#include "transmitter_model.hpp"
#include "message.hpp"
#include "jammer.hpp"
#include "lfsr_random.hpp"
#include "radio_state.hpp"

using namespace jammer;

namespace ui::external_app::hopper {

class HopperView : public View {
   public:
    HopperView(NavigationView& nav);
    ~HopperView();

    HopperView(const HopperView&) = delete;
    HopperView(HopperView&&) = delete;
    HopperView& operator=(const HopperView&) = delete;
    HopperView& operator=(HopperView&&) = delete;

    void focus() override;
    void update_freq_list_menu_view();

    std::string title() const override { return "Hopper"; };

    std::vector<rf::Frequency> freq_list{315000000, 433920000};

   private:
    NavigationView& nav_;
    TxRadioState radio_state_{
        0 /* frequency */,
        3500000 /* bandwidth */,
        3072000 /* sampling rate */
    };

    void start_tx();
    void on_timer();
    void stop_tx();
    void set_hopper_channel(uint32_t i, uint32_t width, uint64_t center, uint32_t duration);
    void on_retune(const rf::Frequency freq, const uint32_t range);
    void load_list();
    void save_list();
    void on_file_save_write(const std::string& value);

    HopperChannel* hopper_channels = (HopperChannel*)shared_memory.bb_data.data;
    bool jamming{false};
    bool cooling{false};     // euquiq: Indicates jammer in cooldown
    uint16_t seconds = 0;    // euquiq: seconds counter for toggling tx / cooldown
    int16_t mscounter = 0;   // euquiq: Internal ms counter for do_timer()
    lfsr_word_t lfsr_v = 1;  // euquiq: Used to generate "random" Jitter

    std::string filename_buffer = "";

    const Style& style_val = *Theme::getInstance()->fg_green;
    const Style& style_cancel = *Theme::getInstance()->fg_red;

    MenuView menu_freq_list{
        {0, 0, screen_width, 8 * 16},
        false};

    NewButton button_load_list{
        {0 * 8, 9 * 16 + 4, 4 * 8, 32},
        {},
        &bitmap_icon_load,
        Color::dark_blue(),
        /*vcenter*/ true};

    NewButton button_save_list{
        {4 * 8, 9 * 16 + 4, 4 * 8, 32},
        {},
        &bitmap_icon_save,
        Color::dark_blue(),
        /*vcenter*/ true};

    NewButton button_add_freq{
        {8 * 8 + 4, 9 * 16 + 4, 4 * 8, 32},
        {},
        &bitmap_icon_add,
        Color::dark_green(),
        /*vcenter*/ true};

    NewButton button_delete_freq{
        {12 * 8 + 4, 9 * 16 + 4, 4 * 8, 32},
        {},
        &bitmap_icon_trash,
        Color::dark_red(),
        /*vcenter*/ true};

    NewButton button_clear{
        {screen_width - 4 * 8, 9 * 16 + 4, 4 * 8, 32},
        {},
        &bitmap_icon_tools_wipesd,
        Color::red(),
        /*vcenter*/ true};

    Labels labels{
        {{2 * 8, 23 * 8}, "Type:", Theme::getInstance()->fg_light->foreground},
        {{1 * 8, 25 * 8}, "Speed:", Theme::getInstance()->fg_light->foreground},
        {{3 * 8, 27 * 8}, "Hop:", Theme::getInstance()->fg_light->foreground},
        {{4 * 8, 29 * 8}, "TX:", Theme::getInstance()->fg_light->foreground},
        {{1 * 8, 31 * 8}, "Sle3p:", Theme::getInstance()->fg_light->foreground},   // euquiq: Token of appreciation to TheSle3p, which made this ehnancement a reality with his bounty.
        {{0 * 8, 33 * 8}, "Jitter:", Theme::getInstance()->fg_light->foreground},  // Maybe the repository curator can keep the "mystype" for some versions.
        {{11 * 8, 29 * 8}, "Secs.", Theme::getInstance()->fg_light->foreground},
        {{11 * 8, 31 * 8}, "Secs.", Theme::getInstance()->fg_light->foreground},
        {{11 * 8, 33 * 8}, "/60", Theme::getInstance()->fg_light->foreground},
        {{2 * 8, 35 * 8}, "Gain:", Theme::getInstance()->fg_light->foreground},
        {{11 * 8, 35 * 8}, "A:", Theme::getInstance()->fg_light->foreground}};

    OptionsField options_type{
        {7 * 8, 23 * 8},
        8,
        {
            {"Rand FSK", 0},
            {"FM tone", 1},
            {"CW sweep", 2},
            {"Rand CW", 3},
            {"Sine", 4},
            {"Square", 5},
            {"Sawtooth", 6},
            {"Triangle", 7},
            {"Chirp", 8},
            {"Gauss", 9},
            {"Brute", 10},
        }};

    Text text_range_number{
        {16 * 8, 23 * 8, 2 * 8, 16},
        "--"};
    Text text_range_total{
        {18 * 8, 23 * 8, 3 * 8, 16},
        "/--"};

    OptionsField options_speed{
        {7 * 8, 25 * 8},
        6,
        {{"10Hz  ", 10},
         {"100Hz ", 100},
         {"1kHz  ", 1000},
         {"10kHz ", 10000},
         {"100kHz", 100000}}};

    OptionsField options_hop{
        {7 * 8, 27 * 8},
        6,
        {{"0ms !!", 0},
         {"1ms  ", 1},
         {"10ms ", 10},
         {"50ms ", 50},
         {"100ms", 100},
         {"1s   ", 1000},
         {"2s   ", 2000},
         {"5s   ", 5000},
         {"10s  ", 10000}}};

    NumberField field_timetx{
        {7 * 8, 29 * 8},
        3,
        {1, 180},
        1,
        ' ',
    };

    NumberField field_timepause{
        {8 * 8, 31 * 8},
        2,
        {1, 60},
        1,
        ' ',
    };

    NumberField field_jitter{
        {8 * 8, 33 * 8},
        2,
        {1, 60},
        1,
        ' ',
    };

    NumberField field_gain{
        {8 * 8, 35 * 8},
        2,
        {0, 47},
        1,
        ' ',
    };

    NumberField field_amp{
        {13 * 8, 35 * 8},
        1,
        {0, 1},
        1,
        ' ',
    };

    Button button_transmit{
        {148, 216, 80, 80},
        LanguageHelper::currentMessages[LANG_START]};

    MessageHandlerRegistration message_handler_retune{
        Message::ID::Retune,
        [this](Message* const p) {
            const auto message = static_cast<const RetuneMessage*>(p);
            this->on_retune(message->freq, message->range);
        }};

    MessageHandlerRegistration message_handler_frame_sync{
        Message::ID::DisplayFrameSync,
        [this](const Message* const) {
            this->on_timer();
        }};
};

}  // namespace ui::external_app::hopper
