#pragma bank 255

#include <string.h>

#include "load_save.h"

#include "system.h"
#include "actor.h"
#include "vm.h"
#include "events.h"
#include "music_manager.h"
#include "data_manager.h"
#include "data/game_globals.h"
#include "data/states_defines.h"

#include "rpg_data.h"
#include "rpg_ram.h"
#include "rpg_save.h"

#ifdef BATTERYLESS
    #include "bankdata.h"
    #include "flasher.h"
#endif

// ---------------------------------------------------------------------------
// Configure Load/Save engine settings (Settings > Engine > Configure Load/Save)
//
//   SAVE_STRUCTURE        what a save slot holds. FULL is GB Studio's own
//                         structure, scene and running scripts included; the
//                         other two hold variables and nothing else.
//   SAVE_SRAM_START_BANK  first SRAM bank the save slots are written to.
//   SAVE_SLOT_COUNT       how many save slots the game has.
// ---------------------------------------------------------------------------
#define SAVE_STRUCTURE_FULL      0
#define SAVE_STRUCTURE_VARIABLES 1
#define SAVE_STRUCTURE_CUSTOM    2

#ifndef SAVE_STRUCTURE
#define SAVE_STRUCTURE SAVE_STRUCTURE_FULL
#endif

#ifndef SAVE_SRAM_START_BANK
#define SAVE_SRAM_START_BANK 1
#endif
#ifndef SAVE_SLOT_COUNT
#define SAVE_SLOT_COUNT 3
#endif

// SRAM banks another plugin owns, counted from bank 0. RPG Maker 2000 holds
// its party and battle state in RPG_SRAM_BANK and nothing may share it, so the
// save slots start at the bank after that one however low "Starting SRAM bank"
// is set. That plugin refuses to build unless the bank is set high enough to
// clear anything else installed alongside it, so this covers those too.
//
// engineAlt-replaces: RPG_SAVE_FIRST_BANK -- that plugin's own macro for this,
// by another name. It was a file-local #define with two uses, mapping the bank
// in data_init and starting the slots in data_slot_address, and
// SAVE_SRAM_FIRST_BANK does both - never lower, since it is clamped up to the
// reserved count below.
#define SAVE_SRAM_RESERVED_BANKS (RPG_SRAM_BANK + 1)

// The starting bank is only a request: it is clamped up so a project can never
// be built with save slots sitting on top of another plugin's SRAM.
#if (SAVE_SRAM_START_BANK < SAVE_SRAM_RESERVED_BANKS)
#define SAVE_SRAM_FIRST_BANK SAVE_SRAM_RESERVED_BANKS
#else
#define SAVE_SRAM_FIRST_BANK SAVE_SRAM_START_BANK
#endif

// Slots are packed head to tail, so N slots can never span more than N banks.
// GB Studio links every ROM with 4 SRAM banks (-Wm-ya4), so banks 0..3 exist
// and that is where the slots have to stop whatever the settings ask for.
#if ((SAVE_SRAM_FIRST_BANK + SAVE_SLOT_COUNT) > 4)
#define SAVE_SRAM_END_BANK 4
#else
#define SAVE_SRAM_END_BANK (SAVE_SRAM_FIRST_BANK + SAVE_SLOT_COUNT)
#endif

// Does a save blob carry the VM contexts, and with them the scene the save was
// taken in? Only the full save-state structure does. Kept in step with
// src/core/core.c, which uses it to decide whether a load reloads the scene.
#if SAVE_STRUCTURE == SAVE_STRUCTURE_FULL
#define SAVE_KEEPS_CONTEXTS 1
#else
#define SAVE_KEEPS_CONTEXTS 0
#endif

// The custom structure is spelled out by a Save configuration event, which
// generates data/save_points.h when the project is built.
#if SAVE_STRUCTURE == SAVE_STRUCTURE_CUSTOM
#if !__has_include ("data/save_points.h")
#error Configure Load/Save: "Save structure" is set to "Custom variable set", but no Save configuration event was found. Add one to a script, or pick another save structure in Settings > Engine > Configure Load/Save.
#endif
#endif

#define SIGN_BY_PTR(ptr) *((UINT32 *)(ptr))
extern const UINT32 save_signature;

// RPG Maker 2000 keeps its state in its own SRAM bank, and a save slot is in
// another; the CPU maps only one at a time, so the party block travels through
// this buffer a chunk at a time. Saving is rare, so the bank thrashing costs
// nothing that matters.
#define RPG_BOUNCE_SIZE 64
static UBYTE rpg_bounce[RPG_BOUNCE_SIZE];

#if SAVE_STRUCTURE == SAVE_STRUCTURE_CUSTOM
#include "data/save_points.h"
#else

typedef struct save_point_t {
    void * target;
    size_t size;
    uint8_t id;
} save_point_t;

#define SAVEPOINT(A, ID) {&(A), sizeof(A), (ID)}
// Only the global variables, not the VM context stacks that sit behind them in
// script_memory[]: those belong to the scripts running right now, and the load
// happens from inside one of them.
#define SAVEPOINT_VARIABLES(ID) {script_memory, (VM_HEAP_SIZE * sizeof(UWORD)), (ID)}
#define SAVEPOINTS_END {0, 0}

extern uint16_t __rand_seed;

const save_point_t save_points[] = {
#if SAVE_STRUCTURE == SAVE_STRUCTURE_VARIABLES
    // variables and nothing else
    SAVEPOINT_VARIABLES(0),
#else
    // variables (must be first, need for peeking)
    SAVEPOINT(script_memory, 0),
    // VM contexts
    SAVEPOINT(CTXS, 1),
    SAVEPOINT(first_ctx, 2), SAVEPOINT(free_ctxs, 3), SAVEPOINT(old_executing_ctx, 4), SAVEPOINT(executing_ctx, 5), SAVEPOINT(vm_lock_state, 6),
    // intupt events
    SAVEPOINT(input_events, 7), SAVEPOINT(input_slots, 8),
    // timers
    SAVEPOINT(timer_events, 9), SAVEPOINT(timer_values, 10),
    // music
    SAVEPOINT(music_current_track_bank, 11),
    SAVEPOINT(music_current_track, 12),
    SAVEPOINT(music_events, 13),
    // scene
    SAVEPOINT(current_scene, 14), SAVEPOINT(scene_stack_ptr, 15), SAVEPOINT(scene_stack, 16), SAVEPOINT(scene_stack_count, 17),
    // actors
    SAVEPOINT(actors, 18),
    SAVEPOINT(actors_active_head, 19), SAVEPOINT(actors_inactive_head, 20), SAVEPOINT(player_moving, 21), SAVEPOINT(player_collision_actor, 22),
    // system
    SAVEPOINT(__rand_seed, 23),
#endif
    // terminator
    SAVEPOINTS_END
};
#endif

#ifdef BATTERYLESS
    extern void _start_save;
#endif

size_t save_blob_size;

// Copies rpg_persist_t between its own SRAM bank and a save slot, one chunk at
// a time. `save_data` points into the slot; `to_slot` picks the direction.
// Leaves RPG_SRAM_BANK mapped.
static void rpg_persist_copy(UBYTE *save_data, UBYTE data_bank, UBYTE to_slot) {
    UBYTE *persist = (UBYTE *)RPG_PERSIST;
    size_t remaining = sizeof(rpg_persist_t);

    while (remaining != 0) {
        size_t chunk = (remaining > RPG_BOUNCE_SIZE) ? RPG_BOUNCE_SIZE : remaining;

        if (to_slot) {
            SWITCH_RAM_BANK(RPG_SRAM_BANK, RAM_BANKS_ONLY);
            memcpy(rpg_bounce, persist, chunk);
            SWITCH_RAM_BANK(data_bank, RAM_BANKS_ONLY);
            memcpy(save_data, rpg_bounce, chunk);
        } else {
            SWITCH_RAM_BANK(data_bank, RAM_BANKS_ONLY);
            memcpy(rpg_bounce, save_data, chunk);
            SWITCH_RAM_BANK(RPG_SRAM_BANK, RAM_BANKS_ONLY);
            memcpy(persist, rpg_bounce, chunk);
        }

        persist += chunk;
        save_data += chunk;
        remaining -= chunk;
    }
    SWITCH_RAM_BANK(RPG_SRAM_BANK, RAM_BANKS_ONLY);
}

void data_init(void) BANKED {
    ENABLE_RAM_MBC5;
    SWITCH_RAM_BANK(SAVE_SRAM_FIRST_BANK, RAM_BANKS_ONLY);
    // calculate save blob size
    save_blob_size = sizeof(save_signature) + sizeof(save_blob_size);
    #if SAVE_STRUCTURE == SAVE_STRUCTURE_CUSTOM
    save_point_t point_ref;
    const save_point_t * point_ptr = save_points;
    MemcpyBanked(&point_ref, point_ptr, sizeof(save_point_t), BANK(save_points));
    while(point_ref.target){
        save_blob_size += sizeof(point_ref.size) + sizeof(point_ref.id) + point_ref.size;
        point_ptr++;
        MemcpyBanked(&point_ref, point_ptr, sizeof(save_point_t), BANK(save_points));
    }
    #else
    for(const save_point_t * point = save_points; (point->target); point++) {
        save_blob_size += sizeof(point->size) + sizeof(point->id) + point->size;
    }
    #endif
    save_blob_size += sizeof(rpg_persist_t);
#ifdef BATTERYLESS
    // load from FLASH ROM
    for (UBYTE i = 0; i < SAVE_SRAM_END_BANK; i++) restore_sram_bank(i);
#endif
    SWITCH_RAM_BANK(0, RAM_BANKS_ONLY);
}

UBYTE * data_slot_address(UBYTE slot, UBYTE *bank) {
    UWORD res = 0;
    UBYTE res_bank = SAVE_SRAM_FIRST_BANK;
    // a slot the settings do not give the game
    if (slot >= SAVE_SLOT_COUNT) return NULL;
    // A blob that does not fit inside one SRAM bank has nowhere to go: writing
    // it would run off the end of the bank and trash whatever the next one
    // holds, so refuse the slot instead.
    if ((save_blob_size == 0) || (save_blob_size > SRAM_BANK_SIZE)) return NULL;
    for (UBYTE i = 0; i < slot; i++) {
        res += save_blob_size;
        if ((res + save_blob_size) > SRAM_BANK_SIZE) {
            if (++res_bank >= SAVE_SRAM_END_BANK) return NULL;
            res = 0;
        }
    }
    *bank = res_bank;
    return (UBYTE *)0xA000u + res;
}

void data_save(UBYTE slot) BANKED {
    UBYTE data_bank, *save_data = data_slot_address(slot, &data_bank);
    if (save_data == NULL) return;
    SWITCH_RAM_BANK(data_bank, RAM_BANKS_ONLY);

    // signature
    SIGN_BY_PTR(save_data) = save_signature;
    save_data += sizeof(save_signature);
    // size of the save blob
    *(size_t*)save_data = save_blob_size;
    save_data += sizeof(save_blob_size);
    #if SAVE_STRUCTURE == SAVE_STRUCTURE_CUSTOM
    save_point_t point_ref;
    const save_point_t * point_ptr = save_points;
    MemcpyBanked(&point_ref, point_ptr, sizeof(save_point_t), BANK(save_points));
    while(point_ref.target){
        // size of the block
        *(size_t*)save_data = point_ref.size;
        save_data += sizeof(point_ref.size);
        // ID of the block
        *(uint8_t*)save_data = point_ref.id;
        save_data += sizeof(point_ref.id);
        // block data
        memcpy(save_data, point_ref.target, point_ref.size);
        save_data += point_ref.size;
        point_ptr++;
        MemcpyBanked(&point_ref, point_ptr, sizeof(save_point_t), BANK(save_points));
    }
    #else
    for(const save_point_t * point = save_points; (point->target); point++) {
        // size of the block
        *(size_t*)save_data = point->size;
        save_data += sizeof(point->size);
        // ID of the block
        *(uint8_t*)save_data = point->id;
        save_data += sizeof(point->id);
        // block data
        memcpy(save_data, point->target, point->size);
        save_data += point->size;
    }
    #endif
    // party state, copied across banks
    rpg_persist_copy(save_data, data_bank, TRUE);
#ifdef BATTERYLESS
    // save to FLASH ROM
    SWITCH_RAM_BANK(data_bank, RAM_BANKS_ONLY);
    save_sram(SAVE_SRAM_END_BANK);
#endif
    SWITCH_RAM_BANK(0, RAM_BANKS_ONLY);
}

UBYTE data_load(UBYTE slot) BANKED {
    UBYTE data_bank, *save_data = data_slot_address(slot, &data_bank);
    if (save_data == NULL) return FALSE;
    SWITCH_RAM_BANK(data_bank, RAM_BANKS_ONLY);
    if (SIGN_BY_PTR(save_data) != save_signature){
        SWITCH_RAM_BANK(0, RAM_BANKS_ONLY);
        return FALSE;
    }
    // seek to the first block
    save_data += sizeof(save_signature) + sizeof(save_blob_size);
    // load blocks
    #if SAVE_STRUCTURE == SAVE_STRUCTURE_CUSTOM
    save_point_t point_ref;
    const save_point_t * point_ptr = save_points;
    MemcpyBanked(&point_ref, point_ptr, sizeof(save_point_t), BANK(save_points));
    while(point_ref.target){
        // check chunk size
        if (*(size_t*)save_data != point_ref.size){
            SWITCH_RAM_BANK(0, RAM_BANKS_ONLY);
            return FALSE;
        } else {
            save_data += sizeof(point_ref.size);
        }
        // check chunk id
        if (*(uint8_t*)save_data != point_ref.id){
            SWITCH_RAM_BANK(0, RAM_BANKS_ONLY);
            return FALSE;
        } else {
            save_data += sizeof(point_ref.id);
        }
        // copy chunk data
        memcpy(point_ref.target, save_data, point_ref.size);
        save_data += point_ref.size;
        point_ptr++;
        MemcpyBanked(&point_ref, point_ptr, sizeof(save_point_t), BANK(save_points));
    }
    #else
    for(const save_point_t * point = save_points; (point->target); point++) {
        // check chunk size
        if (*(size_t*)save_data != point->size){
            SWITCH_RAM_BANK(0, RAM_BANKS_ONLY);
            return FALSE;
        } else {
            save_data += sizeof(point->size);
        }
        // check chunk id
        if (*(uint8_t*)save_data != point->id){
            SWITCH_RAM_BANK(0, RAM_BANKS_ONLY);
            return FALSE;
        } else {
            save_data += sizeof(point->id);
        }
        // copy chunk data
        memcpy(point->target, save_data, point->size);
        save_data += point->size;
    }
    #endif
    // party state, copied across banks
    rpg_persist_copy(save_data, data_bank, FALSE);
    // the cached database records may describe the previous game's party
    rpg_data_invalidate();
    SWITCH_RAM_BANK(0, RAM_BANKS_ONLY);
#if SAVE_KEEPS_CONTEXTS
    // Restart music
    if (music_current_track_bank != MUSIC_STOP_BANK) {
        music_next_track = music_current_track;
    } else {
        music_sound_cut();
    }
#endif
    return TRUE;
}

void data_clear(UBYTE slot) BANKED {
    UBYTE data_bank, *save_data = data_slot_address(slot, &data_bank);
    if (save_data == NULL) return;
    SWITCH_RAM_BANK(data_bank, RAM_BANKS_ONLY);
    SIGN_BY_PTR(save_data) = 0;
#ifdef BATTERYLESS
    // save to FLASH ROM
    save_sram(SAVE_SRAM_END_BANK);
#endif
    SWITCH_RAM_BANK(0, RAM_BANKS_ONLY);
}

UBYTE data_peek(UBYTE slot, UINT16 idx, UWORD count, UINT16 * dest) BANKED {
    UBYTE data_bank, *save_data = data_slot_address(slot, &data_bank);
    if (save_data == NULL) return FALSE;
    SWITCH_RAM_BANK(data_bank, RAM_BANKS_ONLY);
    if (SIGN_BY_PTR(save_data) != save_signature){
        SWITCH_RAM_BANK(0, RAM_BANKS_ONLY);
        return FALSE;
    }
    if (count) memcpy(dest, save_data + (sizeof(save_signature) + sizeof(save_blob_size) + sizeof(size_t) + sizeof(uint8_t)) + (idx << 1), count << 1);
    SWITCH_RAM_BANK(0, RAM_BANKS_ONLY);
    return TRUE;
}

UBYTE data_peek_ex(UBYTE slot, UINT16 idx, UWORD count, UINT16 * dest) BANKED {
    UBYTE data_bank, *save_data = data_slot_address(slot, &data_bank);
    if (save_data == NULL) return FALSE;
    SWITCH_RAM_BANK(data_bank, RAM_BANKS_ONLY);
    if (SIGN_BY_PTR(save_data) != save_signature){
        SWITCH_RAM_BANK(0, RAM_BANKS_ONLY);
        return FALSE;
    }
#if SAVE_STRUCTURE == SAVE_STRUCTURE_CUSTOM
    // one block per configured variable: idx is the save point index
    if (count) memcpy(dest, save_data + (sizeof(save_signature) + sizeof(save_blob_size) + (((idx + 1) * (sizeof(size_t) + sizeof(uint8_t) + sizeof(int16_t))) - sizeof(int16_t))), count << 1);
#else
    // the first block holds every variable: idx is the variable index
    if (count) memcpy(dest, save_data + (sizeof(save_signature) + sizeof(save_blob_size) + sizeof(size_t) + sizeof(uint8_t)) + (idx << 1), count << 1);
#endif
    SWITCH_RAM_BANK(0, RAM_BANKS_ONLY);
    return TRUE;
}

void vm_data_peek_ex(SCRIPT_CTX * THIS) OLDCALL BANKED {
    int16_t idx = *(int16_t*)VM_REF_TO_PTR(FN_ARG3);
    int16_t * A;
    if (idx < 0) A = THIS->stack_ptr + idx - 4; else A = script_memory + idx;
    data_peek_ex(*(uint8_t *)VM_REF_TO_PTR(FN_ARG0), *(uint16_t *)VM_REF_TO_PTR(FN_ARG1), *(uint16_t *)VM_REF_TO_PTR(FN_ARG2), A);
}

void vm_data_check_ex(SCRIPT_CTX * THIS) OLDCALL BANKED {
    int16_t idx = *(int16_t*)VM_REF_TO_PTR(FN_ARG1);
    int16_t * A;
    if (idx < 0) A = THIS->stack_ptr + idx - 2; else A = script_memory + idx;
    // count 0: no data is copied, only the slot and its signature are checked
    *A = data_peek_ex(*(uint8_t *)VM_REF_TO_PTR(FN_ARG0), 0, 0, NULL);
}

void vm_data_save_ex(SCRIPT_CTX * THIS) OLDCALL BANKED {
    data_save(*(uint8_t *)VM_REF_TO_PTR(FN_ARG0));
}

void vm_data_load_ex(SCRIPT_CTX * THIS) OLDCALL BANKED {
    data_load(*(uint8_t *)VM_REF_TO_PTR(FN_ARG0));
}

// ---------------------------------------------------------------------------
// Looking into a slot (RPG Maker 2000)
//
// A save menu has to show what is in a file before deciding whether to open
// it, and none of that is in the variables: the party block is written after
// them, which is why data_peek cannot reach it. This walks past the blocks to
// the rpg_persist_t copy and reads the few fields a file row shows.
// ---------------------------------------------------------------------------

UWORD rpg_save_peek(UBYTE slot, UBYTE field, UBYTE index) BANKED {
    UBYTE data_bank, *save_data = data_slot_address(slot, &data_bank);
    if (save_data == NULL) return 0;
    SWITCH_RAM_BANK(data_bank, RAM_BANKS_ONLY);

    if (SIGN_BY_PTR(save_data) != save_signature) {
        SWITCH_RAM_BANK(0, RAM_BANKS_ONLY);
        return 0;
    }
    if (field == RPG_SAVE_FIELD_USED) {
        SWITCH_RAM_BANK(0, RAM_BANKS_ONLY);
        return 1;
    }

    // Past the header, then past every block the slot holds, exactly as
    // data_save wrote them - whichever structure the settings chose.
    save_data += sizeof(save_signature) + sizeof(save_blob_size);
    #if SAVE_STRUCTURE == SAVE_STRUCTURE_CUSTOM
    save_point_t point_ref;
    const save_point_t * point_ptr = save_points;
    MemcpyBanked(&point_ref, point_ptr, sizeof(save_point_t), BANK(save_points));
    while (point_ref.target) {
        save_data += sizeof(point_ref.size) + sizeof(point_ref.id) + point_ref.size;
        point_ptr++;
        MemcpyBanked(&point_ref, point_ptr, sizeof(save_point_t), BANK(save_points));
    }
    #else
    for (const save_point_t *point = save_points; (point->target); point++) {
        save_data += sizeof(point->size) + sizeof(point->id) + point->size;
    }
    #endif

    // The party block is a byte-for-byte copy of rpg_persist_t, so it can be
    // read through one. The pointer is a runtime address rather than a literal
    // one, which is what keeps SDCC's optimiser out of the ditch it falls into
    // with a struct at a constant address.
    rpg_persist_t *saved = (rpg_persist_t *)save_data;
    UWORD result = 0;

    switch (field) {
        case RPG_SAVE_FIELD_PARTY_COUNT:
            result = saved->party_count;
            break;
        case RPG_SAVE_FIELD_MONEY:
            result = saved->money;
            break;
        default:
            if (index < saved->party_count) {
                UBYTE hero_idx = saved->party[index];
                switch (field) {
                    case RPG_SAVE_FIELD_HERO: result = hero_idx; break;
                    case RPG_SAVE_FIELD_LEVEL: result = saved->heroes[hero_idx].level; break;
                    case RPG_SAVE_FIELD_HP: result = saved->heroes[hero_idx].current_hp; break;
                    default: break;
                }
            } else if (field == RPG_SAVE_FIELD_HERO) {
                // 0xFF rather than 0, so an empty position cannot be mistaken
                // for hero 0 - the same answer the party getter gives.
                result = 0xFFu;
            }
            break;
    }

    SWITCH_RAM_BANK(0, RAM_BANKS_ONLY);
    return result;
}
