# gbs-ConfigLoadSavePlugin

**Version 4.3.11 — Requires GB Studio ≥ 4.3.0**

A GB Studio engine plugin that lets you define exactly which script variables are included in the game's save data, replacing the default "save everything" behaviour with a compact, targeted save structure. It replaces the built-in Game Data Save, Game Data Load and If Game Data Saved events in place — same events, same scripts, no duplicates in the Add Event menu — extending each with a save slot that can be a number or a variable, and adds a peek-by-index event that reads individual variables from a save slot without loading the entire save.

**Why use this plugin?**

- The default GB Studio save serialises nearly the entire engine state (all variables, all VM contexts, all actors, music state, etc.). If you only need to persist a handful of game progress variables this wastes significant SRAM space.
- The plugin makes it possible to have a save slot that acts like a traditional game save (persist progress variables only) rather than a save-state snapshot.

![image](https://github.com/user-attachments/assets/dfb79afd-3435-4270-9775-9a7488dca526)
![image](https://github.com/user-attachments/assets/e30eed80-2cc4-4216-850c-c76b6abc2ef0)
![image](https://github.com/user-attachments/assets/df098de2-9883-4bf3-87af-5dd2b4d37134)

![image](https://github.com/user-attachments/assets/0790c332-7aca-42fa-a01c-73eb49417610)

---

## Table of Contents

1. [Concepts](#concepts)
2. [Project Setup](#project-setup)
3. [Replacing the Built-in Events](#replacing-the-built-in-events)
4. [Engine Settings](#engine-settings)
5. [Size Limits and Restrictions](#size-limits-and-restrictions)
6. [Events Reference](#events-reference)
7. [Memory Footprint](#memory-footprint)
8. [Bank 0 (HOME) Usage](#bank-0-home-usage)
9. [Changelog](#changelog)

---

## Concepts

### The Default GB Studio Save Structure

The built-in GB Studio save system writes a fixed list of engine state blocks into SRAM: every script variable, every running script context, input and timer events, music state, the current scene, the scene stack, and all actor data. This is a complete engine save-state. While powerful, it consumes a large amount of SRAM per slot, and loading it fully overwrites the current game state.

### Custom Save Points

This plugin replaces that list with a set of individual script variables you choose. Only those variables are written to SRAM on save and read back on load. The save blob is much smaller, and the structure is predictable and stable across game updates.

### All Variables Only

**All variables only** is the blunt version of the same idea: instead of naming individual variables, the save blob is every global variable in one block (768 variables, 1,536 bytes) and nothing else. No scene, no actors, no music, no running scripts. It needs no **Save configuration** event.

The saved region deliberately stops at the end of the global variables. The rest of `script_memory` is the VM's context stacks, which belong to the scripts running at that moment — including the one that asked for the load — so restoring them from a save would pull the stack out from under the caller.

### Save Blob Layout

Each save slot in SRAM uses the following layout regardless of which save structure is active:

```
[4 bytes] Save signature (validity check)
[2 bytes] Save blob size
For each save point:
  [2 bytes] Block size
  [1 byte]  Block ID
  [N bytes] Block data
```

The signature and blob size are checked on load. If they do not match the current structure, the load is rejected.

### Peek: Reading Without Loading

The **Store Variable from Game Data In Variable by Index** event reads a single variable out of a save slot without loading the full save. This allows, for example, reading a character's level from each save slot to display on a save selection screen.

---

## Project Setup

1. Copy the plugin folder into your GB Studio project's `plugins/` directory.
2. Compatibility variants are included for use alongside the **MetaTilePlugin**, the **SceneStackExPlugin**, or both at once, and are selected automatically — nothing to configure. The plugin loads early (`"order": -2`) so that plugins patching the same engine files build on top of its version rather than the other way round.
3. Pick **Save structure** in *Settings → Engine → Configure Load/Save*:
   - **Full save-state** — GB Studio's own structure. Nothing changes; this is the default.
   - **All variables only** — every global variable, nothing else. No event needed.
   - **Custom variable set** — the variables you name. Also add a **Save configuration** event to any script (typically a scene's On Init or a dedicated "setup" script); it runs when the project is built and spells out the list.
4. Optionally set **Save slot count** and **Starting SRAM bank** in the same engine settings group to decide how many save slots the game has and where in SRAM they live.
5. Carry on using the built-in **Game Data Save**, **Game Data Load** and **If Game Data Saved** events — the plugin replaces them in place, so existing scripts pick up the new save structure with no edits.

> ⚠️ The setting and the event have to agree: **Custom variable set** without a **Save configuration** event fails the build, and so does a **Save configuration** event under any other structure. Both errors say which one to change.

---

### How to Use

### Define What to Save

1. Set **Save structure** to **Custom variable set**, then add a **Save configuration** event.
2. Set **Amount of variables** to the number of variables you want to persist.
3. Select each variable from the list. The order matters — the index assigned to each variable (0, 1, 2 …) is the index used by the **Store Variable from Game Data In Variable by Index** event.

### Save and Load

- Use **Game Data Save** to write the configured variables to a slot.
- Use **Game Data Load** to read them back.
- Use **If Game Data Saved** to branch on whether a slot holds a save, e.g. to grey out an empty slot on a title screen.
- The slot picker offers the usual slots 1, 2 and 3, plus a **#** button that swaps in a number or variable field so a script can pick any slot the SRAM layout has room for. That field is 0-based: slot 1 on the toggle is 0 here.

### Peek a Variable Without Loading

Use **Store Variable from Game Data In Variable by Index** to read one variable from a save slot into a script variable. Specify the **Saved data index** (0 = first variable in the configuration, 1 = second, etc.) and the save slot. This does not change any current game state.

---

## Replacing the Built-in Events

The plugin ships its own **Game Data Save**, **Game Data Load** and **If Game Data Saved** under the stock event IDs, so they replace the built-in ones rather than sitting next to them. Existing scripts keep working untouched, and the Add Event menu shows one of each.

What they add is the save slot picker: the usual slots 1, 2 and 3, plus a **#** button that swaps in a number or variable field, so a script can pick any slot the save structure has room for.

### Fixed slot

A fixed slot compiles exactly as the built-in event always did — GB Studio's own save/load path, byte for byte. Nothing about an existing project's output changes by installing the plugin.

What *does* change is what the engine does with a load. GB Studio fades the screen out, restores the save and rebuilds the scene the save was taken in. That only makes sense under **Full save-state**, where the save actually holds a scene. Under the other two structures the plugin's `core.c` skips the fade and the reload, restores the data where the game stands, and lets the calling script carry on. Without that, a load would fade to black with no saved script left to fade back in.

### Variable slot

GB Studio carries the save slot as a literal operand inside the save/load instruction, so a slot held in a variable cannot use that path. Those events call the plugin's own save/load instead, which runs inside the calling script rather than between frames.

That is fine for a save built from variables, but it cannot restore running scripts — it is running on them. So a variable slot needs **All variables only** or **Custom variable set**. Under **Full save-state** the event refuses to compile and says so:

> Game Data Load: a save slot taken from a variable needs a save structure that holds no running scripts. Set "Save structure" to "All variables only" or "Custom variable set" in Settings > Engine > Configure Load/Save, or pick a fixed slot.

Peeking or checking a variable slot is always safe and has no such restriction.

### The "On Load" branch

**Game Data Save**'s *On Load* branch runs when a later load resumes the script inside that event, which needs the running scripts to be in the save. Only **Full save-state** keeps them, so under the other two structures that branch never runs — put the script after **Game Data Load** instead. The build warns when one is filled in.

---

## Engine Settings

*Settings → Engine → Configure Load/Save*

| Setting | Default | Description |
|---|---|---|
| Save structure | Full save-state | What a save slot holds. See below. |
| Starting SRAM bank | 1 | First SRAM bank the save slots are written to (0–3). |
| Save slot count | 3 | How many save slots the game has (1–255). |

### Save structure

| Option | A slot holds | Bytes per slot |
|---|---|---|
| **Full save-state** | GB Studio's own structure: every variable, the scene, the actors, the music and the running scripts | ~3.9 KB |
| **All variables only** | all 768 global variables, nothing else | 1,545 |
| **Custom variable set** | the variables named by a **Save configuration** event | 6 + 5 per variable |

**Full save-state** is the default and behaves exactly like GB Studio without the plugin: loading resumes the game where it was saved, scene and all.

The other two hold variables and nothing else, which changes what a load means:

- **Loading does not load a scene.** The current scene, the actor table and the running scripts are not in the save, so there is nothing to restore them from: the game keeps running exactly where it is and only the variables change underneath it. Music is left alone for the same reason, and the engine skips the scene reload it would otherwise do.
- **Game Data Save**'s *On Load* branch can no longer run, since no running script is saved. The build warns if one is filled in.
- A save slot can come from a variable, which **Full save-state** cannot do.

Under **All variables only**, the **Store Variable from Game Data In Variable by Index** event's index is the variable's own index rather than a position in a **Save configuration** list, and the stock **Store Variable From Save Data** event reads the same way.

**Custom variable set** needs a **Save configuration** event somewhere in the project, and that event needs this setting — either one alone fails the build with a message naming the other.

### Where the save slots live

**Save slot count** is the number of slots the game has, not an amount of SRAM. Slot 0 is the first; the save, load and check events refuse anything from the count upwards.

How much SRAM those slots take is worked out from the size of one save: slots are packed head to tail, as many whole blobs as fit in a bank, then on to the next bank. Three slots of a five-variable save occupy 93 bytes; three slots of **All variables only** occupy 4,635. A GB Studio ROM is always linked with 4 SRAM banks (32KB), so slots that would run past bank 3 are refused as well — see [Guards](#guards).

Compatibility variants raise the floor to protect the other plugin's SRAM, whatever **Starting SRAM bank** is set to:

| Installed alongside | First bank actually used |
|---|---|
| *(nothing)* | as configured (0–3) |
| MetaTilePlugin | 1 — bank 0 holds its map and collision data |
| SceneStackExPlugin | 1 — bank 0 holds its scene stack |
| Both | 2 — bank 0 and bank 1 are taken |

> ⚠️ Moving **Starting SRAM bank** relocates every slot. Existing save data is not migrated: it stays where it was and the game stops finding it. Changing **Save slot count** is safe — slots keep their addresses, the count only decides where the range stops.

### Guards

At runtime, `data_slot_address` returns nothing — Save and Load do nothing, Peek returns false — when:

- the slot is at or past **Save slot count**;
- one save is larger than a whole SRAM bank, so writing it would run off the end of the bank;
- the slot would land past SRAM bank 3.

At build time:

- A Save/Load/Peek event with a literal slot number at or past **Save slot count** fails the build, naming the limit, instead of failing silently on hardware. A slot held in a variable can only be checked at runtime.
- A Save or Load event with a variable slot under **Full save-state** fails the build, because that combination cannot work.
- **Save structure** set to **Custom variable set** with no **Save configuration** event in the project fails the build, and so does the reverse.
- With **All variables only**, the size of a save is known too, so a literal slot that the SRAM cannot physically hold is reported as a build warning.
- The **Save configuration** event reports the size of a save, the slot count and the banks it needs in the build log; warns when the configured slots do not fit in SRAM; and errors out if the variable list cannot fit in one SRAM bank.

---

## Size Limits and Restrictions

### Save Configuration Is Compile-Time Only

The **Save configuration** event runs when the project is built and emits no runtime code. Changing which variables are included requires a full rebuild.

### Save/Load Events Must Match the Configuration

Once a **Save configuration** event exists, the built-in Game Data Save/Load events use the same custom structure, so they behave identically to the plugin's own events. Using the plugin-provided events is still recommended, for clarity.

### Peek Index Is 0-Based, Matching Configuration Order

The index passed to **Store Variable from Game Data In Variable by Index** is the 0-based position of the variable in the **Save configuration** event's variable list. If you reorder or add variables to the configuration, existing peek-by-index calls in scripts may read the wrong variable. Always verify indices after changing the configuration.

### Changing the Configuration Invalidates Old Saves

The save blob includes a size field. If the number of saved variables changes between builds, the blob size changes, and loading an old save fails the size check without corrupting game state. Existing save data in SRAM will become unreadable.

### Variable Amount Limit: 768

The **Save configuration** event supports a maximum of 768 variables (the GB Studio variable limit).

### SRAM Slot Count and Size

Slot addresses are derived from the save blob size, so a small custom save structure fits more slots in the same SRAM space. **Save slot count** decides how many slots the game offers and **Starting SRAM bank** where they begin; see [Engine Settings](#engine-settings). The slot index is a byte, so 255 slots is the absolute maximum however small the blob is.

### Modified Engine Files

The plugin replaces the stock save/load engine file, which is what makes the built-in save events use the custom structure too, and patches `core.c` so a load skips the scene reload when the save holds no scene.

Because it patches `core.c`, the plugin declares `"order": -2` so it is applied before the other plugins that touch that file. SceneStackExPlugin (`-9`) is applied before it, and the included SceneStackEx variants carry both sets of changes; SimulateInputPlugin (`-1`) is applied after it and ships its own variants for this plugin.

---

## Events Reference

### Save Configuration

**Event ID:** `EVENT_SAVE_CONFIG`  
**Groups:** Save Data, Variables

Build-time event that defines which variables are included in the save structure. Produces no runtime code.

| Field | Type | Default | Description |
|---|---|---|---|
| Amount of variables | Number | 1 | How many variables to include (1–768). |
| Variable at index 0 … N | Variable picker | Last variable | Each variable to save, in order. Index 0 is the first entry; this index is used by the peek event. |

> ⚠️ Needs **Save structure** set to **Custom variable set**. Under any other structure this event fails the build rather than being quietly ignored.

> ⚠️ Use **Store Variable from Game Data In Variable by Index** rather than the built-in **Store Variable From Save Data** whenever this event is in the project: the built-in peek reads by variable index, which a custom save point list does not lay out that way. Game Data Save, Game Data Load and If Game Data Saved are replaced by the plugin and need no swapping.

---

### Game Data Save

**Event ID:** `EVENT_SAVE_DATA` (replaces the built-in event)
**Groups:** Save Data

Saves the configured variables to the specified save slot.

| Field | Type | Default | Description |
|---|---|---|---|
| Save slot | Toggle (1 / 2 / 3 / #) | 1 | The SRAM slot to write to (slots map to indices 0, 1, 2 internally). **#** reveals the field below. |
| Slot number | Value (number or variable) | 0 | Shown when **#** is selected: a 0-based slot index worked out at runtime. Must be below **Save slot count**; writing to a slot SRAM has no room for does nothing. |
| On Save | Events | — | Runs after the save. |
| On Load | Events | — | Runs when a later load resumes here. Needs the full save-state structure; see [Replacing the Built-in Events](#replacing-the-built-in-events). |

---

### Game Data Load

**Event ID:** `EVENT_LOAD_DATA` (replaces the built-in event)
**Groups:** Save Data

Loads the configured variables from the specified save slot. Returns silently without changing state if the slot is empty or the save structure has changed.

| Field | Type | Default | Description |
|---|---|---|---|
| Save slot | Toggle (1 / 2 / 3 / #) | 1 | The SRAM slot to read from. **#** reveals the field below. |
| Slot number | Value (number or variable) | 0 | Shown when **#** is selected: a 0-based slot index worked out at runtime. Must be below **Save slot count**; reading a slot SRAM has no room for does nothing. |

---

### If Game Data Saved

**Event ID:** `EVENT_IF_SAVED_DATA` (replaces the built-in event)
**Groups:** Save Data, Control Flow

Runs one branch or the other depending on whether the slot holds a save this build can read. Nothing is loaded and no game state changes.

A slot counts as saved only when it exists (below **Save slot count**, and within SRAM) *and* its signature matches the current build. A save written before the save structure changed reads as empty.

| Field | Type | Default | Description |
|---|---|---|---|
| Save slot | Toggle (1 / 2 / 3 / #) | 1 | The SRAM slot to check. **#** reveals the field below. |
| Slot number | Value (number or variable) | 0 | Shown when **#** is selected: a 0-based slot index worked out at runtime. |
| True | Events | — | Runs when the slot holds a save. |
| Else | Events | — | Runs when it does not. |

---

### Store Variable from Game Data In Variable by Index

**Event ID:** `EVENT_PEEK_DATA_BY_INDEX`  
**Groups:** Save Data, Variables

Reads a single variable from a save slot by its index in the save configuration, without loading the full save. Useful for displaying save slot summaries.

| Field | Type | Default | Description |
|---|---|---|---|
| Variable (destination) | Variable picker | Last variable | The script variable to store the read value in. |
| Saved data index | Number | 0 | 0-based index of the variable in the **Save configuration** list to read. With **All variables only** it is the variable's own index instead. |
| Save slot | Toggle (1 / 2 / 3 / #) | 1 | The SRAM slot to read from. **#** reveals the field below. |
| Slot number | Value (number or variable) | 0 | Shown when **#** is selected: a 0-based slot index worked out at runtime. |

---

## Memory Footprint

Measured against the stock GB Studio **4.3.0-e1** engine by `measure_plugin_memory.js` (per-file SDCC compile with GB Studio's own build flags, at default engine settings; report of 2026-08-13). Figures are this plugin's *delta* versus stock — a file that replaces a stock engine file counts only the difference, which is why a plugin can come out negative. Using the plugin's events additionally compiles a few bytes of GBVM script per call into your project's script banks, on top of the fixed cost below.

| Budget | Cost |
|---|---|
| Bank 0 (HOME) | 0 bytes |
| WRAM | 0 bytes |
| Banked ROM | +455 bytes |

- **Bank 0:** nothing. Every function the plugin adds is compiled into a switchable ROM bank.
- **WRAM:** no change — the plugin only changes which data is written to SRAM, and where.
- **Engine WRAM headroom:** a stock GB Studio 4.3.0 project leaves about **854 bytes** of WRAM free (usable engine WRAM is 7,776 bytes at 0xC0A0–0xDF00; the stock engine uses 6,922). With this plugin installed roughly **854 bytes** remain. That does not change with the number of global variables your project defines: the script memory array is a fixed 3,584 bytes at stock engine settings (VM_HEAP_SIZE + VM_MAX_CONTEXTS × VM_CONTEXT_STACK_SIZE = 768 + 16 × 64 words).
- **SRAM:** yes — this plugin *is* the save system. Save slots start at bank 1 by default, leaving bank 0 for SRAM-hungry plugins like MetaTilePlugin/SceneStackExPlugin, and the **Starting SRAM bank** / **Save slot count** engine settings decide where they begin and how many there are. Under **Custom variable set** a slot holds only the variables you list, so slots are far smaller than stock save-states: 6 bytes of header plus 5 per variable. Under **All variables only** every slot is a flat 1,545 bytes.

---

<!-- BANK0:BEGIN -->
## Bank 0 (HOME) Usage

Bank 0 is the 16 KB non-switchable ROM bank that the GB Studio engine core,
the interrupt handlers and the GBDK runtime all share. Banked ROM is cheap
(add another bank), bank 0 is not, so it is usually the first thing a project
runs out of.

| | Bytes |
|---|---|
| Bank 0 used by this plugin | **0** |

**This plugin costs nothing in bank 0.** Every one of its functions is compiled
into a switchable ROM bank; nothing it adds is resident in bank 0.
<!-- BANK0:END -->

## Changelog

Grouped by the date each change was merged into the official
[gb-studio-plugins](https://github.com/gb-studio-dev/gb-studio-plugins) repository.

Only bug fixes, new features and feature changes are listed. Engine version
bumps, patch regeneration, packaging fixes and documentation edits are omitted.

### 2026-08-21

- Added the **Save structure** engine setting — Full save-state, All variables
  only, or Custom variable set — as the single place a project says what a save
  slot holds. The last two make a save blob of variables and nothing else, so
  loading restores no scene, no actors and no running scripts.
- Added the **Starting SRAM bank** and **Save slot count** engine settings, and
  made the compatibility variants clamp the first bank up so save slots can
  never land on another plugin's SRAM.
- The plugin's save, load and check events now replace the built-in
  **Game Data Save**, **Game Data Load** and **If Game Data Saved** under the
  stock event IDs instead of sitting beside them as separate events. Existing
  scripts pick them up with no edits.
- They can now take a slot number or variable instead of the fixed three slots,
  as can the peek-by-index event.
- `core.c` is now patched so a load skips the fade-out and scene reload when the
  save structure holds no scene, instead of leaving a black screen.
- Added guards: a slot at or past the configured slot count, a save blob too
  large for one SRAM bank, or a slot landing past the last SRAM bank, is refused
  instead of overrunning SRAM; a literal slot number outside the count fails the
  build.
- Fixed a load from a project with a **Save configuration** event leaving SRAM
  switched to the save bank when the blob failed its size or ID check.

### 2026-06-14

- Added custom script parameter / stack support to the events.

### 2025-12-20

- Save data now starts from SRAM bank 1.

### 2025-10-29

- Fixed the plugin so that saving and loading actually work.

### 2025-02-24

- Initial release.
