#include "codex_dashboard.h"

#include <lvgl.h>
#include <lv_tiny_ttf.h>
#include <rtthread.h>
#include <stdlib.h>
#include <string.h>

/* Three physical 128 x 128 panels form one 384 x 128 LVGL canvas.
 *
 * Layout is transcribed 1:1 from Claude's companion UI specification:
 * all three canvases use the same centred grid: title y=12, hero y=36/46/48,
 * divider y=98, footer y=106.  The only line in each canvas is that divider;
 * it has 14 px insets. */
#define PANEL_W 128
#define PANEL_H 128

#define COLOR_CANVAS    lv_color_make(0x0B, 0x0C, 0x0E)
#define COLOR_BORDER    lv_color_make(0x24, 0x27, 0x2C)
#define COLOR_PRIMARY   lv_color_make(0xEC, 0xED, 0xEF)
#define COLOR_SECONDARY lv_color_make(0x8A, 0x8F, 0x98)
#define COLOR_FAINT     lv_color_make(0x56, 0x5B, 0x63)
#define COLOR_BLUE      lv_color_make(0x5C, 0xA8, 0xFF)
#define COLOR_PURPLE    lv_color_make(0xB1, 0x8C, 0xFF)
#define COLOR_GREEN     lv_color_make(0x3D, 0xDC, 0x84)
#define COLOR_AMBER     lv_color_make(0xFF, 0xB2, 0x24)
#define COLOR_RED       lv_color_make(0xFF, 0x5C, 0x5C)
#define COLOR_TRACK     COLOR_BORDER

typedef struct {
    char date[20];
    char clock[12];
    char weekday[12];
    char city[20];
    char temperature[12];
    char condition[24];
    char status[20];
    char elapsed[16];
    char detail[28];
    char primary_pct[8];
    char secondary_pct[8];
    char primary_label[12];
    char secondary_label[12];
    char primary_reset[20];
    char secondary_reset[16];
} codex_dashboard_state_t;

typedef struct {
    lv_obj_t *clock;
    lv_obj_t *date;
    lv_obj_t *weekday;
    lv_obj_t *city;
    lv_obj_t *temperature;
    lv_obj_t *weather_dot;
    lv_obj_t *condition;
    lv_obj_t *status;
    lv_obj_t *elapsed;
    lv_obj_t *detail;
    lv_obj_t *status_dot;
    lv_obj_t *primary;
    lv_obj_t *secondary;
    lv_obj_t *primary_reset;
    lv_obj_t *secondary_reset;
    lv_obj_t *usage_bar;
    lv_obj_t *left_rule;
    lv_obj_t *middle_rule;
    lv_obj_t *usage_unavailable;
    lv_obj_t *usage_retrying;
} codex_dashboard_ui_t;

static codex_dashboard_state_t g_state = {
    .date = "--",
    .clock = "--:--",
    .weekday = "",
    .city = "Shenzhen",
    .temperature = "--",
    .condition = "Waiting",
    .status = "CONNECTING",
    .elapsed = "",
    .detail = "Codex companion",
    .primary_pct = "--",
    .secondary_pct = "",
    .primary_label = "1 WEEK",
    .secondary_label = "",
    .primary_reset = "",
    .secondary_reset = "",
};

static codex_dashboard_ui_t g_ui;
static rt_mutex_t g_lock;
static lv_obj_t *g_screen;

/* Claude's source specification uses Inter for labels and state text, then
 * JetBrains Mono for every changing metric.  LVGL v9's TinyTTF path renders
 * the real TTF assets without the incompatible legacy glyph-table converter. */
extern const unsigned char codex_inter_regular[];
extern const int codex_inter_regular_size;
extern const unsigned char codex_inter_semibold[];
extern const int codex_inter_semibold_size;
extern const unsigned char codex_inter_bold[];
extern const int codex_inter_bold_size;
extern const unsigned char codex_jbm_medium[];
extern const int codex_jbm_medium_size;
extern const unsigned char codex_jbm_semibold[];
extern const int codex_jbm_semibold_size;

static lv_font_t *g_font_label;
static lv_font_t *g_font_cap;
static lv_font_t *g_font_body;
static lv_font_t *g_font_metric;
static lv_font_t *g_font_status;
static lv_font_t *g_font_percentage;
static lv_font_t *g_font_display;

#define FONT_CAP     (g_font_cap     ? g_font_cap     : &lv_font_montserrat_12)
#define FONT_LABEL   (g_font_label   ? g_font_label   : &lv_font_montserrat_12)
#define FONT_BODY    (g_font_body    ? g_font_body    : &lv_font_montserrat_12)
#define FONT_METRIC  (g_font_metric  ? g_font_metric  : &lv_font_montserrat_16)
#define FONT_STATUS  (g_font_status  ? g_font_status  : &lv_font_montserrat_20)
#define FONT_PERCENT (g_font_percentage ? g_font_percentage : &lv_font_montserrat_28)
#define FONT_DISPLAY (g_font_display ? g_font_display : &lv_font_montserrat_36)

static void dashboard_fonts_destroy(void)
{
    if (g_font_display) lv_tiny_ttf_destroy(g_font_display);
    if (g_font_percentage) lv_tiny_ttf_destroy(g_font_percentage);
    if (g_font_status)  lv_tiny_ttf_destroy(g_font_status);
    if (g_font_metric)  lv_tiny_ttf_destroy(g_font_metric);
    if (g_font_body)    lv_tiny_ttf_destroy(g_font_body);
    if (g_font_label)   lv_tiny_ttf_destroy(g_font_label);
    if (g_font_cap)     lv_tiny_ttf_destroy(g_font_cap);
    g_font_cap = RT_NULL;
    g_font_label = RT_NULL;
    g_font_body = RT_NULL;
    g_font_metric = RT_NULL;
    g_font_status = RT_NULL;
    g_font_percentage = RT_NULL;
    g_font_display = RT_NULL;
}

static void dashboard_fonts_create(void)
{
    g_font_cap = lv_tiny_ttf_create_data(codex_inter_semibold,
                                         codex_inter_semibold_size, 9);
    g_font_label = lv_tiny_ttf_create_data(codex_inter_semibold,
                                           codex_inter_semibold_size, 10);
    g_font_body = lv_tiny_ttf_create_data(codex_inter_regular,
                                          codex_inter_regular_size, 10);
    g_font_metric = lv_tiny_ttf_create_data(codex_jbm_medium,
                                            codex_jbm_medium_size, 15);
    g_font_status = lv_tiny_ttf_create_data(codex_inter_bold,
                                            codex_inter_bold_size, 17);
    g_font_percentage = lv_tiny_ttf_create_data(codex_jbm_semibold,
                                                codex_jbm_semibold_size, 38);
    g_font_display = lv_tiny_ttf_create_data(codex_jbm_medium,
                                             codex_jbm_medium_size, 36);

    /* TinyTTF allocation can fail under memory pressure.  The dashboard still
     * remains usable with the known-good built-in fallback in that case. */
    if (!g_font_cap || !g_font_label || !g_font_body || !g_font_metric ||
        !g_font_status || !g_font_percentage || !g_font_display) {
        dashboard_fonts_destroy();
    }
}

static void copy_text(char *dst, size_t dst_len, const char *src)
{
    if (!dst || !dst_len) return;
    rt_strncpy(dst, src ? src : "", dst_len - 1);
    dst[dst_len - 1] = '\0';
}

static const char *next_part(const char *src, char *dst, size_t dst_len)
{
    const char *end;
    size_t len;
    if (!src) {
        copy_text(dst, dst_len, "");
        return RT_NULL;
    }
    end = strchr(src, '|');
    len = end ? (size_t)(end - src) : strlen(src);
    if (dst_len) {
        if (len >= dst_len) len = dst_len - 1;
        rt_memcpy(dst, src, len);
        dst[len] = '\0';
    }
    return end ? end + 1 : RT_NULL;
}

/* "MON . JUL 21" (ASCII CDC) -> "MON <bullet> JUL 21" as in the design. */
static void format_date(const char *src, char *dst, size_t dst_len)
{
    size_t di = 0;
    const char *p = src ? src : "";
    while (*p && di + 6 < dst_len) {
        if (p[0] == ' ' && p[1] == '.' && p[2] == ' ') {
            dst[di++] = ' ';
            dst[di++] = '\xE2'; /* U+2022 bullet, included in the */
            dst[di++] = '\x80'; /* built-in Montserrat fonts.     */
            dst[di++] = '\xA2';
            dst[di++] = ' ';
            p += 3;
        } else {
            dst[di++] = *p++;
        }
    }
    dst[di] = '\0';
}

/* "27.3C" (ASCII CDC) -> "27.3(degree)C" as in the design. */
static void format_temp(const char *src, char *dst, size_t dst_len)
{
    size_t len = src ? strlen(src) : 0;
    if (len >= 2 && (src[len - 1] == 'C' || src[len - 1] == 'F') &&
        src[len - 2] >= '0' && src[len - 2] <= '9' && len + 3 < dst_len) {
        rt_memcpy(dst, src, len - 1);
        dst[len - 1] = '\xC2'; /* U+00B0 degree sign */
        dst[len]     = '\xB0';
        dst[len + 1] = src[len - 1];
        dst[len + 2] = '\0';
    } else {
        copy_text(dst, dst_len, src);
    }
}

static lv_obj_t *make_panel(lv_obj_t *screen, int x)
{
    lv_obj_t *panel = lv_obj_create(screen);
    lv_obj_remove_style_all(panel);
    lv_obj_set_size(panel, PANEL_W, PANEL_H);
    lv_obj_set_pos(panel, x, 0);
    lv_obj_set_style_bg_color(panel, COLOR_CANVAS, 0);
    lv_obj_set_style_bg_opa(panel, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(panel, 0, 0);
    return panel;
}

static lv_obj_t *make_label(lv_obj_t *parent, const char *text,
                            int x, int y, int width, int height,
                            lv_color_t color, const lv_font_t *font)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_long_mode(label, LV_LABEL_LONG_CLIP);
    lv_label_set_text(label, text);
    lv_obj_set_pos(label, x, y);
    lv_obj_set_size(label, width, height);
    lv_obj_set_style_text_color(label, color, 0);
    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_pad_all(label, 0, 0);
    return label;
}

static lv_obj_t *make_rule(lv_obj_t *parent)
{
    lv_obj_t *rule = lv_obj_create(parent);
    lv_obj_remove_style_all(rule);
    lv_obj_set_size(rule, 100, 1);
    lv_obj_set_pos(rule, 14, 98);
    lv_obj_set_style_bg_color(rule, COLOR_BORDER, 0);
    lv_obj_set_style_bg_opa(rule, LV_OPA_COVER, 0);
    return rule;
}

/* Claude Design StatusScreen.dc.html: state text scales with its length so
 * it always stays on one clean line (never wrapped, never clipped). */
static void apply_status_style(const char *status)
{
    lv_color_t color = COLOR_AMBER;
    const lv_font_t *font;
    size_t len = strlen(status);

    if (strcmp(status, "RUNNING") == 0) color = COLOR_GREEN;
    else if (strstr(status, "INPUT") || strstr(status, "ALLOW") ||
             strcmp(status, "WAITING") == 0) color = COLOR_AMBER;
    else if (strstr(status, "FAILED") || strstr(status, "DENY")) color = COLOR_RED;
    else if (strcmp(status, "IDLE") == 0 || strcmp(status, "COMPLETED") == 0) color = COLOR_BLUE;
    else if (strcmp(status, "DISCONNECTED") == 0) color = COLOR_SECONDARY;

    /* The source design intentionally does not reflow status changes: common
     * states sit at y=46; two-word/long states use a 14 px face at y=48. */
    if (len <= 7) {
        font = FONT_STATUS;
        lv_obj_set_y(g_ui.status, 46);
        lv_obj_set_style_text_letter_space(g_ui.status, 1, 0);
    } else {
        font = FONT_METRIC;
        lv_obj_set_y(g_ui.status, 48);
        lv_obj_set_style_text_letter_space(g_ui.status, 1, 0);
    }
    lv_obj_set_y(g_ui.elapsed, 74);

    lv_obj_set_style_text_font(g_ui.status, font, 0);
    lv_obj_set_style_text_color(g_ui.status, color, 0);
    lv_obj_set_style_bg_color(g_ui.status_dot, color, 0);
    lv_obj_set_style_shadow_color(g_ui.status_dot, color, 0);
}

static void refresh_timer(lv_timer_t *timer)
{
    codex_dashboard_state_t state;
    char text[32];
    bool usage_available;
    int x, total;
    (void)timer;
    if (!g_lock) return;
    rt_mutex_take(g_lock, RT_WAITING_FOREVER);
    state = g_state;
    rt_mutex_release(g_lock);

    format_date(state.date, text, sizeof(text));
    lv_label_set_text(g_ui.date, text);
    lv_label_set_text(g_ui.clock, state.clock);
    lv_label_set_text(g_ui.weekday, state.weekday);
    lv_label_set_text(g_ui.city, state.city);
    format_temp(state.temperature, text, sizeof(text));
    lv_label_set_text(g_ui.temperature, text);
    lv_label_set_text(g_ui.condition, state.condition);
    lv_label_set_text(g_ui.status, state.status);
    lv_label_set_text(g_ui.elapsed, state.elapsed);
    lv_label_set_text(g_ui.detail, state.detail);

    /* The weather row is a centred three-piece group, exactly as in the HTML
     * source: white mono temperature, faint bullet, muted condition. */
    lv_obj_update_layout(g_ui.temperature);
    lv_obj_update_layout(g_ui.weather_dot);
    lv_obj_update_layout(g_ui.condition);
    total = lv_obj_get_width(g_ui.temperature) + lv_obj_get_width(g_ui.weather_dot) +
            lv_obj_get_width(g_ui.condition);
    x = (PANEL_W - total) / 2;
    lv_obj_set_x(g_ui.temperature, x);
    x += lv_obj_get_width(g_ui.temperature);
    lv_obj_set_x(g_ui.weather_dot, x);
    x += lv_obj_get_width(g_ui.weather_dot);
    lv_obj_set_x(g_ui.condition, x);

    usage_available = state.primary_pct[0] >= '0' && state.primary_pct[0] <= '9';
    if (usage_available) {
        lv_obj_remove_flag(g_ui.primary, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(g_ui.secondary, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(g_ui.usage_bar, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(g_ui.primary_reset, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(g_ui.usage_unavailable, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(g_ui.usage_retrying, LV_OBJ_FLAG_HIDDEN);

        lv_label_set_text(g_ui.primary, state.primary_label[0] ? state.primary_label : "1 WEEK");
        lv_label_set_text_fmt(g_ui.secondary, "%s%%", state.primary_pct);
        lv_label_set_text(g_ui.primary_reset, state.primary_reset);
        lv_bar_set_value(g_ui.usage_bar, atoi(state.primary_pct), LV_ANIM_OFF);

        /* Source UI centres the window and hero independently; they never
         * form a two-column row. */
        lv_obj_set_style_text_font(g_ui.secondary, FONT_PERCENT, 0);
        lv_obj_set_y(g_ui.secondary, 48);
    } else {
        lv_obj_add_flag(g_ui.primary, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(g_ui.secondary, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(g_ui.usage_bar, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(g_ui.primary_reset, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(g_ui.usage_unavailable, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(g_ui.usage_retrying, LV_OBJ_FLAG_HIDDEN);
    }
    lv_obj_add_flag(g_ui.secondary_reset, LV_OBJ_FLAG_HIDDEN);
    apply_status_style(state.status);
}

int codex_dashboard_init(void)
{
    lv_obj_t *left;
    lv_obj_t *middle;
    lv_obj_t *right;
    lv_obj_t *codex_title;
    lv_obj_t *usage_title;

    if (g_screen) return 0;
    g_lock = rt_mutex_create("codex_ui", RT_IPC_FLAG_PRIO);
    if (!g_lock) return -RT_ERROR;
    dashboard_fonts_create();

    g_screen = lv_obj_create(NULL);
    lv_obj_remove_style_all(g_screen);
    lv_obj_set_size(g_screen, PANEL_W * 3, PANEL_H);
    lv_obj_set_style_bg_color(g_screen, COLOR_CANVAS, 0);
    lv_obj_set_style_bg_opa(g_screen, LV_OPA_COVER, 0);
    left = make_panel(g_screen, 0);
    middle = make_panel(g_screen, PANEL_W);
    right = make_panel(g_screen, PANEL_W * 2);

    /* ===== LEFT — location / time / weather (TriScreen.dc.html) ===== */
    g_ui.city = make_label(left, "Shenzhen", 0, 12, PANEL_W, 12,
                           COLOR_BLUE, FONT_LABEL);
    lv_obj_set_style_text_letter_space(g_ui.city, 1, 0);
    g_ui.clock = make_label(left, "--:--", 0, 36, PANEL_W, 38,
                            COLOR_PRIMARY, FONT_DISPLAY);
    lv_obj_set_style_text_letter_space(g_ui.clock, -1, 0);
    g_ui.date = make_label(left, "--", 0, 78, PANEL_W, 11,
                           COLOR_SECONDARY, FONT_CAP);
    lv_obj_set_style_text_letter_space(g_ui.date, 1, 0);
    g_ui.weekday = make_label(left, "", 0, 0, 1, 1, COLOR_CANVAS, FONT_LABEL);
    lv_obj_add_flag(g_ui.weekday, LV_OBJ_FLAG_HIDDEN);
    g_ui.left_rule = make_rule(left);
    g_ui.temperature = make_label(left, "--", 0, 106, LV_SIZE_CONTENT, 14,
                                  COLOR_PRIMARY, FONT_BODY);
    g_ui.weather_dot = make_label(left, " · ", 0, 106, LV_SIZE_CONTENT, 14,
                                  COLOR_FAINT, FONT_BODY);
    g_ui.condition = make_label(left, "Waiting", 0, 107, LV_SIZE_CONTENT, 12,
                                COLOR_SECONDARY, FONT_BODY);

    /* ===== MIDDLE — Codex status (StatusScreen.dc.html) ===== */
    codex_title = make_label(middle, "CODEX", 0, 12, PANEL_W, 11,
                             COLOR_SECONDARY, FONT_CAP);
    lv_obj_set_style_text_letter_space(codex_title, 1, 0);
    g_ui.status_dot = lv_obj_create(middle);
    lv_obj_remove_style_all(g_ui.status_dot);
    lv_obj_set_size(g_ui.status_dot, 5, 5);
    lv_obj_set_pos(g_ui.status_dot, 38, 15);
    lv_obj_set_style_bg_color(g_ui.status_dot, COLOR_GREEN, 0);
    lv_obj_set_style_bg_opa(g_ui.status_dot, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(g_ui.status_dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_shadow_width(g_ui.status_dot, 0, 0);
    lv_obj_set_style_shadow_opa(g_ui.status_dot, LV_OPA_TRANSP, 0);
    lv_obj_set_style_shadow_color(g_ui.status_dot, COLOR_GREEN, 0);
    g_ui.status = make_label(middle, "CONNECTING", 0, 46, PANEL_W, 20,
                             COLOR_AMBER, FONT_STATUS);
    g_ui.elapsed = make_label(middle, "", 0, 74, PANEL_W, 16,
                              COLOR_PRIMARY, FONT_METRIC);
    g_ui.middle_rule = make_rule(middle);
    g_ui.detail = make_label(middle, "Codex companion", 0, 106, PANEL_W, 12,
                             COLOR_SECONDARY, FONT_BODY);

    /* ===== RIGHT — remaining usage (TriScreen.dc.html) ===== */
    usage_title = make_label(right, "REMAINING", 0, 12, PANEL_W, 12,
                             COLOR_PURPLE, FONT_LABEL);
    lv_obj_set_style_text_letter_space(usage_title, 1, 0);
    g_ui.primary = make_label(right, "1 WEEK", 0, 34, PANEL_W, 11,
                              COLOR_SECONDARY, FONT_CAP);
    lv_obj_set_style_text_letter_space(g_ui.primary, 1, 0);
    g_ui.secondary = make_label(right, "--%", 0, 48, PANEL_W, 40,
                                COLOR_PRIMARY, FONT_PERCENT);
    g_ui.usage_bar = lv_bar_create(right);
    lv_obj_set_size(g_ui.usage_bar, 100, 3);
    lv_obj_set_pos(g_ui.usage_bar, 14, 96);
    lv_bar_set_range(g_ui.usage_bar, 0, 100);
    lv_obj_set_style_pad_all(g_ui.usage_bar, 0, LV_PART_MAIN);
    lv_obj_set_style_bg_color(g_ui.usage_bar, COLOR_TRACK, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(g_ui.usage_bar, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(g_ui.usage_bar, COLOR_PURPLE, LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(g_ui.usage_bar, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_radius(g_ui.usage_bar, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_radius(g_ui.usage_bar, LV_RADIUS_CIRCLE, LV_PART_INDICATOR);
    g_ui.primary_reset = make_label(right, "", 0, 108, PANEL_W, 11,
                                    COLOR_SECONDARY, FONT_CAP);
    lv_obj_set_style_text_letter_space(g_ui.primary_reset, 1, 0);
    g_ui.usage_unavailable = make_label(right, "USAGE", 0, 50, PANEL_W, 14,
                                        COLOR_SECONDARY, FONT_METRIC);
    g_ui.usage_retrying = make_label(right, "UNAVAILABLE", 0, 66, PANEL_W, 14,
                                     COLOR_SECONDARY, FONT_METRIC);
    g_ui.secondary_reset = make_label(right, "", 0, 0, 1, 1, COLOR_CANVAS, FONT_LABEL);
    lv_obj_add_flag(g_ui.secondary_reset, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(g_ui.usage_unavailable, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(g_ui.usage_retrying, LV_OBJ_FLAG_HIDDEN);

    lv_scr_load(g_screen);
    lv_timer_create(refresh_timer, 250, RT_NULL);
    refresh_timer(RT_NULL);
    return 0;
}

void codex_dashboard_deinit(void)
{
    if (g_screen) lv_obj_del(g_screen);
    g_screen = RT_NULL;
    dashboard_fonts_destroy();
    if (g_lock) rt_mutex_delete(g_lock);
    g_lock = RT_NULL;
}

bool codex_dashboard_handle_command(const char *key, const char *value)
{
    const char *next;
    if (!g_lock || !key || strncmp(key, "codex_", 6) != 0) return false;
    rt_mutex_take(g_lock, RT_WAITING_FOREVER);
    if (strcmp(key, "codex_time") == 0) {
        next = next_part(value, g_state.date, sizeof(g_state.date));
        next = next_part(next, g_state.clock, sizeof(g_state.clock));
        next_part(next, g_state.weekday, sizeof(g_state.weekday));
    } else if (strcmp(key, "codex_weather") == 0) {
        next = next_part(value, g_state.city, sizeof(g_state.city));
        next = next_part(next, g_state.temperature, sizeof(g_state.temperature));
        next_part(next, g_state.condition, sizeof(g_state.condition));
    } else if (strcmp(key, "codex_status") == 0) {
        next = next_part(value, g_state.status, sizeof(g_state.status));
        next = next_part(next, g_state.elapsed, sizeof(g_state.elapsed));
        next_part(next, g_state.detail, sizeof(g_state.detail));
    } else if (strcmp(key, "codex_usage") == 0) {
        next = next_part(value, g_state.primary_pct, sizeof(g_state.primary_pct));
        next = next_part(next, g_state.secondary_pct, sizeof(g_state.secondary_pct));
        next = next_part(next, g_state.primary_label, sizeof(g_state.primary_label));
        next = next_part(next, g_state.secondary_label, sizeof(g_state.secondary_label));
        next = next_part(next, g_state.primary_reset, sizeof(g_state.primary_reset));
        next_part(next, g_state.secondary_reset, sizeof(g_state.secondary_reset));
    } else {
        rt_mutex_release(g_lock);
        return false;
    }
    rt_mutex_release(g_lock);
    return true;
}
