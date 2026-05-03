#pragma once

#include <gui/view.h>
#include "../helpers/flipchess_custom_event.h"

typedef struct FlipChessScene1 FlipChessScene1;

typedef void (*FlipChessScene1Callback)(FlipChessCustomEvent event, void* context);

void flipchess_scene_1_set_callback(
    FlipChessScene1* flipchess_scene_1,
    FlipChessScene1Callback callback,
    void* context);

View* flipchess_scene_1_get_view(FlipChessScene1* flipchess_static);

FlipChessScene1* flipchess_scene_1_alloc();

void flipchess_scene_1_free(FlipChessScene1* flipchess_static);

// Called by the scene tick handler to auto-advance AI vs AI games.
// app_context is a FlipChess* cast to void* to avoid a circular include.
void flipchess_scene_1_tick(FlipChessScene1* instance, void* app_context);

// If the player has clicked a piece and is mid-move (selection
// in progress), cancel that selection and return true. Otherwise
// returns false. Used by the scene's Back handler so a Back press
// during selection cancels rather than exits the scene.
bool flipchess_scene_1_try_cancel_selection(FlipChessScene1* instance);
