#include <assert.h>
#include <stdint.h>

#include "ambient_reducer.h"

static ambient_event_t event(ambient_event_kind_t kind,
                             ambient_key_t session,
                             ambient_key_t turn,
                             ambient_key_t sequence)
{
    ambient_event_t value = {
        .kind = kind,
        .session_key = session,
        .turn_key = turn,
        .event_key = sequence,
    };
    return value;
}

int main(void)
{
    ambient_reducer_t reducer;
    ambient_reducer_t single_slot_reducer;
    ambient_session_slot_t slots[3];
    ambient_session_slot_t single_slot;
    ambient_dashboard_snapshot_t snapshot;
    ambient_session_view_t view;
    ambient_session_view_t single_slot_view;
    ambient_event_t value;

    assert(!ambient_reducer_init(&reducer, slots, 0, 100, 20));
    assert(!ambient_reducer_init(&reducer, slots, SIZE_MAX, 100, 20));
    assert(ambient_reducer_init(&reducer, slots, 3, 100, 20));
    assert(ambient_reducer_apply(&reducer, NULL, 0)
           == AMBIENT_REDUCER_INVALID);

    value = event(AMBIENT_EVENT_TURN_STARTED, 11, 101, 1);
    assert(ambient_reducer_apply(&reducer, &value, 1000)
           == AMBIENT_REDUCER_APPLIED);
    value = event(AMBIENT_EVENT_TURN_STARTED, 22, 201, 1);
    assert(ambient_reducer_apply(&reducer, &value, 1001)
           == AMBIENT_REDUCER_APPLIED);
    assert(ambient_reducer_snapshot(&reducer, 1001, &snapshot));
    assert(snapshot.status == AMBIENT_STATUS_WORKING);
    assert(snapshot.fresh_session_count == 2);
    assert(snapshot.working_session_count == 2);

    value = event(AMBIENT_EVENT_TURN_SUCCEEDED, 11, 101, 2);
    assert(ambient_reducer_apply(&reducer, &value, 1010)
           == AMBIENT_REDUCER_APPLIED);
    assert(ambient_reducer_get_session(&reducer, 11, 1010, &view, NULL));
    assert(view.status == AMBIENT_STATUS_DONE);
    assert(view.last_outcome == AMBIENT_OUTCOME_SUCCESS);
    assert(ambient_reducer_snapshot(&reducer, 1010, &snapshot));
    assert(snapshot.status == AMBIENT_STATUS_WORKING);
    assert(snapshot.done_session_count == 1);
    assert(snapshot.working_session_count == 1);

    value = event(AMBIENT_EVENT_TURN_SUCCEEDED, 11, 101, 2);
    assert(ambient_reducer_apply(&reducer, &value, 1020)
           == AMBIENT_REDUCER_REPLAY);
    value = event(AMBIENT_EVENT_TURN_SUCCEEDED, 11, 999, 3);
    assert(ambient_reducer_apply(&reducer, &value, 1020)
           == AMBIENT_REDUCER_STALE_TURN);
    value = event(AMBIENT_EVENT_TURN_FAILED, 11, 101, 3);
    assert(ambient_reducer_apply(&reducer, &value, 1020)
           == AMBIENT_REDUCER_STALE_TURN);

    value = event(AMBIENT_EVENT_TURN_FAILED, 22, 201, 2);
    assert(ambient_reducer_get_session(&reducer, 11, 1029, &view, NULL));
    assert(view.status == AMBIENT_STATUS_DONE);
    assert(ambient_reducer_get_session(&reducer, 11, 1030, &view, NULL));
    assert(view.status == AMBIENT_STATUS_IDLE);
    assert(ambient_reducer_apply(&reducer, &value, 1030)
           == AMBIENT_REDUCER_APPLIED);
    assert(ambient_reducer_get_session(&reducer, 22, 1030, &view, NULL));
    assert(view.status == AMBIENT_STATUS_IDLE);
    assert(view.last_outcome == AMBIENT_OUTCOME_FAILED);

    value = event(AMBIENT_EVENT_TURN_STARTED, 22, 202, 3);
    assert(ambient_reducer_apply(&reducer, &value, 1040)
           == AMBIENT_REDUCER_APPLIED);
    value = event(AMBIENT_EVENT_TURN_ABORTED, 22, 202, 4);
    assert(ambient_reducer_apply(&reducer, &value, 1050)
           == AMBIENT_REDUCER_APPLIED);
    assert(ambient_reducer_get_session(&reducer, 22, 1050, &view, NULL));
    assert(view.status == AMBIENT_STATUS_IDLE);
    assert(view.last_outcome == AMBIENT_OUTCOME_ABORTED);

    value = event(AMBIENT_EVENT_ATTENTION_REQUIRED, 33, 0, 1);
    assert(ambient_reducer_apply(&reducer, &value, 1060)
           == AMBIENT_REDUCER_APPLIED);
    assert(ambient_reducer_snapshot(&reducer, 1060, &snapshot));
    assert(snapshot.status == AMBIENT_STATUS_ATTENTION);
    value = event(AMBIENT_EVENT_SESSION_IDLE, 44, 0, 1);
    assert(ambient_reducer_apply(&reducer, &value, 1060)
           == AMBIENT_REDUCER_NO_CAPACITY);

    value = event(AMBIENT_EVENT_ATTENTION_CLEARED, 33, 0, 2);
    assert(ambient_reducer_apply(&reducer, &value, 1061)
           == AMBIENT_REDUCER_APPLIED);
    assert(ambient_reducer_get_session(&reducer, 33, 1061, &view, NULL));
    assert(view.status == AMBIENT_STATUS_IDLE);
    value = event(AMBIENT_EVENT_TURN_STARTED, 33, 331, 3);
    assert(ambient_reducer_apply(&reducer, &value, 1062)
           == AMBIENT_REDUCER_APPLIED);
    value = event(AMBIENT_EVENT_ATTENTION_REQUIRED, 33, 0, 4);
    assert(ambient_reducer_apply(&reducer, &value, 1063)
           == AMBIENT_REDUCER_APPLIED);
    assert(ambient_reducer_get_session(&reducer, 33, 1063, &view, NULL));
    assert(view.status == AMBIENT_STATUS_ATTENTION);
    assert(view.active_turn_key == 331);
    value = event(AMBIENT_EVENT_ATTENTION_CLEARED, 33, 0, 5);
    assert(ambient_reducer_apply(&reducer, &value, 1064)
           == AMBIENT_REDUCER_APPLIED);
    assert(ambient_reducer_get_session(&reducer, 33, 1064, &view, NULL));
    assert(view.status == AMBIENT_STATUS_WORKING);
    assert(view.active_turn_key == 331);
    value = event(AMBIENT_EVENT_ATTENTION_REQUIRED, 33, 0, 6);
    assert(ambient_reducer_apply(&reducer, &value, 1065)
           == AMBIENT_REDUCER_APPLIED);
    value = event(AMBIENT_EVENT_TURN_SUCCEEDED, 33, 331, 7);
    assert(ambient_reducer_apply(&reducer, &value, 1066)
           == AMBIENT_REDUCER_APPLIED);
    assert(ambient_reducer_get_session(&reducer, 33, 1066, &view, NULL));
    assert(view.status == AMBIENT_STATUS_DONE);

    value = event(AMBIENT_EVENT_TURN_STARTED, 33, 332, 8);
    assert(ambient_reducer_apply(&reducer, &value, 1067)
           == AMBIENT_REDUCER_APPLIED);
    value = event(AMBIENT_EVENT_ATTENTION_REQUIRED, 33, 0, 9);
    assert(ambient_reducer_apply(&reducer, &value, 1068)
           == AMBIENT_REDUCER_APPLIED);
    value = event(AMBIENT_EVENT_TURN_FAILED, 33, 332, 10);
    assert(ambient_reducer_apply(&reducer, &value, 1069)
           == AMBIENT_REDUCER_APPLIED);
    assert(ambient_reducer_get_session(&reducer, 33, 1069, &view, NULL));
    assert(view.status == AMBIENT_STATUS_IDLE);
    assert(view.last_outcome == AMBIENT_OUTCOME_FAILED);

    value = event(AMBIENT_EVENT_TURN_STARTED, 33, 333, 11);
    assert(ambient_reducer_apply(&reducer, &value, 1070)
           == AMBIENT_REDUCER_APPLIED);
    value = event(AMBIENT_EVENT_ATTENTION_REQUIRED, 33, 0, 12);
    assert(ambient_reducer_apply(&reducer, &value, 1071)
           == AMBIENT_REDUCER_APPLIED);
    value = event(AMBIENT_EVENT_TURN_ABORTED, 33, 333, 13);
    assert(ambient_reducer_apply(&reducer, &value, 1072)
           == AMBIENT_REDUCER_APPLIED);
    assert(ambient_reducer_get_session(&reducer, 33, 1072, &view, NULL));
    assert(view.status == AMBIENT_STATUS_IDLE);
    assert(view.last_outcome == AMBIENT_OUTCOME_ABORTED);

    /* Freshness stays inclusive at the limit; a regressed clock is clamped. */
    assert(ambient_reducer_get_session(&reducer, 11, 1110, &view, NULL));
    assert(view.status == AMBIENT_STATUS_IDLE);
    assert(ambient_reducer_snapshot(&reducer, 1109, &snapshot));
    assert(snapshot.clock_regressed);
    assert(snapshot.status == AMBIENT_STATUS_IDLE);

    /* Session freshness is separate from the DONE hold period. */
    assert(ambient_reducer_get_session(&reducer, 11, 1120, &view, NULL));
    assert(view.status == AMBIENT_STATUS_OFFLINE);
    assert(ambient_reducer_get_session(&reducer, 33, 1172, &view, NULL));
    assert(view.status == AMBIENT_STATUS_IDLE);
    assert(ambient_reducer_get_session(&reducer, 33, 1173, &view, NULL));
    assert(view.status == AMBIENT_STATUS_OFFLINE);

    value = event(AMBIENT_EVENT_SESSION_IDLE, 44, 1, 1);
    assert(ambient_reducer_apply(&reducer, &value, 1162)
           == AMBIENT_REDUCER_INVALID);
    value = event(AMBIENT_EVENT_TURN_STARTED, 44, 0, 1);
    assert(ambient_reducer_apply(&reducer, &value, 1162)
           == AMBIENT_REDUCER_INVALID);

    assert(ambient_reducer_init(&single_slot_reducer, &single_slot, 1, 10, 5));
    value = event(AMBIENT_EVENT_TURN_STARTED, 100, 1000, 100);
    assert(ambient_reducer_apply(&single_slot_reducer, &value, 0)
           == AMBIENT_REDUCER_APPLIED);
    value = event(AMBIENT_EVENT_TURN_STARTED, 200, 2000, 1);
    assert(ambient_reducer_apply(&single_slot_reducer, &value, 11)
           == AMBIENT_REDUCER_APPLIED);
    assert(ambient_reducer_get_session(&single_slot_reducer, 200, 11,
                                       &single_slot_view, NULL));
    assert(single_slot_view.status == AMBIENT_STATUS_WORKING);
    assert(single_slot_view.active_turn_key == 2000);
    return 0;
}
