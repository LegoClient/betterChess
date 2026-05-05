# betterChess

`HOW ABOUT A NICE GAME OF CHESS?`

![FLIPR](icons/FLIPR_128x64.png)

Chess for the Flipper Zero. Forked from [xtruan/flipper-chess](https://github.com/xtruan/flipper-chess) with a long list of bug fixes plus a hands-off **Watch AI** spectator mode, undo, evaluation display, captured-pieces strip, and configurable AI behaviour.

Built against (firmware API 86.0) — see [Building](#building) below.

## Features

### Play
- Human vs CPU 1 / CPU 2 / CPU 3, CPU vs CPU, or Human vs Human
- Auto-flip the board when you play as Black so your pieces sit at the bottom of the screen
- Save / Resume — the position is persisted to the SD card and restored from the menu's **Resume Game**
- **Long-press OK to undo** the last move (vs CPU: undoes both your move and the AI's reply so you're back on move)
- Haptic buzz on illegal-move attempts
- Cursor navigation respects rank/file boundaries (Right at h-file wraps within the rank; doesn't jump to the next rank)

### Watch AI (spectator mode)
- Endless AI vs AI with auto-restart between games
- Board orientation alternates each game so you see both sides' perspective over a session
- Bounded search extensions in watch mode so Back stays responsive even on CPU 3
- Configurable in **Watch Settings**:

  | Setting | Values | Default |
  |---|---|---|
  | AI Level | CPU 1 / CPU 2 / CPU 3 | CPU 1 |
  | Speed | Fast (200 ms) / Normal (500 ms) / Slow (1 s) / V.Slow (2 s) | Normal |
  | Auto-restart | ON / OFF | ON |
  | Restart Wait | 3 s / 10 s / 25 s / 60 s | 25 s |

  *Speed* is the minimum "thinking…" dwell per move; actual AI compute time stacks on top.
  *Restart Wait* is how long the final position holds on screen before the next game starts.

### Settings
- **White / Black**: Human, CPU 1, CPU 2, CPU 3
- **Vibro / Haptic**: ON / OFF

### Side panel display
- Current message + last move
- Previous move (one ply of history)
- Static evaluation: `White: 1.4`, `Black: 0.5`, or `Even`
- Captured-pieces strip on two lines: `W:<black pieces taken>` / `B:<white pieces taken>` (in descending value: Q, R, B, N, P)

## Controls

| Button | In a game | In menus |
|---|---|---|
| ⬅ ➡ ⬆ ⬇ | Move cursor (rank/file-bounded) | Navigate |
| OK (short) | Pick piece / make move | Select |
| OK (long) | Undo last move | — |
| Back (short) | Cancel selection if one is in progress, otherwise exit to menu | Back / exit |

## Installation

1. Grab `betterchess.fap` (build it yourself — see below — or copy it from `dist/`).
2. Drop it into `apps/Games/` on your Flipper SD card.
3. Open it from the Apps menu.

## Building

The project uses [ufbt](https://github.com/flipperdevices/flipperzero-ufbt) and currently targets the **Momentum mntm-011** SDK (API 86.0).

```bash
# install ufbt once
pip install --upgrade ufbt

# point ufbt at the Momentum mntm-011 SDK
ufbt update --url=https://github.com/Next-Flip/Momentum-Firmware/releases/download/mntm-011/flipper-z-f7-sdk-mntm-011.zip

# build
git clone <this-repo>
cd flipper-chess
ufbt
```

The compiled `betterchess.fap` lands in `dist/`.

To regenerate the app icons (`flipchess_10px.png` and `icons/FLIPR_128x64.png`), run `python scripts/gen_icons.py`.

## Credits

- Original [flipper-chess](https://github.com/xtruan/flipper-chess) by [Struan Clark (xtruan)](https://github.com/xtruan)
- Chess engine: [smallchesslib](https://codeberg.org/drummyfish/smallchesslib) by drummyfish (CC0)
