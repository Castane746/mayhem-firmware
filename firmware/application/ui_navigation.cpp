/*
 * Copyright (C) 2015 Jared Boone, ShareBrained Technology, Inc.
 * Copyright (C) 2016 Furrtek
 * Copyright (C) 2024 u-foka
 * Copyleft (ɔ) 2024 zxkmm under GPL license
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

#include "ui_navigation.hpp"

#include "event_m0.hpp"
#include "portapack_persistent_memory.hpp"
#include "portapack_shared_memory.hpp"
#include "portapack.hpp"

#include "ui_about_simple.hpp"
#include "ui_adsb_rx.hpp"
#include "ui_aprs_rx.hpp"
#include "ui_aprs_tx.hpp"
#include "ui_bht_tx.hpp"
#include "ui_btle_rx.hpp"
#include "ui_debug.hpp"
#include "ui_encoders.hpp"
#include "ui_fileman.hpp"
#include "ui_flash_utility.hpp"
#include "ui_font_fixed_8x16.hpp"
#include "ui_freqman.hpp"
#include "ui_iq_trim.hpp"
#include "ui_looking_glass_app.hpp"
#include "ui_mictx.hpp"

#include "ui_playlist.hpp"
#include "ui_pocsag_tx.hpp"
#include "ui_rds.hpp"
#include "ui_recon.hpp"
// #include "ui_scanner.hpp"
#include "ui_sd_over_usb.hpp"
#include "ui_search.hpp"
#include "ui_settings.hpp"
#include "ui_siggen.hpp"
#include "ui_sonde.hpp"
#include "ui_ss_viewer.hpp"
// #include "ui_test.hpp"
#include "ui_text_editor.hpp"
#include "ui_touchtunes.hpp"
#include "ui_weatherstation.hpp"
#include "ui_subghzd.hpp"
#include "ui_battinfo.hpp"
#include "ui_external_items_menu_loader.hpp"

#include "ais_app.hpp"
#include "analog_audio_app.hpp"
// #include "ble_comm_app.hpp"
#include "ble_rx_app.hpp"
#include "ble_tx_app.hpp"
#include "capture_app.hpp"
#include "ert_app.hpp"
#include "pocsag_app.hpp"
#include "soundboard_app.hpp"

#include "core_control.hpp"
#include "file.hpp"
#include "file_reader.hpp"
#include "png_writer.hpp"
#include "file_path.hpp"
#include "ff.h"

#include <locale>
#include <codecvt>

using portapack::receiver_model;
using portapack::transmitter_model;
namespace pmem = portapack::persistent_memory;

namespace ui {

bool CstrCmp::operator()(const char* a, const char* b) const {
    return strcmp(a, b) < 0;
}

static NavigationView::AppMap generate_app_map(const NavigationView::AppList& appList) {
    NavigationView::AppMap out;

    for (auto& app : appList) {
        if (app.id == nullptr) {
            // Skip items with no id
            continue;
        }

        auto res = out.emplace(app.id, app);
        if (!res.second) {
            chDbgPanic("Application cannot be added, ID not unique!");
        }
    }

    return out;
}

// TODO(u-foka): Check consistency of command names (where we add rx/tx postfix)
const NavigationView::AppList NavigationView::appList = {
    /* HOME ******************************************************************/
    {nullptr, "Receive", HOME, Color::cyan(), &bitmap_icon_receivers, new ViewFactory<ReceiversMenuView>()},
    {nullptr, "Transmit", HOME, Color::cyan(), &bitmap_icon_transmit, new ViewFactory<TransmittersMenuView>()},
    {nullptr, "Tranceiver", HOME, Color::cyan(), &bitmap_icon_tranceivers, new ViewFactory<TranceiversMenuView>()},
    {"recon", "Recon", HOME, Color::green(), &bitmap_icon_scanner, new ViewFactory<ReconView>()},
    {"capture", "Capture", HOME, Color::red(), &bitmap_icon_capture, new ViewFactory<CaptureAppView>()},
    {"replay", "Replay", HOME, Color::green(), &bitmap_icon_replay, new ViewFactory<PlaylistView>()},
    {"lookingglass", "Looking Glass", HOME, Color::green(), &bitmap_icon_looking, new ViewFactory<GlassView>()},
    {nullptr, "Utilities", HOME, Color::cyan(), &bitmap_icon_utilities, new ViewFactory<UtilitiesMenuView>()},
    {nullptr, "Games", HOME, Color::cyan(), &bitmap_icon_games, new ViewFactory<GamesMenuView>()},
    {nullptr, "Settings", HOME, Color::cyan(), &bitmap_icon_setup, new ViewFactory<SettingsMenuView>()},
    /* RX ********************************************************************/
    {"adsbrx", "ADS-B", RX, Color::green(), &bitmap_icon_adsb, new ViewFactory<ADSBRxView>()},
    {"ais", "AIS Boats", RX, Color::green(), &bitmap_icon_ais, new ViewFactory<AISAppView>()},
    {"aprsrx", "APRS", RX, Color::green(), &bitmap_icon_aprs, new ViewFactory<APRSRXView>()},
    {"audio", "Audio", RX, Color::green(), &bitmap_icon_speaker, new ViewFactory<AnalogAudioView>()},
    {"blerx", "BLE Rx", RX, Color::green(), &bitmap_icon_btle, new ViewFactory<BLERxView>()},
    {"ert", "ERT Meter", RX, Color::green(), &bitmap_icon_ert, new ViewFactory<ERTAppView>()},
    {"pocsag", "POCSAG", RX, Color::green(), &bitmap_icon_pocsag, new ViewFactory<POCSAGAppView>()},
    {"radiosonde", "Radiosnde", RX, Color::green(), &bitmap_icon_sonde, new ViewFactory<SondeView>()},
    {"search", "Search", RX, Color::yellow(), &bitmap_icon_search, new ViewFactory<SearchView>()},
    {"subghzd", "SubGhzD", RX, Color::yellow(), &bitmap_icon_remote, new ViewFactory<SubGhzDView>()},
    {"weather", "Weather", RX, Color::green(), &bitmap_icon_thermometer, new ViewFactory<WeatherView>()},
    /* TX ********************************************************************/
    {"aprstx", "APRS TX", TX, ui::Color::green(), &bitmap_icon_aprs, new ViewFactory<APRSTXView>()},
    {"bht", "BHT Xy/EP", TX, ui::Color::green(), &bitmap_icon_bht, new ViewFactory<BHTView>()},
    {"bletx", "BLE Tx", TX, ui::Color::green(), &bitmap_icon_btle, new ViewFactory<BLETxView>()},
    {"ooktx", "OOK", TX, ui::Color::yellow(), &bitmap_icon_remote, new ViewFactory<EncodersView>()},
    {"pocsagtx", "POCSAG TX", TX, ui::Color::green(), &bitmap_icon_pocsag, new ViewFactory<POCSAGTXView>()},
    {"rdstx", "RDS", TX, ui::Color::green(), &bitmap_icon_rds, new ViewFactory<RDSView>()},
    {"soundbrd", "Soundbrd", TX, ui::Color::green(), &bitmap_icon_soundboard, new ViewFactory<SoundBoardView>()},
    {"touchtune", "TouchTune", TX, ui::Color::green(), &bitmap_icon_touchtunes, new ViewFactory<TouchTunesView>()},
    {"signalgen", "SignalGen", TX, Color::green(), &bitmap_icon_cwgen, new ViewFactory<SigGenView>()},
    /* TRX ********************************************************************/
    {"microphone", "Mic", TRX, Color::green(), &bitmap_icon_microphone, new ViewFactory<MicTXView>()},
    /* UTILITIES *************************************************************/
    {"filemanager", "File Manager", UTILITIES, Color::green(), &bitmap_icon_dir, new ViewFactory<FileManagerView>()},
    {"freqman", "Freq. Manager", UTILITIES, Color::green(), &bitmap_icon_freqman, new ViewFactory<FrequencyManagerView>()},
    {"iqtrim", "IQ Trim", UTILITIES, Color::orange(), &bitmap_icon_trim, new ViewFactory<IQTrimView>()},
    {"notepad", "Notepad", UTILITIES, Color::dark_cyan(), &bitmap_icon_notepad, new ViewFactory<TextEditorView>()},
    {nullptr, "SD Over USB", UTILITIES, Color::yellow(), &bitmap_icon_hackrf, new ViewFactory<SdOverUsbView>()},
    {nullptr, "Debug", UTILITIES, Color::light_grey(), &bitmap_icon_debug, new ViewFactory<DebugMenuView>()},
    //{"testapp", "Test App", UTILITIES, Color::dark_grey(), nullptr, new ViewFactory<TestView>()},
    // Dangerous apps.
    {nullptr, "Flash Utility", UTILITIES, Color::red(), &bitmap_icon_peripherals_details, new ViewFactory<FlashUtilityView>()},
};

const NavigationView::AppMap NavigationView::appMap = generate_app_map(NavigationView::appList);

bool NavigationView::StartAppByName(const char* name) {
    home(false);

    auto it = appMap.find(name);
    if (it != appMap.end()) {
        push_view(std::unique_ptr<View>(it->second.viewFactory->produce(*this)));
        return true;
    }

    return false;
}

/* StatusTray ************************************************************/

StatusTray::StatusTray(Point pos)
    : View{{pos, {0, height}}},
      pos_(pos) {
    set_focusable(false);
}

void StatusTray::add(Widget* child) {
    width_ += child->parent_rect().width();
    add_child(child);
}

void StatusTray::update_layout() {
    // Widen the tray's parent rect.
    auto rect = parent_rect();
    set_parent_rect({{rect.left() - width_, rect.top()}, {rect.right() + width_, height}});

    // Update the children.
    auto x = 0;
    for (auto child : children()) {
        auto size = child->parent_rect().size();
        child->set_parent_rect({{x, 0}, size});
        x += size.width();
    }
    set_dirty();
}

void StatusTray::clear() {
    // More efficient than 'remove_children'.
    for (auto child : children())
        child->set_parent(nullptr);
    children_.clear();
    width_ = 0;
    set_parent_rect({pos_, {width_, height}});
    set_dirty();
}

void StatusTray::paint(Painter&) {
}

/* SystemStatusView ******************************************************/

SystemStatusView::SystemStatusView(
    NavigationView& nav)
    : nav_(nav) {
    add_children({
        &backdrop,
        &button_back,
        &title,
        &button_title,
        &status_icons,
    });

    rtc_battery_workaround();

    ui::load_blacklist();

    if (pmem::should_use_sdcard_for_pmem()) {
        pmem::load_persistent_settings_from_file();
    }

    // configure CLKOUT per pmem setting
    portapack::clock_manager.enable_clock_output(pmem::clkout_enabled());

    // force apply of selected sdcard speed override at UI startup
    pmem::set_config_sdcard_high_speed_io(pmem::config_sdcard_high_speed_io(), false);

    button_back.id = -1;  // Special ID used by FocusManager
    title.set_style(Theme::getInstance()->bg_dark);

    button_back.on_select = [this](ImageButton&) {
        if (pmem::should_use_sdcard_for_pmem()) {
            pmem::save_persistent_settings_to_file();
        }
        if (this->on_back)
            this->on_back();
    };

    button_title.on_select = [this](ImageButton&) {
        this->on_title();
    };

    button_converter.on_select = [this](ImageButton&) {
        this->on_converter();
    };

    toggle_speaker.on_change = [this](bool v) {
        pmem::set_config_speaker_disable(v);
        audio::output::update_audio_mute();
        refresh();
    };

    toggle_mute.on_change = [this](bool v) {
        pmem::set_config_audio_mute(v);
        audio::output::update_audio_mute();
        refresh();
    };

    toggle_stealth.on_change = [this, &nav](bool v) {
        pmem::set_stealth_mode(v);
        if (nav.is_valid() && v) {
            nav.display_modal(
                "Stealth",
                "You just enabled stealth mode.\n"
                "When you transmit,\n"
                "screen will turn off;\n");
        }
        refresh();
    };

    battery_icon.on_select = [this]() { on_battery_details(); };
    battery_text.on_select = [this]() { on_battery_details(); };

    button_bias_tee.on_select = [this](ImageButton&) {
        this->on_bias_tee();
    };

    button_fake_brightness.on_select = [this](ImageButton&) {
        set_dirty();
        pmem::toggle_fake_brightness_level();
        refresh();
        if (nullptr != parent()) {
            parent()->set_dirty();  // The parent of NavigationView shal be the SystemView
        }
    };

    button_camera.on_select = [this](ImageButton&) {
        this->on_camera();
    };

    button_sleep.on_select = [this](ImageButton&) {
        DisplaySleepMessage message;
        EventDispatcher::send_message(message);
    };

    button_clock_status.on_select = [this](ImageButton&) {
        this->on_clk();
    };

    // Initialize toggle buttons
    toggle_speaker.set_value(pmem::config_speaker_disable());
    toggle_mute.set_value(pmem::config_audio_mute());
    toggle_stealth.set_value(pmem::stealth_mode());

    audio::output::stop();
    audio::output::update_audio_mute();

    refresh();
}

// when battery icon / text is clicked
void SystemStatusView::on_battery_details() {
    if (!nav_.is_valid()) return;
    if (batt_info_up) return;
    batt_info_up = true;
    nav_.push<BattinfoView>();
    nav_.set_on_pop([this]() {
        batt_info_up = false;
    });
}

void SystemStatusView::on_battery_data(const BatteryStateMessage* msg) {
    if (!batt_was_inited) {
        batt_was_inited = true;
        refresh();
    }

    // Check if charging state changed to charging
    static bool was_charging = false;
    if (msg->on_charger && !was_charging && pmem::ui_battery_charge_hint()) {
        // Only show charging modal when transitioning to charging state
        nav_.display_modal(
            "CHARGING",
            "Screen on while charging?",
            YESNO,
            [this](bool keep_screen_on) {
                if (!keep_screen_on) {
                    EventDispatcher::set_display_sleep(true);
                }
            });
    }
    was_charging = msg->on_charger;

    if (!pmem::ui_hide_numeric_battery()) {
        battery_text.set_battery(msg->valid_mask, msg->percent, msg->on_charger);
    }
    if (!pmem::ui_hide_battery_icon()) {
        battery_icon.set_battery(msg->valid_mask, msg->percent, msg->on_charger);
    }
}

void SystemStatusView::refresh() {
    // NB: Order of insertion is the display order Left->Right.
    // TODO: Might be better to support hide and only add once.
    status_icons.clear();
    if (!pmem::ui_hide_camera()) status_icons.add(&button_camera);
    if (!pmem::ui_hide_sleep()) status_icons.add(&button_sleep);
    if (!pmem::ui_hide_stealth()) status_icons.add(&toggle_stealth);
    if (!pmem::ui_hide_converter()) status_icons.add(&button_converter);
    if (!pmem::ui_hide_bias_tee()) status_icons.add(&button_bias_tee);
    if (!pmem::ui_hide_clock()) status_icons.add(&button_clock_status);
    if (!pmem::ui_hide_mute()) status_icons.add(&toggle_mute);

    // Display "Disable speaker" icon only if AK4951 Codec which has separate speaker/headphone control
    if (audio::speaker_disable_supported() && !pmem::ui_hide_speaker()) status_icons.add(&toggle_speaker);

    if (!pmem::ui_hide_fake_brightness()) status_icons.add(&button_fake_brightness);
    if (battery::BatteryManagement::isDetected()) {
        batt_was_inited = true;
        if (!pmem::ui_hide_battery_icon()) {
            status_icons.add(&battery_icon);
        };
        if (!pmem::ui_hide_numeric_battery()) {
            status_icons.add(&battery_text);
        }
    }

    if (!pmem::ui_hide_sd_card()) status_icons.add(&sd_card_status_view);

    status_icons.update_layout();

    // Clock status
    bool external_clk = portapack::clock_manager.get_reference().source == ClockManager::ReferenceSource::External;
    button_clock_status.set_bitmap(external_clk ? &bitmap_icon_clk_ext : &bitmap_icon_clk_int);
    button_clock_status.set_foreground(pmem::clkout_enabled() ? *Theme::getInstance()->status_active : Theme::getInstance()->fg_light->foreground);

    // Antenna DC Bias
    if (portapack::get_antenna_bias()) {
        button_bias_tee.set_bitmap(&bitmap_icon_biast_on);
        button_bias_tee.set_foreground(Theme::getInstance()->warning_dark->foreground);
    } else {
        button_bias_tee.set_bitmap(&bitmap_icon_biast_off);
        button_bias_tee.set_foreground(Theme::getInstance()->fg_light->foreground);
    }

    // Converter
    button_converter.set_bitmap(pmem::config_updown_converter() ? &bitmap_icon_downconvert : &bitmap_icon_upconvert);
    button_converter.set_foreground(pmem::config_converter() ? Theme::getInstance()->fg_red->foreground : Theme::getInstance()->fg_light->foreground);

    // Fake Brightness
    button_fake_brightness.set_foreground(pmem::apply_fake_brightness() ? *Theme::getInstance()->status_active : Theme::getInstance()->fg_light->foreground);

    set_dirty();
}

void SystemStatusView::set_back_enabled(bool new_value) {
    if (new_value) {
        add_child(&button_back);
    } else {
        remove_child(&button_back);
    }
}

void SystemStatusView::set_back_hidden(bool new_value) {
    button_back.hidden(new_value);
}

void SystemStatusView::set_title_image_enabled(bool new_value) {
    if (new_value) {
        add_child(&button_title);
    } else {
        remove_child(&button_title);
    }
}

void SystemStatusView::set_title(const std::string new_value) {
    if (new_value.empty()) {
        title.set(default_title);
    } else {
        // Limit length of title string to prevent partial characters if too many StatusView icons
        size_t max_len = (status_icons.parent_rect().left() - title.parent_rect().left()) / 8;
        title.set(truncate(new_value, max_len));
    }
}

void SystemStatusView::on_converter() {
    pmem::set_config_converter(!pmem::config_converter());

    // Poke to update tuning
    // NOTE: Code assumes here that a TX app isn't active, since RX & TX have diff tuning offsets
    // (and there's only one tuner in the radio so can't update tuner for both).
    // TODO: Maybe expose the 'enabled_' flag on models.
    receiver_model.set_target_frequency(receiver_model.target_frequency());
    refresh();
}

void SystemStatusView::on_bias_tee() {
    if (!portapack::get_antenna_bias()) {
        nav_.display_modal(
            "Bias voltage",
            "Enable DC voltage on\nantenna connector?",
            YESNO,
            [this](bool v) {
                if (v) {
                    portapack::set_antenna_bias(true);
                    receiver_model.set_antenna_bias();
                    transmitter_model.set_antenna_bias();
                    refresh();
                }
            });
    } else {
        portapack::set_antenna_bias(false);
        receiver_model.set_antenna_bias();
        transmitter_model.set_antenna_bias();

        // Ensure this is disabled. The models don't actually
        // update the radio unless they are 'enabled_'.
        radio::set_antenna_bias(false);
        refresh();
    }
}

void SystemStatusView::on_camera() {
    ensure_directory(screenshots_dir);
    auto path = next_filename_matching_pattern(screenshots_dir / u"SCR_????.PNG");

    if (path.empty())
        return;

    PNGWriter png;
    auto error = png.create(path);
    if (error)
        return;

    for (int i = 0; i < screen_height; i++) {
        std::vector<ColorRGB888> row(ui::screen_width);
        portapack::display.read_pixels({0, i, screen_width, 1}, row);
        png.write_scanline(row);
    }
}

void SystemStatusView::on_clk() {
    pmem::set_clkout_enabled(!pmem::clkout_enabled());
    portapack::clock_manager.enable_clock_output(pmem::clkout_enabled());
    refresh();
}

void SystemStatusView::on_title() {
    if (nav_.is_top())
        nav_.push<AboutView>();
    else
        nav_.pop();
}

void SystemStatusView::rtc_battery_workaround() {
    if (sd_card::status() != sd_card::Status::Mounted)
        return;

    uint16_t year;
    uint8_t month;
    uint8_t day;
    FATTimestamp timestamp;
    rtc::RTC datetime;

    rtcGetTime(&RTCD1, &datetime);

    // if year is 0000, assume RTC battery is dead
    if (datetime.year() == 0) {
        // if timestamp file is present, use it's date and add 1 day
        if (std::filesystem::file_exists(DATE_FILEFLAG)) {
            timestamp = file_created_date(DATE_FILEFLAG);

            year = (timestamp.FAT_date >> 9) + 1980;
            month = (timestamp.FAT_date >> 5) & 0xF;
            day = timestamp.FAT_date & 0x1F;

            // bump to next month
            if (++day > rtc_time::days_per_month(year, month)) {
                day = 1;
                if (++month > 12) {
                    month = 1;
                    ++year;
                }
            }
        } else {
            ensure_directory(settings_dir);
            make_new_file(DATE_FILEFLAG);

            year = 1980;
            month = 1;
            day = 1;
        }

        // update RTC (keeps ticking while powered on regardless of RTC battery condition)
        rtc::RTC new_datetime{year, month, day, datetime.hour(), datetime.minute(), datetime.second()};
        rtcSetTime(&RTCD1, &new_datetime);

        // update file date
        timestamp.FAT_date = ((year - 1980) << 9) | ((uint16_t)month << 5) | day;
        timestamp.FAT_time = 0;
        file_update_date(DATE_FILEFLAG, timestamp);
    }
}

/* Information View *****************************************************/

InformationView::InformationView(
    NavigationView& nav)
    : nav_(nav) {
    add_children({&backdrop,
                  &version,
                  &ltime});

#if GCC_VERSION_MISMATCH
    version.set_style(Theme::getInstance()->warning_dark);
#else
    version.set_style(Theme::getInstance()->bg_darker);
#endif

    if (firmware_checksum_error()) {
        version.set("FLASH ERR");
        version.set_style(Theme::getInstance()->error_dark);
    }

    ltime.set_style(Theme::getInstance()->bg_darker);
    refresh();
    set_dirty();
}

void InformationView::refresh() {
    ltime.set_hide_clock(pmem::hide_clock());
    ltime.set_seconds_enabled(true);
    ltime.set_date_enabled(pmem::clock_with_date());
}

bool InformationView::firmware_checksum_error() {
    static bool fw_checksum_checked{false};
    static bool fw_checksum_error{false};

    // only checking firmware checksum once per boot
    if (!fw_checksum_checked) {
        fw_checksum_error = (simple_checksum(FLASH_STARTING_ADDRESS, FLASH_ROM_SIZE) != FLASH_EXPECTED_CHECKSUM);
    }
    return fw_checksum_error;
}

/* Navigation ************************************************************/

bool NavigationView::is_top() const {
    return view_stack.size() == 1;
}

bool NavigationView::is_valid() const {
    return view_stack.size() != 0;  // work around to check if nav is valid, not elegant i know. so TODO
}

View* NavigationView::push_view(std::unique_ptr<View> new_view) {
    free_view();
    const auto p = new_view.get();
    view_stack.emplace_back(ViewState{std::move(new_view), {}});

    update_view();
    return p;
}

void NavigationView::pop(bool trigger_update) {
    // Don't pop off the NavView.
    if (view_stack.size() <= 1)
        return;

    auto on_pop = view_stack.back().on_pop;

    free_view();
    view_stack.pop_back();

    // NB: These are executed _after_ the view has been
    // destroyed. The old view MUST NOT be referenced in
    // these callbacks or it will cause crashes.
    if (trigger_update) update_view();
    if (on_pop) on_pop();
}

void NavigationView::home(bool trigger_update) {
    while (view_stack.size() > 1) {
        pop(false);
    }

    if (trigger_update) update_view();
}

void NavigationView::display_modal(
    const std::string& title,
    const std::string& message) {
    display_modal(title, message, INFO, nullptr);
}

void NavigationView::display_modal(
    const std::string& title,
    const std::string& message,
    modal_t type,
    std::function<void(bool)> on_choice,
    bool compact) {
    push<ModalMessageView>(title, message, type, on_choice, compact);
}

void NavigationView::free_view() {
    // The focus_manager holds a raw pointer to the currently focused Widget.
    // It then tries to call blur() on that instance when the focus is changed.
    // This causes crashes if focused_widget has been deleted (as is the case
    // when a view is popped). Calling blur() here resets the focus_manager's
    // focus_widget pointer so focus can be called safely.
    this->blur();
    remove_child(view());
}

void NavigationView::update_view() {
    const auto& top = view_stack.back();
    auto top_view = top.view.get();

    add_child(top_view);
    auto newSize = (is_top()) ? Size{size().width(), size().height() - 16} : size();  // if top(), then there is the info bar at the bottom, so leave space for it
    top_view->set_parent_rect({{0, 0}, newSize});
    focus();
    set_dirty();

    if (on_view_changed)
        on_view_changed(*top_view);
}

Widget* NavigationView::view() const {
    return children_.empty() ? nullptr : children_[0];
}

void NavigationView::focus() {
    if (view())
        view()->focus();
}

bool NavigationView::set_on_pop(std::function<void()> on_pop) {
    if (view_stack.size() <= 1)
        return false;

    auto& top = view_stack.back();
    if (top.on_pop)
        return false;

    top.on_pop = on_pop;
    return true;
}

void NavigationView::handle_autostart() {
    std::string autostart_app{""};
    SettingsStore nav_setting{
        "nav"sv,
        {{"autostart_app"sv, &autostart_app}}};
    if (!autostart_app.empty()) {
        bool started = false;
        // inner app
        if (StartAppByName(autostart_app.c_str())) {
            started = true;
        }

        if (!started) {
            // ppma

            std::string appwithpath = "/" + apps_dir.string() + "/" + autostart_app + ".ppma";
            std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t> conv;
            std::filesystem::path pth = conv.from_bytes(appwithpath.c_str());
            if (ui::ExternalItemsMenuLoader::run_external_app(*this, pth)) {
                started = true;
            }

            if (!started) {
                // ppmp / standalone
                appwithpath = "/" + apps_dir.string() + "/" + autostart_app + ".ppmp";
                pth = conv.from_bytes(appwithpath.c_str());
                if (ui::ExternalItemsMenuLoader::run_standalone_app(*this, pth)) {
                    started = true;
                }
            }
        }
        if (!started) {
            display_modal(
                "Notice", "Autostart failed:\n" +
                              autostart_app +
                              "\nupdate sdcard content\n" +
                              "and check if .ppma exists");
        }
    }  // autostart end
    return;
}

/* Helpers  **************************************************************/

void add_apps(NavigationView& nav, BtnGridView& grid, app_location_t loc) {
    for (auto& app : NavigationView::appList) {
        if (app.menuLocation == loc) {
            grid.add_item({app.displayName, app.iconColor, app.icon,
                           [&nav, &app]() { 
                            i2cdev::I2CDevManager::set_autoscan_interval(0); //if i navigate away from any menu, turn off autoscan
                            nav.push_view(std::unique_ptr<View>(app.viewFactory->produce(nav))); }},
                          true);
        }
    };

    grid.update_items();
}

// clang-format off
void add_external_items(NavigationView& nav, app_location_t location, BtnGridView& grid, uint8_t error_tile_pos) {
    auto externalItems = ExternalItemsMenuLoader::load_external_items(location, nav);
    if (externalItems.empty()) {
        grid.insert_item({"ExtAppErr",
                          Theme::getInstance()->error_dark->foreground,
                          nullptr,
                          [&nav]() {
                              nav.display_modal(
                                  "Notice",
                                  "Can't read external apps\n"
                                  "Check SD card\n"
                                  "Update SD card content\n");
                          }},
                         error_tile_pos);
    } else {
        std::sort(externalItems.begin(), externalItems.end(), [](const auto &a, const auto &b)
        { 
            return a.desired_position < b.desired_position; 
        });

        for (auto const& gridItem : externalItems) {
            if (gridItem.desired_position < 0) {
                grid.add_item(gridItem, true);
            } else {
                grid.insert_item(gridItem, gridItem.desired_position, true);
            }
            
        }

        grid.update_items();
    }
}
// clang-format on

bool verify_sdcard_format() {
    FATFS* fs = &sd_card::fs;
    return (fs->fs_type == FS_FAT32 || fs->fs_type == FS_EXFAT) || !(sd_card::status() == sd_card::Status::Mounted);
    /*                                                             ^ to satisfy those users that not use an sd*/
}

/* ReceiversMenuView *****************************************************/

ReceiversMenuView::ReceiversMenuView(NavigationView& nav)
    : nav_(nav) {}

void ReceiversMenuView::on_populate() {
    bool return_icon = pmem::show_gui_return_icon();
    if (return_icon) {
        add_item({"..", Theme::getInstance()->fg_light->foreground, &bitmap_icon_previous, [this]() { nav_.pop(); }});
    }
    add_apps(nav_, *this, RX);
    add_external_items(nav_, app_location_t::RX, *this, return_icon ? 1 : 0);
}

/* TransmittersMenuView **************************************************/

TransmittersMenuView::TransmittersMenuView(NavigationView& nav)
    : nav_(nav) {}

void TransmittersMenuView::on_populate() {
    bool return_icon = pmem::show_gui_return_icon();
    if (return_icon) {
        add_items({{"..", Theme::getInstance()->fg_light->foreground, &bitmap_icon_previous, [this]() { nav_.pop(); }}});
    }
    add_apps(nav_, *this, TX);
    add_external_items(nav_, app_location_t::TX, *this, return_icon ? 1 : 0);
}

/* TranceiversMenuView **************************************************/

TranceiversMenuView::TranceiversMenuView(NavigationView& nav)
    : nav_(nav) {}

void TranceiversMenuView::on_populate() {
    bool return_icon = pmem::show_gui_return_icon();
    if (return_icon) {
        add_items({{"..", Theme::getInstance()->fg_light->foreground, &bitmap_icon_previous, [this]() { nav_.pop(); }}});
    }
    add_apps(nav_, *this, TRX);
    // add_external_items(nav_, app_location_t::TRX, *this, return_icon ? 1 : 0);
    // this folder doesn't have external apps, comment to prevent pop the err msg.
    // NB: when has external app someday, uncomment this.
}

/* UtilitiesMenuView *****************************************************/

UtilitiesMenuView::UtilitiesMenuView(NavigationView& nav)
    : nav_(nav) {
    set_max_rows(2);  // allow wider buttons
}

void UtilitiesMenuView::on_populate() {
    bool return_icon = pmem::show_gui_return_icon();
    if (return_icon) {
        add_items({{"..", Theme::getInstance()->fg_light->foreground, &bitmap_icon_previous, [this]() { nav_.pop(); }}});
    }
    add_apps(nav_, *this, UTILITIES);
    add_external_items(nav_, app_location_t::UTILITIES, *this, return_icon ? 1 : 0);
}

/* GamesMenuView ********************************************************/

GamesMenuView::GamesMenuView(NavigationView& nav)
    : nav_(nav) {
    set_max_rows(2);
}

void GamesMenuView::on_populate() {
    bool return_icon = pmem::show_gui_return_icon();
    if (return_icon) {
        add_item({"..", Theme::getInstance()->fg_light->foreground, &bitmap_icon_previous, [this]() { nav_.pop(); }});
    }
    add_apps(nav_, *this, GAMES);
    add_external_items(nav_, app_location_t::GAMES, *this, return_icon ? 1 : 0);
}

/* SystemMenuView ********************************************************/

void SystemMenuView::hackrf_mode(NavigationView& nav) {
    nav.push<ModalMessageView>(
        "HackRF mode",
        " This mode enables HackRF\n functionality. To return,\n  press the reset button.\n\n  Switch to HackRF mode?",
        YESNO,
        [this](bool choice) {
            if (choice) {
                EventDispatcher::request_stop();
            }
        });
}

SystemMenuView::SystemMenuView(NavigationView& nav)
    : nav_(nav) {
    set_max_rows(2);  // allow wider buttons
    show_arrows_enabled(false);
}

void SystemMenuView::on_populate() {
    if (!verify_sdcard_format()) {
        add_item({"SDCard Error", Theme::getInstance()->error_dark->foreground, nullptr, [this]() {
                      nav_.display_modal("Error", "SD Card is not exFAT/FAT32");
                  }});
    }
    add_apps(nav_, *this, HOME);
    add_external_items(nav_, app_location_t::HOME, *this, 0);
    add_item({"HackRF", Theme::getInstance()->fg_cyan->foreground, &bitmap_icon_hackrf, [this]() { hackrf_mode(nav_); }});
}

/* SystemView ************************************************************/

SystemView::SystemView(
    Context& context,
    const Rect parent_rect)
    : View{parent_rect},
      context_(context) {
    set_style(Theme::getInstance()->bg_darkest);

    constexpr Dim status_view_height = 16;
    constexpr Dim info_view_height = 16;

    add_child(&status_view);
    status_view.set_parent_rect(
        {{0, 0},
         {parent_rect.width(), status_view_height}});
    status_view.on_back = [this]() {
        this->navigation_view.pop();
    };

    add_child(&navigation_view);
    navigation_view.set_parent_rect(
        {{0, status_view_height},
         {parent_rect.width(), static_cast<Dim>(parent_rect.height() - status_view_height)}});

    add_child(&info_view);
    info_view.set_parent_rect(
        {{0, screen_height - 16},
         {screen_width, info_view_height}});

    navigation_view.on_view_changed = [this](const View& new_view) {
        if (!this->navigation_view.is_top()) {
            remove_child(&info_view);
        } else {
            add_child(&info_view);
            info_view.refresh();
            i2cdev::I2CDevManager::set_autoscan_interval(3);  // turn on autoscan in sysmainv
        }

        this->status_view.set_back_enabled(!this->navigation_view.is_top());
        this->status_view.set_title_image_enabled(this->navigation_view.is_top());
        this->status_view.set_title(new_view.title());
        this->status_view.set_dirty();
    };

    navigation_view.push<SystemMenuView>();

    if (pmem::config_splash()) {
        navigation_view.push<SplashScreenView>();
    }
    status_view.set_back_enabled(false);
    status_view.set_title_image_enabled(true);
    status_view.set_dirty();
}

Context& SystemView::context() const {
    return context_;
}
NavigationView* SystemView::get_navigation_view() {
    return &navigation_view;
}
SystemStatusView* SystemView::get_status_view() {
    return &status_view;
}

void SystemView::toggle_overlay() {
    static uint8_t last_perf_counter_status = shared_memory.request_m4_performance_counter;
    switch (++overlay_active) {
        case 1:
            this->add_child(&this->overlay);
            this->set_dirty();
            shared_memory.request_m4_performance_counter = 1;
            shared_memory.m4_performance_counter = 0;
            shared_memory.m4_heap_usage = 0;
            shared_memory.m4_stack_usage = 0;
            break;
        case 2:
            this->remove_child(&this->overlay);
            this->add_child(&this->overlay2);
            this->set_dirty();
            shared_memory.request_m4_performance_counter = 2;
            break;
        case 3:
            this->remove_child(&this->overlay2);
            this->set_dirty();
            shared_memory.request_m4_performance_counter = last_perf_counter_status;
            overlay_active = 0;
            break;
    }
}

void SystemView::paint_overlay() {
    static bool last_paint_state = false;
    if (overlay_active) {
        // paint background only every other second
        if ((((chTimeNow() >> 10) & 0x01) == 0x01) == last_paint_state)
            return;

        last_paint_state = !last_paint_state;
        if (overlay_active == 1)
            this->overlay.set_dirty();
        else
            this->overlay2.set_dirty();
    }
}

void SystemView::set_app_fullscreen(bool fullscreen) {
    auto parent_rect = screen_rect();
    Dim status_view_height = (fullscreen) ? 0 : 16;
    status_view.hidden(fullscreen);
    navigation_view.set_parent_rect(
        {{0, status_view_height},
         {parent_rect.width(), static_cast<Dim>(parent_rect.height() - status_view_height)}});
}

/* ***********************************************************************/

void SplashScreenView::focus() {
    button_done.focus();
}

SplashScreenView::SplashScreenView(NavigationView& nav)
    : nav_(nav) {
    add_children({&button_done});

    button_done.on_select = [this](Button&) {
        handle_pop();
    };
}

void SplashScreenView::paint(Painter&) {
    if (!portapack::display.draw_bmp_from_sdcard_file({0, 0}, splash_dot_bmp))
        // ^ try draw bmp file from sdcard at (0,0), and the (0,0) already bypassed the status bar, so actual pos is (0, STATUS_BAR_HEIGHT)
        portapack::display.draw_bitmap({0,
                                        screen_height / 2},
                                       bitmap_titlebar_image.size,
                                       bitmap_titlebar_image.data,
                                       Theme::getInstance()->bg_darkest->foreground,
                                       Theme::getInstance()->bg_darkest->background, 3);
    // ^ draw BMP HEX arr in firmware, note that the BMP HEX arr only cover the image part (instead of fill the screen with background, this position is located it in the center)
}

bool SplashScreenView::on_touch(const TouchEvent event) {
    /* the event thing were resolved by HTotoo, talked here https://discord.com/channels/719669764804444213/956561375155589192/1287756910950486027
     * the touch screen policy can be better, talked here https://discord.com/channels/719669764804444213/956561375155589192/1198926225897443328
     * this workaround discussed here: https://discord.com/channels/719669764804444213/1170738202924044338/1295630640158478418
     */

    if (!nav_.is_valid()) {
        return false;
    }

    switch (event.type) {
        case TouchEvent::Type::Start:
            handle_pop();
            return false;

        default:
            break;
    }

    return false;
}

void SplashScreenView::handle_pop() {
    if (nav_.is_valid()) {
        nav_.pop();
    }
}

/* NotImplementedView ****************************************************/

/*NotImplementedView::NotImplementedView(NavigationView& nav) {
        button_done.on_select = [&nav](Button&){
                nav.pop();
        };

        add_children({
                &text_title,
                &button_done,
        });
}

void NotImplementedView::focus() {
        button_done.focus();
}*/

/* ModalMessageView ******************************************************/

ModalMessageView::ModalMessageView(
    NavigationView& nav,
    const std::string& title,
    const std::string& message,
    modal_t type,
    std::function<void(bool)> on_choice,
    bool compact)
    : title_{title},
      message_{message},
      type_{type},
      on_choice_{on_choice},
      compact{compact} {
    if (type == INFO) {
        add_child(&button_ok);
        button_ok.on_select = [this, &nav](Button&) {
            if (on_choice_) on_choice_(true);
            nav.pop();
        };

    } else if (type == YESNO) {
        add_children({&button_yes,
                      &button_no});

        button_yes.on_select = [this, &nav](Button&) {
            if (on_choice_) on_choice_(true);
            nav.pop();
        };
        button_no.on_select = [this, &nav](Button&) {
            if (on_choice_) on_choice_(false);
            nav.pop();
        };

    } else {  // ABORT
        add_child(&button_ok);

        button_ok.on_select = [this, &nav](Button&) {
            if (on_choice_) on_choice_(true);
            nav.pop(false);  // Pop the modal.
            nav.pop();       // Pop the underlying view.
        };
    }
}

void ModalMessageView::paint(Painter& painter) {
    if (!compact) portapack::display.draw_bitmap({screen_width / 2 - 3 * 16 / 2,
                                                  screen_height / 2 - 3 * 16 / 2 - 100},
                                                 bitmap_icon_utilities.size,
                                                 bitmap_icon_utilities.data,
                                                 Theme::getInstance()->bg_darkest->foreground,
                                                 Theme::getInstance()->bg_darkest->background, 3);

    // Break lines.
    auto lines = split_string(message_, '\n');
    for (size_t i = 0; i < lines.size(); ++i) {
        painter.draw_string(
            {1 * 8, (Coord)(((compact) ? 8 * 3 : 120) + (i * 16))},
            style(),
            lines[i]);
    }
}

void ModalMessageView::focus() {
    if ((type_ == YESNO)) {
        button_yes.focus();
    } else {
        button_ok.focus();
    }
}

} /* namespace ui */
