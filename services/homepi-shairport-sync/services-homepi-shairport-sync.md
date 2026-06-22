# HomePi Shairport Sync

Shairport Sync is the program used to enable airplay2 capabilities for the audio feature. Each audio zone in the [homepi-hifi-serial](../homepi-hifi-serial/) will have its own unique configuration file to create multiple devices on this single device. The [homepi-pcm-router](../homepi-pcm-router/) will route the PCM to a single DAC output that the user specifies. The [homepi-metadata](../homepi-metadata/) is responsible for parsing all of the meta data from the streams. 

## Requirements
- Must use HomePi [core](../../core/) utilities
- Each zone in the controller will have its own Shairport Sync configuration file.
- Reliant on the homepi-serial service and intial loads data from the database. If there is nothing in the database remain offline untill there the controller syncs with database.



## Configuration Requirements

**From Controller with Update:** Indicated that the value is populated based on the value from the controller. If the user edits the UI for this variable it must be updated in the controller, database and its respected configuration file.

**From Controller:** Indicates that these values come from the controller and should remain static (i.e. zone number)

**Static:** Indicates these are hardcoded values and are not editable

**Editable:** Indicates that the user can change these settings in the "zone configuration" UI (not yet built).

|Section|Variable|Comment|
|---|---|---|
|general|name|From Controller with Update|
|general|interpolation|Static: SOXR|
|general|output_backend|Static: alsa|
|general|mdns_backend|Static: avahi|
|general|port|From Controller: 7000 + ZONE_ID|
|general|airplay_device_id_offset|From Controller: ZONE_ID|
|general|regtype|Static: _airplay._tcp|
|general|ignore_volume_control|Static: yes|
|general|volume_control_profile|Editable: [standard, flat, dasl_tapered]|
|general|default_airplay_volume|From Controller: INIVOL (Must be converted from 0-100 to Apple range of -30.0 to 0.00)|
|general|run_this_when_volume_is_set|From Controller: /path/to/script.sh (This will be converted and volume command sent to controller)|
|sessioncontrol|run_this_before_entering_active_state|From Controller: path/to/script.sh (This will turn the zone on and set the zone source to the airplay source number set in the UI.)|
|sessioncontrol|run_this_after_exiting_active_state|PCM handoff + always power off the exiting zone|
|sessioncontrol|active_state_timeout|Editable: Default 5.0|
|sessioncontrol|run_this_before_play_begins|From Controller: path/to/script.sh (This will turn the zone on and set the zone source to the airplay source number set in the UI.)|
|sessioncontrol|run_this_after_play_ends|No-op at track boundary (PCM teardown runs on `deactivate` only)|
|sessioncontrol|run_this_if_an_unfixable_error_is_detected|Static: path/to/script.sh (This should be sent to a logger function and notification in UI)|
|sessioncontrol|wait_for_completion|Static: yes|
|sessioncontrol|allow_session_interruption|Static: no|
|sessioncontrol|session_timeout|Editable: Default 60|
|alsa|output_device|Static: This is based on the zone and alsa loopback|
|alsa|output_rate|Staic: Default 44100 (This should match the PCM Router)|
|alsa|output_format|Static: S32_LE (This should match the PCM Router)|
|metadata|enabled|Static: yes|
|metadata|include_cover_art|Static: yes|
|metadata|cover_art_cache_directory|Static: ""|
|metadata|pipe_name|From Controller: /tmp/homepi-metadata-zone-ZONE_ID|
|mqtt|enabled|Static: yes|
|mqtt|publish_raw|Static: no|
|mqtt|publish_parsed|Static: yes|
|mqtt|publish_cover|Static: yes|
|mqtt|publish_retain|Static: yes|
|mqtt|enable_autodiscovery|Static: yes|
|mqtt|autodiscovery_prefix|Static: homepi|
|mqtt|enable_remote|Static: yes|
|diagnostic|log_output_to|Static: Need to incorporate a way to get these logs into the homepi ecosystem|
|diagnostic|log_verbosity|Editable: [0,1,2,3]|

