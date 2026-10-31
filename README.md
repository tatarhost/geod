# Geod Studio

Geod Studio is a Geode mod project for Geometry Dash 2.2081. The first milestone adds an in-game mobile-friendly script workspace opened from the main menu by tapping **GEOD**.

## Current features

- **Editor:** enter commands one line at a time, add them to the script, save, load, and run.
- **Console:** see script output, notifications, errors, and execution counts.
- **Script library:** list saved `.gds` files.
- **Persistent scripts:** files are stored in Geode's per-mod save directory, under `scripts/`.
- **Android builds:** CI targets Android64 and Android32, plus Windows.

## Geod Script API (v0.1)

The current runtime is a small, sandboxed command interpreter. It does not execute arbitrary C++, native binaries, shell commands, or downloaded code.

```text
print("Hello from Geod!")
toast("My mod is running")
notify("Script finished")
help()
```

Supported calls:

- `print("text")` — write to the Geod console.
- `toast("text")` — show a temporary message inside the game.
- `notify("text")` — show a Geometry Dash dialog.
- `help()` — print the API help.
- `clear()` — clear the console.

Each command is one line. A script can contain up to 80 lines. The editor currently uses a single-line mobile keyboard input to add each source line, so it works without needing a desktop keyboard.

## Building

The GitHub Actions workflow builds Android64, Android32, and Windows 64-bit. After a successful workflow, download the **Geod-Studio** artifact from the Actions run. It contains the packaged `.geode` file(s). On Android, use the package matching the device architecture and install it through Geode.

## Development notes

This is the first functional foundation, not yet a general Lua/C++ runtime or a compiler for standalone native Geode mods. The current scripts run inside Geod and can call the API above. A later runtime can expand the API with event callbacks and richer Geometry Dash objects while keeping scripts portable across Android and desktop.

Geode SDK documentation: https://docs.geode-sdk.org/
