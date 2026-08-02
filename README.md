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
3. [How to Use](#how-to-use)
4. [Technicalities and Restrictions](#technicalities-and-restrictions)
5. [Events Reference](#events-reference)
6. [Inner Workings](#inner-workings)
7. [Memory Footprint](#memory-footprint)

---

## Concepts

### The Default GB Studio Save Structure

The built-in GB Studio save system serialises a fixed list of engine state blocks into SRAM: all script variables (`script_memory`), all VM contexts, input/timer events, music state, scene pointer, scene stack, and all actor data. This is a complete engine save-state. While powerful, it consumes a large amount of SRAM per slot, and loading it fully overwrites the current game state.

### Custom Save Points

This plugin replaces the save point list with a user-defined set of individual script variables. Each variable in the list becomes one `save_point_t` entry:

```c
SAVEPOINT(script_memory[variable_alias], index)
```

Only those variables are written to SRAM on save and read back on load. The save blob is much smaller, and the structure is predictable and stable across game updates.

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

The signature and blob size are checked on load. If they do not match the current structure, the load is rejected and returns `FALSE`.

### Peek: Reading Without Loading

The **Store Variable from Game Data In Variable by Index** event reads a single variable out of a save slot without loading the full save. It uses `data_peek_ex` which navigates the custom save structure by block index to find the correct offset. This allows, for example, reading a character's level from each save slot to display on a save selection screen.

---

## Project Setup

1. Copy the plugin folder into your GB Studio project's `plugins/` directory.
2. If your project also uses other plugins that modify `load_save.c`, use the matching pre-merged `engineAlt` subfolder:

| Other plugins in use | Use `engineAlt/` subfolder |
|---|---|
| MetaTilePlugin | `MetaTilePlugin/` |
| SceneStackExPlugin | `SceneStackExPlugin/` |
| MetaTilePlugin + SceneStackExPlugin | `MetaTilePlugin_SceneStackExPlugin/` |

3. Add a **Save configuration** event to any script in your project (typically a scene's On Init or a dedicated "setup" script). This event runs at compile time and generates the custom `save_points.c` / `save_points.h` files.
4. Use **Save Game Data Using Save Config** and **Load Game Data Using Save Config** everywhere you previously used the built-in Game Data Save / Game Data Load events.

> ⚠️ If the **Save configuration** event is not present in any script, the plugin falls back to the default full-state save structure.

---

## How to Use

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

## Technicalities and Restrictions

### Save Configuration Is Compile-Time Only

The **Save configuration** event runs at GB Studio compile time. It generates `save_points.c` and `save_points.h` asset files. Changing which variables are included requires a full project rebuild. The event does not emit any GBVM bytecode.

### Save/Load Events Must Match the Configuration

The custom `vm_data_save_ex` / `vm_data_load_ex` functions in the plugin's `load_save.c` use the generated `save_points` array when `data/save_points.h` is present. If you use the **built-in** Game Data Save/Load events alongside the custom ones, the built-in events will also use the same custom `load_save.c` (because the plugin replaces the file), so the built-in save/load events are effectively identical to the custom ones when this plugin is installed. Using the plugin-provided events is recommended for clarity.

### Peek Index Is 0-Based, Matching Configuration Order

The index passed to **Store Variable from Game Data In Variable by Index** is the 0-based position of the variable in the **Save configuration** event's variable list. If you reorder or add variables to the configuration, existing peek-by-index calls in scripts may read the wrong variable. Always verify indices after changing the configuration.

### Changing the Configuration Invalidates Old Saves

The save blob includes a size field. If the number of saved variables changes between builds, the blob size changes, and loading an old save will fail the size check and return `FALSE` without corrupting game state. Existing save data in SRAM will become unreadable.

### Variable Amount Limit: 768

The **Save configuration** event supports a maximum of 768 variables (the GB Studio variable limit).

### SRAM Slot Count and Size

The number of save slots and the SRAM bank layout are controlled by engine settings outside this plugin (`SRAM_BANKS_TO_SAVE`). The plugin's `data_slot_address` function calculates where each slot begins based on `save_blob_size`. With a very small custom save structure, more slots can fit in the same SRAM space.

### Modified Engine File

The plugin replaces `engine/src/core/load_save.c`. This file is used by both the custom events and the built-in Game Data Save/Load events (since they all link against the same `data_save` / `data_load` / `data_peek` functions).

---

## Events Reference

### Save Configuration

**Event ID:** `EVENT_SAVE_CONFIG`  
**Groups:** Save Data, Variables

Compile-time event that generates the `save_points.c` and `save_points.h` asset files defining which variables are included in the save structure. Produces no runtime bytecode.

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

## Inner Workings

### Conditional Compilation: Custom vs Default

`load_save.c` uses a `__has_include` preprocessor check to select which save point list to use:

```c
#if __has_include ("data/save_points.h")
#include "data/save_points.h"
// use banked save_points array from the generated file
#else
// use the inline default full-state save_point_t array
#endif
```

When the **Save configuration** event generates `save_points.h`, all `data_save`, `data_load`, and `data_peek_ex` functions switch to iterating the custom list via `MemcpyBanked` (because the custom array is in a ROM bank). When the file is absent, the original flat inline array is used directly without bank switching.

### Generated `save_points.c`

For a configuration with three variables (aliases 5, 12, 42):

```c
#pragma bank 255

#include <string.h>
#include "data/save_points.h"
#include "vm.h"
#include "data/game_globals.h"

BANKREF(save_points)

const save_point_t save_points[] = {
    SAVEPOINT(script_memory[5], 0),
    SAVEPOINT(script_memory[12], 1),
    SAVEPOINT(script_memory[42], 2),
    SAVEPOINTS_END
};
```

`SAVEPOINT(A, ID)` expands to `{ &(A), sizeof(A), (ID) }`. Each entry saves 2 bytes (one `uint16_t` variable), plus 3 bytes of overhead (2-byte size field + 1-byte ID). The total blob size for 3 variables is: 4 (signature) + 2 (blob size) + 3 × (2 + 1 + 2) = 21 bytes per slot, compared to several kilobytes for the default full-state save.

### `data_save` and `data_load` with Custom Points

Both functions iterate the `save_points` array via `MemcpyBanked` (since the array is in a ROM bank):

```c
MemcpyBanked(&point_ref, point_ptr, sizeof(save_point_t), BANK(save_points));
while (point_ref.target) {
    // write/read block size, ID, and data
    point_ptr++;
    MemcpyBanked(&point_ref, point_ptr, sizeof(save_point_t), BANK(save_points));
}
```

On load, each block's size and ID are checked before the data is copied. A mismatch returns `FALSE` immediately, leaving game state untouched.

### `data_peek_ex` — Navigating by Block Index

Unlike the standard `data_peek` (which assumes the first save point is `script_memory` and indexes into it as a flat array), `data_peek_ex` navigates to a specific block by its ordinal index. The byte offset into the SRAM slot for block `idx` is:

```c
offset = sizeof(save_signature)
       + sizeof(save_blob_size)
       + ((idx + 1) * (sizeof(size_t) + sizeof(uint8_t) + sizeof(int16_t)))
       - sizeof(int16_t)
```

This formula skips `idx + 1` block headers (size + ID), then backs up by `sizeof(int16_t)` to land on the data of block `idx`. Since each custom save point saves exactly one `int16_t` (2 bytes), the formula is fixed-stride and requires no iterating. `count` variables are then copied from that address.

### `vm_data_save_ex`, `vm_data_load_ex`, `vm_data_peek_ex`

These thin wrapper functions extract arguments from the VM stack and delegate to `data_save`, `data_load`, and `data_peek_ex`:

```c
void vm_data_save_ex(SCRIPT_CTX * THIS) OLDCALL BANKED {
    data_save(*(uint8_t *)VM_REF_TO_PTR(FN_ARG0));
}
void vm_data_load_ex(SCRIPT_CTX * THIS) OLDCALL BANKED {
    data_load(*(uint8_t *)VM_REF_TO_PTR(FN_ARG0));
}
void vm_data_peek_ex(SCRIPT_CTX * THIS) OLDCALL BANKED {
    data_peek_ex(
        *(uint8_t *)  VM_REF_TO_PTR(FN_ARG0),   // slot
        *(uint16_t *) VM_REF_TO_PTR(FN_ARG1),   // index
        *(uint16_t *) VM_REF_TO_PTR(FN_ARG2),   // count (always 1 from the event)
        &script_memory[*(int16_t*) VM_REF_TO_PTR(FN_ARG3)] // destination variable
    );
}
```


---

## Memory Footprint

Measured against the stock GB Studio **4.3.0-e1** engine (per-file SDCC compile with GB Studio's build flags, default engine settings). Values are the plugin's *delta* versus the stock engine; DMG build, with CGB noted where it differs. ROM cost lands in banked ROM (GB Studio's autobanker spreads it across switchable banks); using the plugin's events additionally compiles a few bytes of GBVM script per call into your project's script banks.

| | Cost |
|---|---|
| WRAM | +0 bytes |
| ROM | +455 bytes |

- **WRAM:** no change — the plugin only replaces the stock `load_save.c` save-point table and slot addressing.
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
