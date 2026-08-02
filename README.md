# gbs-ConfigLoadSavePlugin

**Version 4.3.0 — Requires GB Studio ≥ 4.3.0**

A GB Studio engine plugin that lets you define exactly which script variables are included in the game's save data, replacing the default "save everything" behaviour with a compact, targeted save structure. It also adds dedicated save, load, and peek events that use this custom structure, and a peek-by-index event that reads individual variables from a save slot without loading the entire save.

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
3. [Size Limits and Restrictions](#size-limits-and-restrictions)
4. [Events Reference](#events-reference)
5. [Memory Footprint](#memory-footprint)

---

## Concepts

### The Default GB Studio Save Structure

The built-in GB Studio save system writes a fixed list of engine state blocks into SRAM: every script variable, every running script context, input and timer events, music state, the current scene, the scene stack, and all actor data. This is a complete engine save-state. While powerful, it consumes a large amount of SRAM per slot, and loading it fully overwrites the current game state.

### Custom Save Points

This plugin replaces that list with a set of individual script variables you choose. Only those variables are written to SRAM on save and read back on load. The save blob is much smaller, and the structure is predictable and stable across game updates.

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
2. Compatibility variants are included for use alongside the **MetaTilePlugin**, the **SceneStackExPlugin**, or both at once, and are selected automatically — nothing to configure.
3. Add a **Save configuration** event to any script in your project (typically a scene's On Init or a dedicated "setup" script). This event runs when the project is built and defines the custom save structure.
4. Use **Save Game Data Using Save Config** and **Load Game Data Using Save Config** everywhere you previously used the built-in Game Data Save / Game Data Load events.

> ⚠️ If the **Save configuration** event is not present in any script, the plugin falls back to the default full-state save structure.

---

### How to Use

### Define What to Save

1. Add a **Save configuration** event.
2. Set **Amount of variables** to the number of variables you want to persist.
3. Select each variable from the list. The order matters — the index assigned to each variable (0, 1, 2 …) is the index used by the **Store Variable from Game Data In Variable by Index** event.

### Save and Load

- Use **Save Game Data Using Save Config** to write the configured variables to a slot (1, 2, or 3).
- Use **Load Game Data Using Save Config** to read them back.

### Peek a Variable Without Loading

Use **Store Variable from Game Data In Variable by Index** to read one variable from a save slot into a script variable. Specify the **Saved data index** (0 = first variable in the configuration, 1 = second, etc.) and the save slot. This does not change any current game state.

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

The number of save slots and the SRAM bank layout are controlled by engine settings outside this plugin. Slot addresses are derived from the save blob size, so a small custom save structure fits more slots in the same SRAM space.

### Modified Engine File

The plugin replaces the stock save/load engine file, which is what makes the built-in save events use the custom structure too. Another plugin that patches the same file needs one of the included compatibility variants.

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

> ⚠️ Use **Save Game Data Using Save Config**, **Load Game Data Using Save Config**, and **Store Variable from Game Data In Variable by Index** whenever this event is in the project. Do not mix with the standard save events.

---

### Save Game Data Using Save Config

**Event ID:** `EVENT_SAVE_DATA_EX`  
**Groups:** Save Data, Variables

Saves the configured variables to the specified save slot. Equivalent to the built-in Game Data Save but uses the custom save point list when a configuration is defined.

| Field | Type | Default | Description |
|---|---|---|---|
| Save slot | Toggle (1 / 2 / 3) | 1 | The SRAM slot to write to (slots map to indices 0, 1, 2 internally). |

---

### Load Game Data Using Save Config

**Event ID:** `EVENT_LOAD_DATA_EX`  
**Groups:** Save Data, Variables

Loads the configured variables from the specified save slot. Returns silently without changing state if the slot is empty or the save structure has changed.

| Field | Type | Default | Description |
|---|---|---|---|
| Save slot | Toggle (1 / 2 / 3) | 1 | The SRAM slot to read from. |

---

### Store Variable from Game Data In Variable by Index

**Event ID:** `EVENT_PEEK_DATA_BY_INDEX`  
**Groups:** Save Data, Variables

Reads a single variable from a save slot by its index in the save configuration, without loading the full save. Useful for displaying save slot summaries.

| Field | Type | Default | Description |
|---|---|---|---|
| Variable (destination) | Variable picker | Last variable | The script variable to store the read value in. |
| Saved data index | Number | 0 | 0-based index of the variable in the **Save configuration** list to read. |
| Save slot | Toggle (1 / 2 / 3) | 1 | The SRAM slot to read from. |

---

## Memory Footprint

Measured against the stock GB Studio **4.3.0-e1** engine (per-file SDCC compile with GB Studio's build flags, default engine settings). Values are the plugin's *delta* versus the stock engine; DMG build, with CGB noted where it differs. ROM cost lands in banked ROM (GB Studio's autobanker spreads it across switchable banks); using the plugin's events additionally compiles a few bytes of GBVM script per call into your project's script banks.

| | Cost |
|---|---|
| WRAM | +0 bytes |
| ROM | +455 bytes |

- **WRAM:** no change — the plugin only changes which data is written to SRAM and where.
- **Engine WRAM headroom:** the stock GB Studio 4.3.0 engine leaves about **854 bytes** of WRAM free (usable engine WRAM is 7,776 bytes at 0xC0A0–0xDF00; the stock engine uses 6,922 bytes). With this plugin installed roughly **854 bytes** remain. This figure does not depend on how many global variables your project defines: the script memory array has a fixed size of VM_HEAP_SIZE + (VM_MAX_CONTEXTS × VM_CONTEXT_STACK_SIZE) words — 768 + 16 × 64 = 1,792 words (3,584 bytes) with stock engine settings.
- **SRAM:** yes — this plugin *is* the save system. Save slots are relocated from SRAM bank 0 to banks 1–3 (bank 0 is left untouched so SRAM-hungry plugins like MetaTilePlugin/SceneStackExPlugin can coexist). Each save blob contains only the variables you list in the Save Config event, so slots are far smaller than stock save-states; the exact size is your variable list plus a few bytes of header per entry.

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
| Bank 0 free with this plugin installed | **1,451** of 16,384 (91% used) |

**This plugin costs nothing in bank 0.** All of its code lives in a switchable
ROM bank; nothing it adds is resident in bank 0.

<details><summary>How this was measured</summary>

GB Studio 4.3.2, DMG target, default engine settings. Each module's bank 0
contribution is the `A _HOME size` record that SDCC writes into its `.rel`
object, summed over the engine sources this plugin provides. Stock sizes come
from building projects whose only plugin ships no engine C, so every module in
them is the untouched engine; two such builds were compared and agreed on all
73 shared modules.

The "free" figure is a stock project with this plugin and nothing else. Your
own number will differ: other plugins, and any engine settings that change what
the core compiles, move it independently of this plugin.

</details>
<!-- BANK0:END -->
