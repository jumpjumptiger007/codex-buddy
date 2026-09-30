#include "ambient_reducer.h"

#include <stdint.h>
#include <string.h>

static uint64_t ambient_reducer_normalize_now(ambient_reducer_t *reducer,
                                              uint64_t now_ms,
                                              bool *clock_regressed)
{
    *clock_regressed = reducer->has_now && now_ms < reducer->last_now_ms;
    if (*clock_regressed) {
        return reducer->last_now_ms;
    }

    reducer->last_now_ms = now_ms;
    reducer->has_now = true;
    return now_ms;
}

static bool ambient_event_uses_turn(ambient_event_kind_t kind)
{
    return kind == AMBIENT_EVENT_TURN_STARTED
        || kind == AMBIENT_EVENT_TURN_SUCCEEDED
        || kind == AMBIENT_EVENT_TURN_FAILED
        || kind == AMBIENT_EVENT_TURN_ABORTED;
}

static bool ambient_event_is_valid(const ambient_event_t *event)
{
    if (!event || event->session_key == 0 || event->event_key == 0) {
        return false;
    }

    if (event->kind <= AMBIENT_EVENT_INVALID
        || event->kind > AMBIENT_EVENT_ATTENTION_CLEARED) {
        return false;
    }

    return ambient_event_uses_turn(event->kind)
         ? event->turn_key != 0
         : event->turn_key == 0;
}

static bool ambient_session_is_stale(const ambient_reducer_t *reducer,
                                     const ambient_session_slot_t *slot,
                                     uint64_t now_ms)
{
    return now_ms >= slot->last_seen_ms
        && now_ms - slot->last_seen_ms > reducer->freshness_ms;
}

static ambient_session_slot_t *ambient_reducer_find_slot(
    ambient_reducer_t *reducer,
    ambient_key_t session_key,
    uint64_t now_ms)
{
    ambient_session_slot_t *free_slot = NULL;
    ambient_session_slot_t *oldest_stale_slot = NULL;

    for (size_t i = 0; i < reducer->slot_capacity; ++i) {
        ambient_session_slot_t *slot = &reducer->slots[i];
        if (slot->occupied && slot->session_key == session_key) {
            return slot;
        }
        if (!slot->occupied && !free_slot) {
            free_slot = slot;
        }
        if (slot->occupied && ambient_session_is_stale(reducer, slot, now_ms)
            && (!oldest_stale_slot
                || slot->last_seen_ms < oldest_stale_slot->last_seen_ms)) {
            oldest_stale_slot = slot;
        }
    }

    if (free_slot) {
        return free_slot;
    }
    return oldest_stale_slot;
}

static ambient_status_t ambient_session_effective_status(
    const ambient_reducer_t *reducer,
    const ambient_session_slot_t *slot,
    uint64_t now_ms)
{
    if (!slot->occupied || ambient_session_is_stale(reducer, slot, now_ms)) {
        return AMBIENT_STATUS_OFFLINE;
    }
    if (slot->status == AMBIENT_STATUS_DONE
        && now_ms >= slot->done_at_ms
        && now_ms - slot->done_at_ms >= reducer->done_hold_ms) {
        return AMBIENT_STATUS_IDLE;
    }
    return slot->status;
}

bool ambient_reducer_init(ambient_reducer_t *reducer,
                          ambient_session_slot_t *slots,
                          size_t slot_capacity,
                          uint64_t freshness_ms,
                          uint64_t done_hold_ms)
{
    if (!reducer || !slots || slot_capacity == 0 || freshness_ms == 0
        || slot_capacity > SIZE_MAX / sizeof(*slots)) {
        return false;
    }

    memset(slots, 0, sizeof(*slots) * slot_capacity);
    reducer->slots = slots;
    reducer->slot_capacity = slot_capacity;
    reducer->freshness_ms = freshness_ms;
    reducer->done_hold_ms = done_hold_ms;
    reducer->last_now_ms = 0;
    reducer->has_now = false;
    return true;
}

ambient_reducer_result_t ambient_reducer_apply(ambient_reducer_t *reducer,
                                               const ambient_event_t *event,
                                               uint64_t now_ms)
{
    bool clock_regressed;
    uint64_t effective_now;
    ambient_session_slot_t *slot;
    bool same_session;

    if (!reducer || !reducer->slots || !ambient_event_is_valid(event)) {
        return AMBIENT_REDUCER_INVALID;
    }

    effective_now = ambient_reducer_normalize_now(reducer, now_ms,
                                                  &clock_regressed);
    (void)clock_regressed;
    slot = ambient_reducer_find_slot(reducer, event->session_key,
                                     effective_now);
    if (!slot) {
        return AMBIENT_REDUCER_NO_CAPACITY;
    }
    same_session = slot->occupied
        && slot->session_key == event->session_key;
    if (same_session && event->event_key <= slot->last_event_key) {
        return AMBIENT_REDUCER_REPLAY;
    }

    if (event->kind == AMBIENT_EVENT_TURN_SUCCEEDED
        || event->kind == AMBIENT_EVENT_TURN_FAILED
        || event->kind == AMBIENT_EVENT_TURN_ABORTED) {
        if (!same_session || slot->active_turn_key != event->turn_key) {
            return AMBIENT_REDUCER_STALE_TURN;
        }
    }

    if (!same_session) {
        memset(slot, 0, sizeof(*slot));
        slot->occupied = true;
        slot->session_key = event->session_key;
        slot->status = AMBIENT_STATUS_IDLE;
    }

    slot->last_event_key = event->event_key;
    slot->last_seen_ms = effective_now;

    switch (event->kind) {
    case AMBIENT_EVENT_SESSION_IDLE:
        slot->active_turn_key = 0;
        slot->status = AMBIENT_STATUS_IDLE;
        break;
    case AMBIENT_EVENT_TURN_STARTED:
        slot->active_turn_key = event->turn_key;
        slot->last_outcome = AMBIENT_OUTCOME_NONE;
        slot->status = AMBIENT_STATUS_WORKING;
        break;
    case AMBIENT_EVENT_TURN_SUCCEEDED:
        slot->active_turn_key = 0;
        slot->last_outcome = AMBIENT_OUTCOME_SUCCESS;
        slot->done_at_ms = effective_now;
        slot->status = AMBIENT_STATUS_DONE;
        break;
    case AMBIENT_EVENT_TURN_FAILED:
        slot->active_turn_key = 0;
        slot->last_outcome = AMBIENT_OUTCOME_FAILED;
        slot->status = AMBIENT_STATUS_IDLE;
        break;
    case AMBIENT_EVENT_TURN_ABORTED:
        slot->active_turn_key = 0;
        slot->last_outcome = AMBIENT_OUTCOME_ABORTED;
        slot->status = AMBIENT_STATUS_IDLE;
        break;
    case AMBIENT_EVENT_ATTENTION_REQUIRED:
        slot->status = AMBIENT_STATUS_ATTENTION;
        break;
    case AMBIENT_EVENT_ATTENTION_CLEARED:
        slot->status = slot->active_turn_key != 0
            ? AMBIENT_STATUS_WORKING : AMBIENT_STATUS_IDLE;
        break;
    case AMBIENT_EVENT_INVALID:
    default:
        return AMBIENT_REDUCER_INVALID;
    }

    return AMBIENT_REDUCER_APPLIED;
}

bool ambient_reducer_get_session(ambient_reducer_t *reducer,
                                 ambient_key_t session_key,
                                 uint64_t now_ms,
                                 ambient_session_view_t *view,
                                 bool *clock_regressed)
{
    bool regressed;
    uint64_t effective_now;

    if (!reducer || !reducer->slots || session_key == 0 || !view) {
        return false;
    }

    effective_now = ambient_reducer_normalize_now(reducer, now_ms, &regressed);
    for (size_t i = 0; i < reducer->slot_capacity; ++i) {
        const ambient_session_slot_t *slot = &reducer->slots[i];
        if (slot->occupied && slot->session_key == session_key) {
            view->session_key = slot->session_key;
            view->active_turn_key = slot->active_turn_key;
            view->status = ambient_session_effective_status(reducer, slot,
                                                            effective_now);
            view->last_outcome = slot->last_outcome;
            view->last_seen_ms = slot->last_seen_ms;
            if (clock_regressed) {
                *clock_regressed = regressed;
            }
            return true;
        }
    }
    if (clock_regressed) {
        *clock_regressed = regressed;
    }
    return false;
}

bool ambient_reducer_snapshot(ambient_reducer_t *reducer,
                              uint64_t now_ms,
                              ambient_dashboard_snapshot_t *snapshot)
{
    bool regressed;
    uint64_t effective_now;
    bool any_idle = false;

    if (!reducer || !reducer->slots || !snapshot) {
        return false;
    }

    effective_now = ambient_reducer_normalize_now(reducer, now_ms, &regressed);
    memset(snapshot, 0, sizeof(*snapshot));
    snapshot->status = AMBIENT_STATUS_OFFLINE;
    snapshot->clock_regressed = regressed;

    for (size_t i = 0; i < reducer->slot_capacity; ++i) {
        const ambient_session_slot_t *slot = &reducer->slots[i];
        ambient_status_t status;
        if (!slot->occupied) {
            continue;
        }
        status = ambient_session_effective_status(reducer, slot, effective_now);
        if (status == AMBIENT_STATUS_OFFLINE) {
            continue;
        }
        snapshot->fresh_session_count++;
        switch (status) {
        case AMBIENT_STATUS_ATTENTION:
            snapshot->attention_session_count++;
            break;
        case AMBIENT_STATUS_WORKING:
            snapshot->working_session_count++;
            break;
        case AMBIENT_STATUS_DONE:
            snapshot->done_session_count++;
            break;
        case AMBIENT_STATUS_IDLE:
            any_idle = true;
            break;
        case AMBIENT_STATUS_OFFLINE:
        default:
            break;
        }
    }

    if (snapshot->attention_session_count > 0) {
        snapshot->status = AMBIENT_STATUS_ATTENTION;
    } else if (snapshot->working_session_count > 0) {
        snapshot->status = AMBIENT_STATUS_WORKING;
    } else if (snapshot->done_session_count > 0) {
        snapshot->status = AMBIENT_STATUS_DONE;
    } else if (any_idle) {
        snapshot->status = AMBIENT_STATUS_IDLE;
    }
    return true;
}
