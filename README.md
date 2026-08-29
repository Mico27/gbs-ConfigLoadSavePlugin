# gbs-ConfigLoadSavePlugin

**Version 4.3.11. Requires GB Studio 4.3.0 or newer.**

Lets you choose exactly which variables go into a save, instead of GB Studio saving the entire state of the game.

GB Studio's own save is a snapshot: every variable, every running script, every actor, the music, the scene. That is about 3.9 KB per slot, so three slots fill most of a cartridge's save memory, and loading one drops the player back into the exact scene it was taken in.

A save of just the variables you name is 6 bytes plus 5 per variable. Twenty variables gives a 106 byte save, so you can have dozens of slots, and loading one leaves the game running where it stands. That is what a traditional save file behaves like, and what a save select screen showing several files needs.

It replaces the built-in **Game Data Save**, **Game Data Load** and **If Game Data Saved** events in place, so existing scripts keep working and the Add Event menu shows one of each. Each gains a save slot that can come from a variable, and there is a new event that reads one variable out of a slot without loading it, for showing a summary of each file.

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
7. [FAQ](#faq)
8. [Memory Footprint](#memory-footprint)
9. [Bank 0 (HOME) Usage](#bank-0-home-usage)
10. [Changelog](#changelog)

---

## Concepts

### The Default GB Studio Save Structure

GB Studio's save writes a fixed list of blocks into the cartridge's save memory: every variable, every running script, input and timer events, music, the current scene, the scene stack and all actor data. It is a complete snapshot of the game. It also takes a lot of room per slot, and loading it replaces everything.

### Custom Save Points

This plugin replaces that list with the variables you choose. Only those are written on save and read back on load. The save is far smaller and its layout is predictable.

### All Variables Only

**All variables only** is the same idea without a list: the save is every global variable in one block, 768 of them in 1,536 bytes, and nothing else. No scene, no actors, no music, no running scripts. It needs no **Save configuration** event.

It deliberately stops at the end of the variables. What follows belongs to the scripts running at that moment, including the one that asked for the load, so restoring it would pull the ground out from under the caller.

### Save Blob Layout

Every save slot uses the same layout, whichever save structure is active:

```
[4 bytes] Save signature (validity check)
[2 bytes] Save blob size
For each save point:
  [2 bytes] Block size
  [1 byte]  Block ID
  [N bytes] Block data
```

Both the signature and the size are checked on load. A save that does not match the current structure is refused.

### Peek: Reading Without Loading

**Store Variable from Game Data In Variable by Index** reads one variable out of a slot without loading it. That is how you build a save select screen showing each file's level, play time or location.

---

## Project Setup

1. Copy the plugin folder into your project's `plugins` folder.
2. Compatibility variants ship for **MetaTilePlugin** and **SceneStackExPlugin**, alone or together, and GB Studio picks the right one. The plugin loads early so that plugins changing the same engine files build on top of its version.
3. Pick **Save structure** under **Settings**, then **Engine**, then **Configure Load/Save**:
   - **Full save-state** is GB Studio's own structure and the default. Nothing changes.
   - **All variables only** saves every global variable and nothing else. No event needed.
   - **Custom variable set** saves the variables you name. Add a **Save configuration** event to any script, typically a scene's On Init or a dedicated setup script. It runs during the build and spells out the list.
4. Set **Save slot count** and **Starting SRAM bank** in the same group to decide how many files the game has and where they live.
5. Carry on using **Game Data Save**, **Game Data Load** and **If Game Data Saved**. The plugin replaces them in place, so existing scripts pick up the new structure with no edits.

> ⚠️ The setting and the event have to agree: **Custom variable set** without a **Save configuration** event fails the build, and so does a **Save configuration** event under any other structure. Both errors say which one to change.

---

### How to Use

### Define What to Save

1. Set **Save structure** to **Custom variable set**, then add a **Save configuration** event.
2. Set **Amount of variables** to the number of variables you want to persist.
3. Pick each variable from the list. The order matters, because the position of each variable in the list, counting from 0, is the number the **Store Variable from Game Data In Variable by Index** event uses.

### Save and Load

- Use **Game Data Save** to write the configured variables to a slot.
- Use **Game Data Load** to read them back.
- Use **If Game Data Saved** to branch on whether a slot holds a save, for instance to grey out an empty file on a title screen.
- The slot picker offers slots 1, 2 and 3, plus a **#** button that swaps in a number or variable field so a script can pick any slot the layout has room for. That field counts from 0, so slot 1 on the toggle is 0 there.

### Peek a Variable Without Loading

**Store Variable from Game Data In Variable by Index** reads one variable from a slot into a variable of yours. Give it the **Saved data index**, counting from 0 through your configuration list, and the slot. Nothing about the running game changes.

---

## Replacing the Built-in Events

The plugin's **Game Data Save**, **Game Data Load** and **If Game Data Saved** take the place of the built-in ones rather than sitting next to them. Existing scripts keep working and the Add Event menu shows one of each.

What they add is the slot picker: slots 1, 2 and 3 as before, plus a **#** button that swaps in a number or variable field so a script can pick any slot the layout has room for.

### Fixed slot

A fixed slot builds exactly as the built-in event always did, byte for byte. Installing the plugin changes nothing about an existing project's output.

What does change is what a load does. GB Studio fades the screen out, restores the save and rebuilds the scene it was taken in. That only makes sense under **Full save-state**, where the save holds a scene. Under the other two the plugin skips the fade and the reload, restores the variables where the game stands, and lets the calling script carry on. Without that, a load would fade to black with no saved script left to fade it back in.

### Variable slot

GB Studio writes the slot number directly into its save and load instruction, so a slot held in a variable cannot use that route. Those events call the plugin's own save and load instead, which run inside the calling script.

That works for a save built from variables. It cannot restore running scripts, because it is running on them. So a variable slot needs **All variables only** or **Custom variable set**. Under **Full save-state** the event stops the build and says so:

> Game Data Load: a save slot taken from a variable needs a save structure that holds no running scripts. Set "Save structure" to "All variables only" or "Custom variable set" in Settings > Engine > Configure Load/Save, or pick a fixed slot.

Peeking or checking a variable slot is always safe and has no such restriction.

### The "On Load" branch

**Game Data Save**'s **On Load** branch runs when a later load resumes the script inside that event, which needs the running scripts to be in the save. Only **Full save-state** keeps them, so under the other two structures the branch never runs. Put that script after **Game Data Load** instead. The build warns when one is filled in.

---

## Engine Settings

*Settings → Engine → Configure Load/Save*

| Setting | Default | Description |
|---|---|---|
| Save structure | Full save-state | What a save slot holds. See below. |
| Starting SRAM bank | 1 | The first save memory bank the slots are written to, 0 to 3. |
| Save slot count | 3 | How many save files the game has, 1 to 255. |

### Save structure

| Option | A slot holds | Bytes per slot |
|---|---|---|
| **Full save-state** | GB Studio's own structure: every variable, the scene, the actors, the music and the running scripts | about 3.9 KB |
| **All variables only** | all 768 global variables, nothing else | 1,545 |
| **Custom variable set** | the variables named by a **Save configuration** event | 6, plus 5 per variable |

**Full save-state** is the default and behaves exactly as GB Studio does without the plugin. Loading resumes the game where it was saved, scene and all.

The other two hold variables and nothing else, which changes what a load means:

- **Loading does not load a scene.** The scene, the actors and the running scripts are not in the save, so the game keeps running where it is and only the variables change underneath it. Music is left alone for the same reason.
- **Game Data Save**'s **On Load** branch can no longer run, because no running script is saved. The build warns if one is filled in.
- A slot can come from a variable, which **Full save-state** cannot do.

Under **All variables only**, the number given to **Store Variable from Game Data In Variable by Index** is the variable's own number rather than a position in a list, and the stock **Store Variable From Save Data** event reads the same way.

**Custom variable set** needs a **Save configuration** event somewhere in the project, and that event needs this setting. Either one on its own stops the build with a message naming the other.

### Where the save slots live

**Save slot count** is how many files the game has, not an amount of memory. Slot 0 is the first, and the save, load and check events refuse anything from the count upwards.

How much memory those files take follows from the size of one save. Slots are packed one after another, as many whole ones as fit in a bank, then on to the next. Three slots of a five-variable save take 93 bytes, and three slots of **All variables only** take 4,635. A GB Studio ROM always has 4 banks of save memory, 32 KB in total, so slots that would run past the last bank are refused. See [Guards](#guards).

Compatibility variants raise the first bank used to protect the other plugin's data, whatever **Starting SRAM bank** says:

| Installed alongside | First bank actually used |
|---|---|
| nothing else | as configured, 0 to 3 |
| MetaTilePlugin | 1, because bank 0 holds its map and collision data |
| SceneStackExPlugin | 1, because bank 0 holds its scene stack |
| Both | 2, because banks 0 and 1 are taken |

> Moving **Starting SRAM bank** relocates every slot. Existing save data is not moved with it, so the game stops finding it. Changing **Save slot count** is safe, because slots keep their addresses and the count only decides where the range stops.

### Guards

While the game runs, Save and Load do nothing and the read-one-variable event reports failure when:

- the slot is at or past **Save slot count**;
- one save is larger than a whole bank, so writing it would run off the end;
- the slot would land past the last bank.

At build time:

- An event with a typed-in slot number at or past **Save slot count** stops the build and names the limit. A slot held in a variable can only be checked while the game runs.
- A Save or Load with a variable slot under **Full save-state** stops the build, because that combination cannot work.
- **Custom variable set** with no **Save configuration** event stops the build, and so does the reverse.
- With **All variables only** the size of a save is known, so a typed-in slot the memory cannot hold is reported as a build warning.
- The **Save configuration** event reports the size of a save, the slot count and the banks it needs in the build log. It warns when the slots do not fit and stops the build when the variable list will not fit in one bank.

---

## Size Limits and Restrictions

### Save Configuration Is Compile-Time Only

The **Save configuration** event runs during the build and adds no code to your game. Changing which variables are included means rebuilding.

### Save/Load Events Must Match the Configuration

Once a **Save configuration** event exists, the built-in save and load events use the same structure, so they behave the same as the plugin's own.

### Peek Index Is 0-Based, Matching Configuration Order

The number given to **Store Variable from Game Data In Variable by Index** is the position of the variable in the **Save configuration** list, counting from 0. Reordering or inserting variables shifts every position after the change, so check your scripts after editing the list.

### Changing the Configuration Invalidates Old Saves

A save records its own size. Changing how many variables are saved changes that size, so an old save fails the check and is refused. Nothing is corrupted, but existing save files become unreadable.

### Variable Amount Limit: 768

The **Save configuration** event holds up to 768 variables, which is the GB Studio limit.

### SRAM Slot Count and Size

Slot addresses follow from the size of a save, so a small custom structure fits more files in the same space. **Save slot count** decides how many the game offers and **Starting SRAM bank** where they begin. See [Engine Settings](#engine-settings). The slot number is one byte, so 255 files is the most possible however small each one is.

### Modified Engine Files

The plugin replaces the stock save and load code, which is what makes the built-in save events use the custom structure, and changes the main game loop so a load skips the scene reload when the save holds no scene.

Because it changes the main game loop, it is applied before the other plugins that touch that file. SceneStackExPlugin is applied before it, and the SceneStackEx variants here carry both sets of changes. SimulateInputPlugin is applied after it and ships its own variants for this plugin.

---

## Events Reference

### Save Configuration

**Groups:** Save Data, Variables

Names the variables a save holds. Runs during the build and adds no code to your game.

| Field | Type | Default | Description |
|---|---|---|---|
| Amount of variables | Number | 1 | How many variables to include, 1 to 768. |
| Variable at index 0 to N | Variable | Last variable | Each variable to save, in order. The first is number 0, and that number is what the read-one-variable event uses. |

> Needs **Save structure** set to **Custom variable set**. Under any other structure it stops the build rather than being quietly ignored.

> Use **Store Variable from Game Data In Variable by Index** in place of the built-in **Store Variable From Save Data** whenever this event is in the project. The built-in one reads by variable number, which a custom list does not lay out that way. Game Data Save, Game Data Load and If Game Data Saved are replaced by the plugin and need no swapping.

---

### Game Data Save

Replaces the built-in event. Group: Save Data.

Saves the configured variables to the specified save slot.

| Field | Type | Default | Description |
|---|---|---|---|
| Save slot | Toggle: 1, 2, 3 or # | 1 | Which file to write to. **#** reveals the field below. |
| Slot number | Value or variable | 0 | Shown with **#** selected. Counts from 0 and is worked out while the game runs. It must be below **Save slot count**, and writing to a file the memory has no room for does nothing. |
| On Save | Script | none | Runs after the save. |
| On Load | Script | none | Runs when a later load resumes here. Needs the full save-state structure. See [Replacing the Built-in Events](#replacing-the-built-in-events). |

---

### Game Data Load

Replaces the built-in event. Group: Save Data.

Reads the configured variables back from a file. It does nothing, without complaint, when the file is empty or was written by a build with a different save structure.

| Field | Type | Default | Description |
|---|---|---|---|
| Save slot | Toggle: 1, 2, 3 or # | 1 | Which file to read from. **#** reveals the field below. |
| Slot number | Value or variable | 0 | Shown with **#** selected. Counts from 0. It must be below **Save slot count**, and reading a file the memory has no room for does nothing. |

---

### If Game Data Saved

Replaces the built-in event. Groups: Save Data, Control Flow.

Branches on whether the file holds a save this build can read. Nothing is loaded and nothing about the running game changes.

A file counts as saved when it exists, meaning below **Save slot count** and inside the available memory, and when its signature matches this build. A save written before the structure changed reads as empty.

| Field | Type | Default | Description |
|---|---|---|---|
| Save slot | Toggle: 1, 2, 3 or # | 1 | Which file to check. **#** reveals the field below. |
| Slot number | Value or variable | 0 | Shown with **#** selected. Counts from 0. |
| True | Script | none | Runs when the file holds a save. |
| Else | Script | none | Runs when it does not. |

---

### Store Variable from Game Data In Variable by Index

**Groups:** Save Data, Variables

Reads one variable out of a save file by its position in the configuration, without loading the file. This is how you show a summary of each save on a file select screen.

| Field | Type | Default | Description |
|---|---|---|---|
| Variable (destination) | Variable | Last variable | Where to put the value. |
| Saved data index | Number | 0 | Which entry of the **Save configuration** list to read, counting from 0. With **All variables only** it is the variable's own number instead. |
| Save slot | Toggle: 1, 2, 3 or # | 1 | Which file to read from. **#** reveals the field below. |
| Slot number | Value or variable | 0 | Shown with **#** selected. Counts from 0. |

---

## FAQ

**My three save slots eat all the cartridge's save memory. Can I get more?**
Yes. Set **Save structure** to **Custom variable set** and name only the variables that matter. A
twenty variable save is 106 bytes, so dozens of files fit where three snapshots did.

**How do I show the player's level and play time on a save select screen?**
Use **Store Variable from Game Data In Variable by Index** once per value, for each slot. It reads
straight out of the file without loading it, so nothing about the running game changes.

**How do I let the player pick a slot from a variable?**
Press the **#** button on the slot picker and point it at a variable. That needs **All variables
only** or **Custom variable set**, because a full snapshot cannot be loaded that way.

**Do I have to change my existing save scripts?**
No. The plugin replaces **Game Data Save**, **Game Data Load** and **If Game Data Saved** in place,
so existing scripts pick up the new structure with no edits.

**Loading now leaves the player where they are instead of the saved scene. Is that a bug?**
No, that is what the variable-only structures do. They hold no scene, so nothing is reloaded. Save
the scene number in a variable yourself and change scene after loading if you want the old
behaviour.

**My old saves stopped working after I added a variable.**
A save records its own size, so changing the list makes older files unreadable. Nothing is
corrupted, and **If Game Data Saved** reports them as empty. Finish the list before release.

**My On Load branch never runs.**
It needs the running scripts to be in the save, which only **Full save-state** keeps. Put that
script after **Game Data Load** instead. The build warns about this.

**What number does the read-one-variable event want?**
The position in the **Save configuration** list, counting from 0. Under **All variables only** it
is the variable's own number instead.

**How many save files can I have?**
Up to 255, as long as they fit. Three files of a five-variable save take 93 bytes in total, so the
limit is usually the count rather than the memory.

**My save data vanished after I changed a setting.**
Moving **Starting SRAM bank** relocates every file and the old data is left behind. Changing
**Save slot count** is safe, since files keep their addresses.

**Can I use this with the MetaTile or SceneStackEx plugins, which also need save memory?**
Yes. Compatibility variants ship for both, and they push the save files past whatever memory the
other plugin owns.

**My build failed complaining about Save configuration.**
The setting and the event have to agree. **Custom variable set** needs a **Save configuration**
event, and that event needs the setting. The message names the one to change.

---

## Memory Footprint

Measured against the stock GB Studio **4.3.0-e1** engine at default engine settings, report of 2026-08-13. Figures are the difference against a stock project: a file that replaces a stock engine file counts only the change, which is why a plugin can come out negative. Each event you use also compiles a few bytes of script into your project, on top of the fixed cost below.

| Budget | Cost |
|---|---|
| Bank 0 (HOME) | 0 bytes |
| WRAM | 0 bytes |
| Banked ROM | +455 bytes |

- **Bank 0:** nothing. Everything the plugin adds is compiled into a switchable ROM bank.
- **WRAM:** no change. The plugin only changes what is written to the cartridge's save memory, and where.
- **Engine WRAM headroom:** a stock GB Studio 4.3.0 project leaves about **854 bytes** of WRAM free (the engine has 7,776 bytes to work with and uses 6,922 of them). With this plugin installed roughly **854 bytes** remain. Adding more global variables to your project does not change that figure, because script memory is a fixed 3,584 byte block at stock engine settings.
- **SRAM:** this plugin is the save system. Files start at bank 1 by default, leaving bank 0 for plugins that need it such as MetaTile and SceneStackEx, and **Starting SRAM bank** and **Save slot count** decide where they begin and how many there are. Under **Custom variable set** a file holds only the variables you list: 6 bytes plus 5 per variable. Under **All variables only** every file is 1,545 bytes.

---

<!-- BANK0:BEGIN -->
## Bank 0 (HOME) Usage

Bank 0 is the 16 KB fixed ROM bank shared by the GB Studio engine core, the
interrupt handlers and the GBDK runtime. Extra banked ROM is cheap to add,
bank 0 is not, so bank 0 is usually the first thing a project runs out of.

| | Bytes |
|---|---|
| Bank 0 used by this plugin | **0** |

**This plugin costs nothing in bank 0.** Everything it adds is compiled into a
switchable ROM bank.
<!-- BANK0:END -->

## Changelog

Grouped by the date each change was merged into the official
[gb-studio-plugins](https://github.com/gb-studio-dev/gb-studio-plugins) repository.

Only bug fixes, new features and feature changes are listed. Engine version
bumps, patch regeneration, packaging fixes and documentation edits are omitted.

### 2026-08-21

- Added the **Save structure** engine setting, offering Full save-state, All variables
  only and Custom variable set, as the single place a project says what a save file
  holds. The last two save variables and nothing else, so loading restores no scene, no
  actors and no running scripts.
- Added the **Starting SRAM bank** and **Save slot count** engine settings, and
  made the compatibility variants clamp the first bank up so save slots can
  never land on another plugin's SRAM.
- The plugin's save, load and check events now replace the built-in
  **Game Data Save**, **Game Data Load** and **If Game Data Saved** under the
  stock event IDs instead of sitting beside them as separate events. Existing
  scripts pick them up with no edits.
- They can now take a slot number or variable instead of the fixed three slots,
  as can the peek-by-index event.
- The main game loop now skips the fade out and scene reload on a load when the save
  holds no scene, instead of leaving a black screen.
- Added guards. A slot at or past the configured count, a save too large for one memory
  bank, or a slot landing past the last bank is now refused rather than overrunning the
  memory, and a typed-in slot number outside the count stops the build.
- Fixed a load in a project with a **Save configuration** event leaving the cartridge on
  the save memory bank when the file failed its size or identity check.

### 2026-06-14

- Added custom script parameter and stack support to the events.

### 2025-12-20

- Save files now start at save memory bank 1.

### 2025-10-29

- Fixed the plugin so that saving and loading actually work.

### 2025-02-24

- Initial release.
