#pragma once

#include <QString>

// Opaque Rust state handle and the C ABI declared in crates/core/src/lib.rs.
extern "C" {
void *namaz_core_new(void);
void namaz_core_free(void *core);
void namaz_core_free_string(char *value);

char *namaz_core_day_json(void *core);
char *namaz_core_month_json(void *core);
char *namaz_core_year_json(void *core);

void namaz_core_tick(void *core);
char *namaz_core_tick_events(void *core);
void namaz_core_startup(void *core);
char *namaz_core_csv_month(void *core, int year, int month);
char *namaz_core_csv_year(void *core, int year);
char *namaz_core_csv_month_calendar(void *core, int year, int month, int calendar);
char *namaz_core_csv_year_calendar(void *core, int year, int calendar);
char *namaz_core_csv_hijri_month(void *core, int hijri_year, int hijri_month);
char *namaz_core_csv_hijri_year(void *core, int hijri_year);
void namaz_core_shift_day(void *core, int days);
void namaz_core_shift_month(void *core, int months);
void namaz_core_shift_year(void *core, int years);
void namaz_core_go_today(void *core);
void namaz_core_set_date(void *core, const char *iso_date);
void namaz_core_set_view(void *core, int view);

void namaz_core_set_location(void *core, double latitude, double longitude,
                              double timezone, const char *city);
void namaz_core_set_hijri_offset(void *core, int offset);
char *namaz_core_config_json(void *core);
char *namaz_core_default_config_json(void);
char *namaz_core_countries_json(void);
char *namaz_core_country_cities_json(const char *country);
char *namaz_core_find_city_json(const char *country, const char *city);
void namaz_core_set_flag(void *core, const char *key, int value);
int namaz_core_apply_settings_json(void *core, const char *json);
int namaz_core_apply_preferences_json(void *core, const char *json);

int namaz_core_play_sound(const char *name);
int namaz_core_play_path(const char *path);
int namaz_core_play_folder(const char *folder);
int namaz_core_play_quran_random(void);
int namaz_core_play_random_athan(void);
int namaz_core_play_iqama_random(void);
void namaz_core_set_volume(int volume);
int namaz_core_volume(void);
int namaz_core_is_playing(void);
void namaz_core_stop_audio(void);
char *namaz_core_available_sounds_json(void);
char *namaz_core_athan_sounds_json(void);
int namaz_core_sound_exists(const char *name);
}

class Core {
public:
    Core();
    ~Core();
    Core(const Core &) = delete;
    Core &operator=(const Core &) = delete;

    void *handle() const { return m_handle; }

private:
    void *m_handle = nullptr;
};

QString takeCoreString(char *value);
