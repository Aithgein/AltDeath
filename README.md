# Shades Respawn Addon

A small SKSE addon for **Shades of Mortality** that lets you choose where you return after death.

Open **SKSE Menu Framework**, stand where you want to respawn, and select **Set Current Location**. From then on, whenever Shades of Mortality resurrects you, this addon moves you back to that location.

Shades still handles the death itself, including health restoration, injuries, gold loss, and any other options you have enabled. This addon only handles the relocation.

## Requirements

* Skyrim Script Extender (SKSE)
* Address Library for SKSE Plugins
* Shades of Mortality
* SKSE Menu Framework

## Installation

Install the mod normally through Mod Organizer 2, Vortex, or another mod manager. The DLL should end up at:

```text
Data/SKSE/Plugins/shades-respawn-addon.dll
```

This mod has no ESP or ESL and does not need a load-order position.

For the intended behavior, disable Shades of Mortality’s enemy-calming effect in:

```text
Data/SKSE/Plugins/shades_custom.toml
```

Set:

```toml
[Settings]
bToggleCalmSpell = false
```

## Use

1. Stand at the location where you want to return after death.
2. Open SKSE Menu Framework.
3. Select **Shades Respawn Addon**.
4. Select **Respawn Location**.
5. Press **Set Current Location**.

The message **“You are bound.”** confirms that the location was saved.

The bound location remains in effect until you replace it with another one. Sleeping and waiting do not change it.

If Shades resurrects you before you have chosen a location, the addon displays **“You are not bound.”** and leaves you where Shades revived you.

## What the Mod Does

* Adds one button to SKSE Menu Framework for marking your current position.
* Saves that position with your game.
* Detects when Shades of Mortality resurrects the player.
* Moves the player to the saved position.

It uses no Papyrus scripts and adds no plugin file.

## Compatibility

This addon is made specifically for Shades of Mortality. It identifies Shades’ resurrection through the ethereal effect applied during its death sequence. If a future Shades update changes that effect or renames its plugin, this addon may need an update.

## Building from Source

The repository includes a GitHub Actions workflow. Open the repository’s **Actions** tab, select **Build**, and run the workflow. The completed run produces a mod-manager-ready ZIP under **Artifacts**.

The project can also be built locally with XMake:

```powershell
xmake f -m releasedbg -y
xmake -y
```

## License

MIT
