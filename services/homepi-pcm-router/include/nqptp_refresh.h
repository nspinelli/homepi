#pragma once

/**
 * Sends a delayed NQPTP clock refresh for a promoted zone after another zone drops.
 * Re-registers play-active and timing peer after a departing zone clears shared state.
 * @param zone_id Promoted owner zone id (1–16).
 * @param client_ip AirPlay client IP from zone metadata (may be empty).
 * @param delay_ms Delay before sending, to run after departing zone teardown.
 */
void nqptp_schedule_owner_handoff_refresh(int zone_id, const char* client_ip, unsigned int delay_ms);
