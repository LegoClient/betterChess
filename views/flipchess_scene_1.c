#include "../flipchess.h"
#include <furi.h>
// #include <furi_hal.h>
// #include <furi_hal_random.h>
#include <input/input.h>
#include <gui/elements.h>
//#include <dolphin/dolphin.h>
#include <string.h>
#include <stdio.h>
//#include "flipchess_icons.h"
#include "../helpers/flipchess_voice.h"
#include "../helpers/flipchess_haptic.h"

#define SCL_960_CASTLING        0 // setting to 1 compiles a 960 version of smolchess
#define XBOARD_DEBUG            0 // will create files with xboard communication
#define SCL_EVALUATION_FUNCTION SCL_boardEvaluateStatic
#define SCL_DEBUG_AI            0

#include "../chess/smallchesslib.h"

#define ENABLE_960       0 // setting to 1 enables 960 chess
#define MAX_TEXT_LEN     15 // 15 = max length of text
#define MAX_TEXT_BUF     (MAX_TEXT_LEN + 1) // max length of text + null terminator
#define THREAD_WAIT_TIME 20 // time to wait for draw thread to finish

struct FlipChessScene1 {
    View* view;
    FlipChessScene1Callback callback;
    void* context;
};
typedef struct {
    uint8_t paramPlayerW;
    uint8_t paramPlayerB;

    uint8_t paramAnalyze; // depth of analysis
    uint8_t paramMoves;
    uint8_t paramInfo;
    uint8_t paramFlipBoard;
    uint8_t paramExit;
    uint16_t paramStep;
    char* paramFEN;
    char* paramPGN;

    int clockSeconds;
    SCL_Game game;
    SCL_Board startState;

#if ENABLE_960
    int16_t random960PosNumber;
#endif

    //uint8_t picture[SCL_BOARD_PICTURE_WIDTH * SCL_BOARD_PICTURE_WIDTH];
    uint8_t squareSelected;
    uint8_t squareSelectedLast;

    char* msg;
    char* msg2;
    char* msg3;
    char moveString[MAX_TEXT_BUF];
    char moveString2[MAX_TEXT_BUF];
    char moveString3[MAX_TEXT_BUF];
    uint8_t thinking;

    SCL_SquareSet moveHighlight;
    uint8_t squareFrom;
    uint8_t squareTo;
    uint8_t turnState;

    // Cached static evaluation of the current position (units: SCL_VALUE_PAWN).
    int16_t cachedEval;
    // Ticks elapsed since game-over while in watch mode; used to auto-restart.
    uint16_t autoRestartTicks;
    // While > 0, the watch-mode tick will keep showing "thinking..." and
    // hold off the AI computation. Counts down one per 100 ms tick.
    uint8_t thinkingTicksRemaining;
    // When set, flipchess_makeAIMove caps the capture/endgame extensions
    // so the synchronous search can't hog the GUI thread for tens of
    // seconds. Used in watch (spectator) mode where Back must stay
    // responsive without modifying smallchesslib for cancellation.
    uint8_t watchCappedSearch;

} FlipChessScene1Model;

static uint8_t picture[SCL_BOARD_PICTURE_WIDTH * SCL_BOARD_PICTURE_WIDTH];

void flipchess_putImagePixel(uint8_t pixel, uint16_t index) {
    picture[index] = pixel;
}

// Fill two captured-pieces strips, one per side:
//   w_out gets "W:<black pieces missing>" (white's captures, lowercase)
//   b_out gets "B:<white pieces missing>" (black's captures, uppercase)
// Pieces are printed in descending value: Q, R, B, N, P. Kings are skipped
// because the king is never captured (game ends in mate first).
void flipchess_format_captured(
    SCL_Board board,
    char* w_out,
    uint8_t w_size,
    char* b_out,
    uint8_t b_size) {
    // Index order: Q, R, B, N, P. Initial counts per side.
    const uint8_t initial[5] = {1, 2, 2, 2, 8};
    const char white_letters[5] = {'Q', 'R', 'B', 'N', 'P'};
    const char black_letters[5] = {'q', 'r', 'b', 'n', 'p'};

    uint8_t w_on_board[5] = {0, 0, 0, 0, 0};
    uint8_t b_on_board[5] = {0, 0, 0, 0, 0};

    for(int i = 0; i < 64; i++) {
        char p = board[i];
        switch(p) {
        case 'Q': w_on_board[0]++; break;
        case 'R': w_on_board[1]++; break;
        case 'B': w_on_board[2]++; break;
        case 'N': w_on_board[3]++; break;
        case 'P': w_on_board[4]++; break;
        case 'q': b_on_board[0]++; break;
        case 'r': b_on_board[1]++; break;
        case 'b': b_on_board[2]++; break;
        case 'n': b_on_board[3]++; break;
        case 'p': b_on_board[4]++; break;
        default: break;
        }
    }

    // White's captures = black pieces missing
    uint8_t pos = 0;
    if(pos + 2 < w_size) {
        w_out[pos++] = 'W';
        w_out[pos++] = ':';
    }
    for(int i = 0; i < 5; i++) {
        int missing = (int)initial[i] - (int)b_on_board[i];
        for(int j = 0; j < missing && pos + 1 < w_size; j++) {
            w_out[pos++] = black_letters[i];
        }
    }
    w_out[pos < w_size ? pos : w_size - 1] = '\0';

    // Black's captures = white pieces missing
    pos = 0;
    if(pos + 2 < b_size) {
        b_out[pos++] = 'B';
        b_out[pos++] = ':';
    }
    for(int i = 0; i < 5; i++) {
        int missing = (int)initial[i] - (int)w_on_board[i];
        for(int j = 0; j < missing && pos + 1 < b_size; j++) {
            b_out[pos++] = white_letters[i];
        }
    }
    b_out[pos < b_size ? pos : b_size - 1] = '\0';
}

uint8_t flipchess_stringsEqual(const char* s1, const char* s2, int max) {
    for(int i = 0; i < max; ++i) {
        if(*s1 != *s2) return 0;

        if(*s1 == 0) return 1;

        s1++;
        s2++;
    }

    return 1;
}

int16_t flipchess_makeAIMove(
    SCL_Board board,
    uint8_t* s0,
    uint8_t* s1,
    char* prom,
    FlipChessScene1Model* model) {
    uint8_t level = SCL_boardWhitesTurn(board) ? model->paramPlayerW : model->paramPlayerB;
    uint8_t depth = (level > 0) ? level : 1;
    uint8_t extraDepth = 3;
    uint8_t endgameDepth = 1;
    uint8_t randomness =
        model->game.ply < 2 ? 1 : 0; /* in first moves increase randomness for different 
                             openings */
    uint8_t rs0, rs1;

    SCL_gameGetRepetiotionMove(&(model->game), &rs0, &rs1);

    if(model->clockSeconds >= 0) // when using clock, choose AI params accordingly
    {
        if(model->clockSeconds <= 5) {
            depth = 1;
            extraDepth = 2;
            endgameDepth = 0;
        } else if(model->clockSeconds < 15) {
            depth = 2;
            extraDepth = 2;
        } else if(model->clockSeconds < 100) {
            depth = 2;
        } else if(model->clockSeconds < 5 * 60) {
            depth = 3;
        } else {
            depth = 3;
            extraDepth = 4;
        }
    }

    if(model->watchCappedSearch) {
        // Watch mode runs the search synchronously on the GUI thread, so
        // every move blocks input for the duration of SCL_getAIMove. Cap
        // the capture/endgame extensions so even CPU3 finishes in a few
        // seconds at worst -- otherwise Back stays unresponsive long
        // enough for qFlipper to time out RPC input events.
        if(extraDepth > 2) extraDepth = 2;
        endgameDepth = 0;
    }

    return SCL_getAIMove(
        board,
        depth,
        extraDepth,
        endgameDepth,
        SCL_boardEvaluateStatic,
        SCL_randomBetter,
        randomness,
        rs0,
        rs1,
        s0,
        s1,
        prom);
}

bool flipchess_isPlayerTurn(FlipChessScene1Model* model) {
    return (SCL_boardWhitesTurn(model->game.board) && model->paramPlayerW == 0) ||
           (!SCL_boardWhitesTurn(model->game.board) && model->paramPlayerB == 0);
}

void flipchess_shiftMessages(FlipChessScene1Model* model) {
    // shift messages
    model->msg3 = model->msg2;
    model->msg2 = model->msg;
    strncpy(model->moveString3, model->moveString2, MAX_TEXT_LEN);
    strncpy(model->moveString2, model->moveString, MAX_TEXT_LEN);
}

void flipchess_drawBoard(FlipChessScene1Model* model) {
    // draw chess board
    SCL_drawBoard(
        model->game.board,
        flipchess_putImagePixel,
        model->squareSelected,
        model->moveHighlight,
        model->paramFlipBoard);
}

uint8_t flipchess_saveState(FlipChess* app, FlipChessScene1Model* model) {
    for(uint8_t i = 0; i < SCL_FEN_MAX_LENGTH; i++) {
        app->import_game_text[i] = '\0';
    }
    const uint8_t res = SCL_boardToFEN(model->game.board, app->import_game_text);
    if(res > 0) {
        app->import_game = 1;
    }
    return res;
}

uint8_t flipchess_turn(FlipChessScene1Model* model) {
    // 0: none, 1: player, 2: AI, 3: undo
    uint8_t moveType = FlipChessStatusNone;

    // if(model->paramInfo) {

    //     if(model->random960PosNumber >= 0)
    //         printf("960 random position number: %d\n", model->random960PosNumber);

    //     printf("ply number: %d\n", model->game.ply);

    //     int16_t eval = SCL_boardEvaluateStatic(model->game.board);
    //     printf(
    //         "board static evaluation: %lf (%d)\n",
    //         ((double)eval) / ((double)SCL_VALUE_PAWN),
    //         eval);
    //     printf("board hash: %u\n", SCL_boardHash32(model->game.board));
    //     printf("phase: ");

    //     switch(SCL_boardEstimatePhase(model->game.board)) {
    //     case SCL_PHASE_OPENING:
    //         puts("opening");
    //         break;
    //     case SCL_PHASE_ENDGAME:
    //         puts("endgame");
    //         break;
    //     default:
    //         puts("midgame");
    //         break;
    //     }

    //     printf(
    //         "en passant: %d\n",
    //         ((model->game.board[SCL_BOARD_ENPASSANT_CASTLE_BYTE] & 0x0f) + 1) % 16);
    //     printf(
    //         "50 move rule count: %d\n", model->game.board[SCL_BOARD_MOVE_COUNT_BYTE]);

    //     if(model->paramFEN == NULL && model->paramPGN == NULL) {
    //         printf("PGN: ");
    //         SCL_printPGN(model->game.record, putCharacter, startState);
    //         putchar('\n');
    //     }
    // }

    if(model->game.state != SCL_GAME_STATE_PLAYING) {
        model->paramExit = FlipChessStatusNone;

    } else {
        char movePromote = 'q';

        if(flipchess_isPlayerTurn(model)) {
            // if(stringsEqual(string, "undo", 5))
            //     moveType = FlipChessStatusMoveUndo;
            // else if(stringsEqual(string, "quit", 5))
            //     break;

            if(model->turnState == 0 && model->squareSelected != 255) {
                // Color check before allowing piece selection
                char piece = model->game.board[model->squareSelected];
                if(piece != '.' &&
                   SCL_pieceIsWhite(piece) == SCL_boardWhitesTurn(model->game.board)) {
                    model->squareFrom = model->squareSelected;
                    model->turnState = 1;
                }
            } else if(model->turnState == 1 && model->squareSelected != 255) {
                // Validate before executing
                if(SCL_boardMoveIsLegal(
                       model->game.board, model->squareFrom, model->squareSelected)) {
                    model->squareTo = model->squareSelected;
                    model->turnState = 2;
                    model->squareSelectedLast = model->squareSelected;
                } else {
                    // Invalid move, reset state
                    model->turnState = 0;
                    SCL_squareSetClear(model->moveHighlight);
                }
            }

            if(model->turnState == 1 && model->squareFrom != 255) {
                if((model->game.board[model->squareFrom] != '.') &&
                   (SCL_pieceIsWhite(model->game.board[model->squareFrom]) ==
                    SCL_boardWhitesTurn(model->game.board))) {
                    SCL_boardGetMoves(model->game.board, model->squareFrom, model->moveHighlight);
                }
            } else if(model->turnState == 2) {
                if(SCL_squareSetContains(model->moveHighlight, model->squareTo)) {
                    moveType = FlipChessStatusMovePlayer;
                }
                model->turnState = 0;
                SCL_squareSetClear(model->moveHighlight);
            }

        } else {
            model->squareSelected = 255;
            flipchess_makeAIMove(
                model->game.board, &(model->squareFrom), &(model->squareTo), &movePromote, model);
            moveType = FlipChessStatusMoveAI;
            model->turnState = 0;
        }

        if(moveType == FlipChessStatusMovePlayer || moveType == FlipChessStatusMoveAI) {
            flipchess_shiftMessages(model);

            SCL_moveToString(
                model->game.board,
                model->squareFrom,
                model->squareTo,
                movePromote,
                model->moveString);

            SCL_gameMakeMove(&(model->game), model->squareFrom, model->squareTo, movePromote);

            SCL_squareSetClear(model->moveHighlight);
            SCL_squareSetAdd(model->moveHighlight, model->squareFrom);
            SCL_squareSetAdd(model->moveHighlight, model->squareTo);

            // Cache the static evaluation so the side panel can show it.
            model->cachedEval = SCL_boardEvaluateStatic(model->game.board);
        } else if(moveType == FlipChessStatusMoveUndo) {
            flipchess_shiftMessages(model);

            if(model->paramPlayerW != 0 || model->paramPlayerB != 0)
                SCL_gameUndoMove(&(model->game));

            SCL_gameUndoMove(&(model->game));
            SCL_squareSetClear(model->moveHighlight);

            model->cachedEval = SCL_boardEvaluateStatic(model->game.board);
        }

        switch(model->game.state) {
        case SCL_GAME_STATE_WHITE_WIN:
            model->msg = "white wins";
            model->paramExit = FlipChessStatusReturn;
            break;

        case SCL_GAME_STATE_BLACK_WIN:
            model->msg = "black wins";
            model->paramExit = FlipChessStatusReturn;
            break;

        case SCL_GAME_STATE_DRAW_STALEMATE:
            model->msg = "stalemate";
            model->paramExit = FlipChessStatusReturn;
            break;

        case SCL_GAME_STATE_DRAW_REPETITION:
            model->msg = "draw-repetition";
            model->paramExit = FlipChessStatusReturn;
            break;

        case SCL_GAME_STATE_DRAW_DEAD:
            model->msg = "draw-dead pos.";
            model->paramExit = FlipChessStatusReturn;
            break;

        case SCL_GAME_STATE_DRAW:
            model->msg = "draw";
            model->paramExit = FlipChessStatusReturn;
            break;

        case SCL_GAME_STATE_DRAW_50:
            model->msg = "draw-50 moves";
            model->paramExit = FlipChessStatusReturn;
            break;

        default:
            if(model->game.ply > 0) {
                const uint8_t whitesTurn = SCL_boardWhitesTurn(model->game.board);

                if(SCL_boardCheck(model->game.board, whitesTurn)) {
                    model->msg = (whitesTurn ? "black: check!" : "white: check!");
                } else {
                    model->msg = (whitesTurn ? "black played" : "white played");
                }

                uint8_t s0, s1;
                char p;

                SCL_recordGetMove(model->game.record, model->game.ply - 1, &s0, &s1, &p);
                SCL_moveToString(model->game.board, s0, s1, p, model->moveString);
            }
            model->paramExit = moveType;
            break;
        }
    }

    model->thinking = 0;
    return model->paramExit;
}

void flipchess_scene_1_set_callback(
    FlipChessScene1* instance,
    FlipChessScene1Callback callback,
    void* context) {
    furi_assert(instance);
    furi_assert(callback);
    instance->callback = callback;
    instance->context = context;
}

void flipchess_scene_1_draw(Canvas* canvas, FlipChessScene1Model* model) {
    //UNUSED(model);
    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);

    //canvas_draw_icon(canvas, 0, 0, &I_FLIPR_128x64);

    // Frame
    canvas_draw_frame(canvas, 0, 0, 66, 64);

    // Side panel layout:
    //   y=10  current msg (or "thinking...")
    //   y=19  current move
    //   y=31  previous move (just the algebraic move; we drop the redundant
    //         "white played" / "black played" label since the current msg
    //         + alternation already implies whose move it was)
    //   y=43  evaluation, side-named: "White: 1.4", "Black: 0.5", or "Even"
    //   y=52  W:<captured> (black pieces white has taken)
    //   y=61  B:<captured> (white pieces black has taken)
    canvas_set_font(canvas, FontSecondary);
    if(model->thinking) {
        canvas_draw_str(canvas, 68, 10, "thinking...");
    } else {
        canvas_draw_str(canvas, 68, 10, model->msg);
    }
    canvas_draw_str(canvas, 68, 19, model->moveString);
    canvas_draw_str(canvas, 68, 31, model->moveString2);

    // Eval line. SCL_VALUE_PAWN is 256, so a raw eval can reach ~32k;
    // promote to int32 so the *10 doesn't overflow.
    char eval_buf[16];
    int32_t e10 = ((int32_t)model->cachedEval * 10) / (int32_t)SCL_VALUE_PAWN;
    if(e10 == 0) {
        strncpy(eval_buf, "Even", sizeof(eval_buf) - 1);
        eval_buf[sizeof(eval_buf) - 1] = '\0';
    } else {
        int32_t abs_e10 = e10 < 0 ? -e10 : e10;
        snprintf(
            eval_buf,
            sizeof(eval_buf),
            "%s: %ld.%ld",
            e10 > 0 ? "White" : "Black",
            (long)(abs_e10 / 10),
            (long)(abs_e10 % 10));
    }
    canvas_draw_str(canvas, 68, 43, eval_buf);

    // Captured-pieces strips, one line per side. Computed fresh from the
    // board each draw so undo / new game never desync the display.
    char w_captured[14];
    char b_captured[14];
    flipchess_format_captured(
        model->game.board, w_captured, sizeof(w_captured), b_captured, sizeof(b_captured));
    canvas_draw_str(canvas, 68, 52, w_captured);
    canvas_draw_str(canvas, 68, 61, b_captured);

    // Board
    for(uint16_t y = 0; y < SCL_BOARD_PICTURE_WIDTH; y++) {
        for(uint16_t x = 0; x < SCL_BOARD_PICTURE_WIDTH; x++) {
            if(!picture[x + (y * SCL_BOARD_PICTURE_WIDTH)]) {
                canvas_draw_dot(canvas, x + 1, y);
            }
        }
    }
}

static int flipchess_scene_1_model_init(
    FlipChessScene1Model* const model,
    const int white_mode,
    const int black_mode,
    char* import_game_text) {
    model->paramPlayerW = white_mode;
    model->paramPlayerB = black_mode;

    model->paramAnalyze = 255; // depth of analysis
    model->paramMoves = 0;
    model->paramInfo = 1;
    // Auto-flip the board when the human is playing Black against the AI,
    // so the player's pieces sit at the bottom of the screen as expected.
    model->paramFlipBoard = (black_mode == 0 && white_mode != 0) ? 1 : 0;
    model->paramExit = FlipChessStatusNone;
    model->paramStep = 0;
    model->paramFEN = import_game_text;
    model->paramPGN = NULL;
    model->clockSeconds = -1;

    model->cachedEval = 0;
    model->autoRestartTicks = 0;
    model->thinkingTicksRemaining = 0;
    // Default off; the watch-mode entry path explicitly turns this on
    // after init.
    model->watchCappedSearch = 0;

    SCL_Board emptyStartState = SCL_BOARD_START_STATE;
    memcpy(model->startState, &emptyStartState, sizeof(SCL_Board));

#if ENABLE_960
    model->random960PosNumber = -1;
#endif

    model->squareSelected = 255;
    model->squareSelectedLast = 28; // start selector near middle

    model->msg = "init";
    model->moveString[0] = '\0';
    model->msg2 = "";
    model->moveString2[0] = '\0';
    model->msg3 = "";
    model->moveString3[0] = '\0';
    model->thinking = 0;

    SCL_SquareSet emptySquareSet = SCL_SQUARE_SET_EMPTY;
    memcpy(model->moveHighlight, &emptySquareSet, sizeof(SCL_SquareSet));
    model->squareFrom = 255;
    model->squareTo = 255;
    model->turnState = 0;

    SCL_randomBetterSeed(furi_hal_random_get());

#if ENABLE_960
#if SCL_960_CASTLING
    if(model->random960PosNumber < 0) model->random960PosNumber = SCL_randomBetter();
#endif
    if(model->random960PosNumber >= 0) model->random960PosNumber %= 960;
#endif

    if(model->paramFEN != NULL)
        SCL_boardFromFEN(model->startState, model->paramFEN);
    else if(model->paramPGN != NULL) {
        SCL_Record record;
        SCL_recordFromPGN(record, model->paramPGN);
        SCL_boardInit(model->startState);
        SCL_recordApply(record, model->startState, model->paramStep);
    }

#if ENABLE_960
#if SCL_960_CASTLING
    else
        SCL_boardInit960(model->startState, model->random960PosNumber);
#endif
#endif

    SCL_gameInit(&(model->game), model->startState);

    if(model->paramAnalyze != 255) {
        char p;
        uint8_t move[] = {0, 0};

        model->paramPlayerW = model->paramAnalyze;
        model->paramPlayerB = model->paramAnalyze;

        int16_t evaluation =
            flipchess_makeAIMove(model->game.board, &(move[0]), &(move[1]), &p, model);

        if(model->paramAnalyze == 0) evaluation = SCL_boardEvaluateStatic(model->game.board);

        char moveStr[5];
        moveStr[4] = 0;

        SCL_squareToString(move[0], moveStr);
        SCL_squareToString(move[1], moveStr + 2);

        //printf("%lf (%d)\n", ((double)evaluation) / ((double)SCL_VALUE_PAWN), evaluation);
        //puts(moveStr);

        return evaluation;
    }

    if(model->paramMoves) {
        char string[256];

        for(int i = 0; i < 64; ++i)
            if(model->game.board[i] != '.' &&
               SCL_pieceIsWhite(model->game.board[i]) == SCL_boardWhitesTurn(model->game.board)) {
                SCL_SquareSet possibleMoves = SCL_SQUARE_SET_EMPTY;

                SCL_boardGetMoves(model->game.board, i, possibleMoves);

                SCL_SQUARE_SET_ITERATE_BEGIN(possibleMoves)
                SCL_moveToString(model->game.board, i, iteratedSquare, 'q', string);
                //printf("%s ", string);
                SCL_SQUARE_SET_ITERATE_END
            }

        return FlipChessStatusReturn;
    }

    model->msg = (SCL_boardWhitesTurn(model->game.board) ? "white to move" : "black to move");

    // Seed cached eval with the actual starting position so an imported FEN
    // (or a fresh game where the player moves first) shows a real number
    // instead of 0.0 until someone makes a move.
    model->cachedEval = SCL_boardEvaluateStatic(model->game.board);

    // 0 = success
    return FlipChessStatusNone;
}

bool flipchess_scene_1_input(InputEvent* event, void* context) {
    furi_assert(context);
    FlipChessScene1* instance = context;
    FlipChess* app = instance->context;

    // Back is intentionally NOT consumed here -- returning false lets the
    // view dispatcher route Short/Long Back to the navigation callback,
    // which calls scene_manager_handle_back_event -> scene's Back handler.
    // That path is reliable even when watch-mode AI compute briefly busy-
    // blocks the GUI thread; the previous "send custom event from input"
    // path could appear unresponsive because the custom event waited
    // behind the next tick in the dispatcher queue.
    if(event->key == InputKeyBack) {
        return false;
    }

    // Long-press OK = undo. Undoes the last AI reply plus the player's move
    // when there's an AI opponent, so the human ends up on move again.
    if(event->type == InputTypeLong && event->key == InputKeyOk) {
        with_view_model(
            instance->view,
            FlipChessScene1Model * model,
            {
                if(model->game.ply > 0 &&
                   model->game.state == SCL_GAME_STATE_PLAYING) {
                    SCL_gameUndoMove(&(model->game));
                    // If there's at least one AI player and a previous ply
                    // exists, also undo so the human is back on move.
                    if((model->paramPlayerW != 0 || model->paramPlayerB != 0) &&
                       model->game.ply > 0) {
                        SCL_gameUndoMove(&(model->game));
                    }
                    // Reset selection / move-in-progress state.
                    model->turnState = 0;
                    SCL_squareSetClear(model->moveHighlight);
                    model->squareFrom = 255;
                    model->squareTo = 255;
                    // Refresh status line and clear pending move text.
                    model->msg = SCL_boardWhitesTurn(model->game.board) ?
                                     "white to move" :
                                     "black to move";
                    model->moveString[0] = '\0';
                    // History lines below would otherwise still show the
                    // move(s) we just undid, so blank them.
                    model->msg2 = "";
                    model->moveString2[0] = '\0';
                    // Recompute eval for the restored position.
                    model->cachedEval = SCL_boardEvaluateStatic(model->game.board);
                    flipchess_drawBoard(model);
                }
            },
            true);
        flipchess_play_happy_bump(app);
        // Persist the rolled-back position so Resume picks it up correctly.
        with_view_model(
            instance->view,
            FlipChessScene1Model * model,
            { flipchess_saveState(app, model); },
            true);
        return true;
    }

    if(event->type == InputTypeRelease) {
        switch(event->key) {
        case InputKeyBack:
            // Back is handled via the navigation callback path; the early
            // return at the top of this function ensures we never reach
            // here for InputKeyBack. Listed only to satisfy -Wswitch.
            break;
        case InputKeyRight:
            with_view_model(
                instance->view,
                FlipChessScene1Model * model,
                {
                    if(model->squareSelectedLast != 255 && model->squareSelected == 255) {
                        model->squareSelected = model->squareSelectedLast;
                    } else {
                        // Visual right: file +1 normally, file -1 when flipped.
                        int delta = model->paramFlipBoard ? -1 : 1;
                        model->squareSelected = (model->squareSelected & 0x38) |
                                                ((model->squareSelected + delta) & 0x07);
                    }
                    flipchess_drawBoard(model);
                },
                true);
            break;
        case InputKeyDown:
            with_view_model(
                instance->view,
                FlipChessScene1Model * model,
                {
                    if(model->squareSelectedLast != 255 && model->squareSelected == 255) {
                        model->squareSelected = model->squareSelectedLast;
                    } else {
                        // Visual down: rank -1 normally (+56 mod 64),
                        // rank +1 when flipped (+8 mod 64).
                        uint8_t step = model->paramFlipBoard ? 8 : 56;
                        model->squareSelected = (model->squareSelected + step) % 64;
                    }
                    flipchess_drawBoard(model);
                },
                true);
            break;
        case InputKeyLeft:
            with_view_model(
                instance->view,
                FlipChessScene1Model * model,
                {
                    if(model->squareSelectedLast != 255 && model->squareSelected == 255) {
                        model->squareSelected = model->squareSelectedLast;
                    } else {
                        // Visual left: file -1 normally, file +1 when flipped.
                        int delta = model->paramFlipBoard ? 1 : -1;
                        model->squareSelected = (model->squareSelected & 0x38) |
                                                ((model->squareSelected + delta) & 0x07);
                    }
                    flipchess_drawBoard(model);
                },
                true);
            break;
        case InputKeyUp:
            with_view_model(
                instance->view,
                FlipChessScene1Model * model,
                {
                    if(model->squareSelectedLast != 255 && model->squareSelected == 255) {
                        model->squareSelected = model->squareSelectedLast;
                    } else {
                        // Visual up: rank +1 normally (+8), rank -1 flipped (+56 mod 64).
                        uint8_t step = model->paramFlipBoard ? 56 : 8;
                        model->squareSelected = (model->squareSelected + step) % 64;
                    }
                    flipchess_drawBoard(model);
                },
                true);
            break;
        case InputKeyOk:
            with_view_model(
                instance->view,
                FlipChessScene1Model * model,
                {
                    // if(model->paramExit == FlipChessStatusReturn) {
                    //     instance->callback(FlipChessCustomEventScene1Back, instance->context);
                    //     break;
                    // }
                    if(!flipchess_isPlayerTurn(model)) {
                        model->thinking = 1;
                    }
                },
                true);
            furi_thread_flags_wait(0, FuriFlagWaitAny, THREAD_WAIT_TIME);

            with_view_model(
                instance->view,
                FlipChessScene1Model * model,
                {
                    // first turn of round, probably player but could be AI
                    uint8_t prevTurnState = model->turnState;
                    if(flipchess_turn(model) == FlipChessStatusReturn) {
                        if(app->sound == 1) flipchess_voice_a_strange_game();
                        flipchess_play_long_bump(app);
                    } else if(prevTurnState == 1 && model->turnState == 0) {
                        // turnState reset from waiting-for-dest to idle = illegal move
                        flipchess_play_bad_bump(app);
                    }
                    flipchess_saveState(app, model);
                    flipchess_drawBoard(model);
                },
                true);

            with_view_model(
                instance->view,
                FlipChessScene1Model * model,
                {
                    if(!flipchess_isPlayerTurn(model)) {
                        model->thinking = 1;
                    }
                },
                true);
            furi_thread_flags_wait(0, FuriFlagWaitAny, THREAD_WAIT_TIME);

            with_view_model(
                instance->view,
                FlipChessScene1Model * model,
                {
                    // if player played, let AI play
                    if(!flipchess_isPlayerTurn(model)) {
                        if(flipchess_turn(model) == FlipChessStatusReturn) {
                            if(app->sound == 1) flipchess_voice_a_strange_game();
                            flipchess_play_long_bump(app);
                        }
                        flipchess_saveState(app, model);
                        flipchess_drawBoard(model);
                    }
                },
                true);
            break;
        case InputKeyMAX:
            break;
        }
    }
    return true;
}

void flipchess_scene_1_exit(void* context) {
    furi_assert(context);
    FlipChessScene1* instance = (FlipChessScene1*)context;

    with_view_model(instance->view, FlipChessScene1Model * model, { model->paramExit = 0; }, true);
}

bool flipchess_scene_1_try_cancel_selection(FlipChessScene1* instance) {
    bool cancelled = false;
    with_view_model(
        instance->view,
        FlipChessScene1Model * model,
        {
            if(model->turnState == 1) {
                model->turnState = 0;
                SCL_squareSetClear(model->moveHighlight);
                flipchess_drawBoard(model);
                cancelled = true;
            }
        },
        true);
    return cancelled;
}

// Per-speed "thinking..." dwell, in 100 ms ticks. Indexed by app->watch_speed
// (0=Fast, 1=Normal, 2=Slow, 3=V.Slow). This is *minimum* dwell -- AI
// computation time stacks on top.
static const uint8_t watch_speed_ticks[4] = {2, 5, 10, 20};

// Auto-restart hold time after game-over, in 100 ms ticks. Indexed by
// app->watch_restart_delay: 0=3s, 1=10s, 2=25s, 3=60s.
static const uint16_t watch_restart_ticks[4] = {30, 100, 250, 600};

void flipchess_scene_1_tick(FlipChessScene1* instance, void* app_context) {
    FlipChess* app = (FlipChess*)app_context;

    // Tick-driven state machine for watch mode. We deliberately do NOT
    // furi_thread_flags_wait() here -- that blocks the GUI thread, which
    // queues the user's Back press until the wait + AI computation
    // finish. Instead each phase yields, the next 100 ms tick re-enters,
    // and input events get a chance to run between ticks.
    //
    //   game over     -> count ticks toward auto-restart (if enabled)
    //   thinking + N>0 -> decrement N, redraw, wait another tick
    //   thinking + N=0 -> compute the AI move
    //   AI's turn      -> arm thinking, set N from speed setting
    bool should_compute = false;
    bool should_restart = false;
    with_view_model(
        instance->view,
        FlipChessScene1Model * model,
        {
            if(model->game.state != SCL_GAME_STATE_PLAYING) {
                if(app->watch_autorestart) {
                    uint8_t didx = app->watch_restart_delay < 4 ?
                                       app->watch_restart_delay :
                                       2;
                    uint16_t target = watch_restart_ticks[didx];
                    if(model->autoRestartTicks < target) {
                        model->autoRestartTicks++;
                    } else {
                        model->autoRestartTicks = 0;
                        should_restart = true;
                    }
                }
                // Auto-restart off: do nothing, leave the result on screen
                // until the user presses Back.
            } else if(model->thinking && model->thinkingTicksRemaining > 0) {
                // Still pacing the move; let the dispatcher service
                // input events between ticks.
                model->thinkingTicksRemaining--;
            } else if(model->thinking) {
                // Dwell elapsed -- next thing this tick handler does is
                // run the AI, which will reset thinking when it returns.
                should_compute = true;
            } else if(!flipchess_isPlayerTurn(model)) {
                // AI's turn just came up. Arm the thinking dwell and
                // redraw so "thinking..." appears immediately.
                model->thinking = 1;
                uint8_t idx = app->watch_speed < 4 ? app->watch_speed : 1;
                model->thinkingTicksRemaining = watch_speed_ticks[idx];
            }
        },
        true);

    if(should_restart) {
        // Re-init the model with a fresh AI vs AI game and play the first move.
        with_view_model(
            instance->view,
            FlipChessScene1Model * model,
            {
                uint8_t lvl = app->watch_ai_level >= 1 && app->watch_ai_level <= 3 ?
                                  app->watch_ai_level :
                                  1;
                flipchess_scene_1_model_init(model, lvl, lvl, NULL);
                model->watchCappedSearch = 1;
                if(flipchess_turn(model) != FlipChessStatusReturn) {
                    flipchess_saveState(app, model);
                    flipchess_drawBoard(model);
                }
            },
            true);
        return;
    }

    if(!should_compute) return;

    // Compute the AI move. This still blocks the GUI thread for the
    // duration of the search, but the dwell loop above means a viewer
    // pressing Back during the dwell phase gets a response within one
    // tick (~100 ms) instead of waiting for the full speed setting.
    with_view_model(
        instance->view,
        FlipChessScene1Model * model,
        {
            if(flipchess_turn(model) == FlipChessStatusReturn) {
                if(app->sound == 1) flipchess_voice_a_strange_game();
                flipchess_play_long_bump(app);
            }
            flipchess_saveState(app, model);
            flipchess_drawBoard(model);
        },
        true);
}

void flipchess_scene_1_enter(void* context) {
    furi_assert(context);
    FlipChessScene1* instance = (FlipChessScene1*)context;
    FlipChess* app = instance->context;

    flipchess_play_happy_bump(app);

    with_view_model(
        instance->view,
        FlipChessScene1Model * model,
        {
            int init;
            if(app->watch_mode == 1) {
                // Watch mode: both sides at the configured AI level,
                // always a fresh game (no FEN import).
                uint8_t lvl = app->watch_ai_level >= 1 && app->watch_ai_level <= 3 ?
                                  app->watch_ai_level :
                                  1;
                init = flipchess_scene_1_model_init(model, lvl, lvl, NULL);
                // Cap search extensions so the synchronous AI compute
                // can't lock out Back for tens of seconds.
                model->watchCappedSearch = 1;
            } else {
                // load imported game if applicable
                char* import_game_text = NULL;
                if(app->import_game == 1 && strlen(app->import_game_text) > 0) {
                    import_game_text = app->import_game_text;
                } else {
                    if(app->sound == 1) flipchess_voice_how_about_chess();
                }
                init = flipchess_scene_1_model_init(
                    model, app->white_mode, app->black_mode, import_game_text);
            }

            if(init == FlipChessStatusNone) {
                // perform initial turn, sets up and lets white
                // AI play if applicable
                const uint8_t turn = flipchess_turn(model);
                if(turn == FlipChessStatusReturn) {
                    init = turn;
                } else {
                    flipchess_saveState(app, model);
                    flipchess_drawBoard(model);
                }
            }

            // if return status, return from scene immediately
            // if(init == FlipChessStatusReturn) {
            //     instance->callback(FlipChessCustomEventScene1Back, instance->context);
            // }
        },
        true);
}

FlipChessScene1* flipchess_scene_1_alloc() {
    FlipChessScene1* instance = malloc(sizeof(FlipChessScene1));
    instance->view = view_alloc();
    view_allocate_model(instance->view, ViewModelTypeLocking, sizeof(FlipChessScene1Model));
    view_set_context(instance->view, instance); // furi_assert crashes in events without this
    view_set_draw_callback(instance->view, (ViewDrawCallback)flipchess_scene_1_draw);
    view_set_input_callback(instance->view, flipchess_scene_1_input);
    view_set_enter_callback(instance->view, flipchess_scene_1_enter);
    view_set_exit_callback(instance->view, flipchess_scene_1_exit);

    return instance;
}

void flipchess_scene_1_free(FlipChessScene1* instance) {
    furi_assert(instance);

    with_view_model(instance->view, FlipChessScene1Model * model, { UNUSED(model); }, true);

    view_free(instance->view);
    free(instance);
}

View* flipchess_scene_1_get_view(FlipChessScene1* instance) {
    furi_assert(instance);
    return instance->view;
}
