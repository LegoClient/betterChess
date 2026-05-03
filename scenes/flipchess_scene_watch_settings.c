#include "../flipchess.h"
#include <lib/toolbox/value_index.h>

#define TEXT_LABEL_ON  "ON"
#define TEXT_LABEL_OFF "OFF"

const char* const watch_ai_text[3] = {
    "CPU 1",
    "CPU 2",
    "CPU 3",
};
const uint32_t watch_ai_value[3] = {1, 2, 3};

// Speed = how long the "thinking..." pause lasts before each AI move.
// Values are in 100 ms ticks, matched to the view-dispatcher tick rate.
const char* const watch_speed_text[4] = {
    "Fast",
    "Normal",
    "Slow",
    "V.Slow",
};
const uint32_t watch_speed_value[4] = {0, 1, 2, 3};

const char* const watch_restart_text[2] = {
    TEXT_LABEL_OFF,
    TEXT_LABEL_ON,
};
const uint32_t watch_restart_value[2] = {0, 1};

static void flipchess_scene_watch_settings_set_ai_level(VariableItem* item) {
    FlipChess* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, watch_ai_text[index]);
    app->watch_ai_level = (uint8_t)watch_ai_value[index];
}

static void flipchess_scene_watch_settings_set_speed(VariableItem* item) {
    FlipChess* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, watch_speed_text[index]);
    app->watch_speed = (uint8_t)watch_speed_value[index];
}

static void flipchess_scene_watch_settings_set_restart(VariableItem* item) {
    FlipChess* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);
    variable_item_set_current_value_text(item, watch_restart_text[index]);
    app->watch_autorestart = (uint8_t)watch_restart_value[index];
}

void flipchess_scene_watch_settings_on_enter(void* context) {
    FlipChess* app = context;
    VariableItem* item;
    uint8_t value_index;

    // AI Level
    item = variable_item_list_add(
        app->variable_item_list,
        "AI Level:",
        3,
        flipchess_scene_watch_settings_set_ai_level,
        app);
    value_index = value_index_uint32(app->watch_ai_level, watch_ai_value, 3);
    variable_item_set_current_value_index(item, value_index);
    variable_item_set_current_value_text(item, watch_ai_text[value_index]);

    // Match Speed
    item = variable_item_list_add(
        app->variable_item_list,
        "Speed:",
        4,
        flipchess_scene_watch_settings_set_speed,
        app);
    value_index = value_index_uint32(app->watch_speed, watch_speed_value, 4);
    variable_item_set_current_value_index(item, value_index);
    variable_item_set_current_value_text(item, watch_speed_text[value_index]);

    // Auto-restart
    item = variable_item_list_add(
        app->variable_item_list,
        "Auto-restart:",
        2,
        flipchess_scene_watch_settings_set_restart,
        app);
    value_index = value_index_uint32(app->watch_autorestart, watch_restart_value, 2);
    variable_item_set_current_value_index(item, value_index);
    variable_item_set_current_value_text(item, watch_restart_text[value_index]);

    view_dispatcher_switch_to_view(app->view_dispatcher, FlipChessViewIdSettings);
}

bool flipchess_scene_watch_settings_on_event(void* context, SceneManagerEvent event) {
    FlipChess* app = context;
    UNUSED(app);
    bool consumed = false;
    if(event.type == SceneManagerEventTypeCustom) {
    }
    return consumed;
}

void flipchess_scene_watch_settings_on_exit(void* context) {
    FlipChess* app = context;
    variable_item_list_set_selected_item(app->variable_item_list, 0);
    variable_item_list_reset(app->variable_item_list);
}
