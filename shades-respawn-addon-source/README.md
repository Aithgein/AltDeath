# Shades Respawn Addon

A standalone SKSE/CommonLibSSE-NG addon for **Shades of Mortality**.

It does exactly three things:

1. After a successful sleep, it records the player's location.
2. After a successful wait, it records the player's location.
3. When Shades applies its player ethereal effect during resurrection, it moves the player to that recorded location on the next game-thread update.

It does not subscribe to Resurrection API, replace Shades' DLL, add an ESP/ESL, use Papyrus, calm enemies, heal enemies, clear combat, fade the screen, or add configuration menus.

## Requirements

- Skyrim Special Edition/Anniversary Edition supported by the current CommonLibSSE-NG build
- SKSE
- Address Library as required by CommonLibSSE-NG
- Resurrection API
- Shades of Mortality

## Shades settings

Disable Shades' calming in `Data/SKSE/Plugins/shades_custom.toml`:

```toml
[Settings]
bToggleCalmSpell = false
```

Shades' existing `HealAndCalmEnemiesOnDeath` function also controls enemy healing and optional cell resurrection. To leave defeated enemies entirely untouched, use:

```toml
[Settings]
bToggleCalmSpell = false
fHealEnemiesArea = 0.0
bResurrectEnemiesInCell = false
```

## Build with GitHub Actions

1. Create an empty GitHub repository.
2. Upload the contents of this folder to its `main` branch.
3. Open **Actions** → **Build** → **Run workflow**.
4. Open the completed run and download the `Shades-Respawn-Addon` artifact.
5. Install `Shades-Respawn-Addon.zip` with MO2 after the requirements.

The workflow fetches CommonLibSSE-NG, builds the DLL, and creates an MO2-ready ZIP.

## Behavior

- The checkpoint updates only after a non-interrupted sleep or wait.
- Sleeping or waiting again replaces the previous checkpoint.
- Dying before the first successful sleep/wait leaves Shades' ordinary resurrection behavior unchanged, minus whatever Shades options you disabled.
- The addon recognizes Shades by the player ethereal magic effect at local FormID `0x800` in `shade-of-mortality.esp`.
- The checkpoint uses a persistent runtime XMarker and stores its FormID in the SKSE cosave.

## Testing checklist

Test on a disposable save first:

- sleep indoors → die outdoors
- wait outdoors → die indoors
- save/reload after setting a checkpoint → die
- set a second checkpoint → confirm it replaces the first
- die before setting any checkpoint
- die twice in quick succession

The log is written to the normal SKSE plugin log directory as `shades-respawn-addon.log`.

## Current status

Version 0.1.1 updates the logging calls for the current CommonLibSSE build. Runtime behavior still needs in-game testing.
