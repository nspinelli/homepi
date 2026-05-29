#pragma once

#include <string>

namespace homepi::hifi_serial {

/**
 * Escapes a string for Hi-Fi2 protocol (quotes and asterisks).
 * @param value Raw string.
 * @return Escaped string without surrounding quotes.
 */
std::string escape_protocol_string(const std::string& value);

/**
 * Builds a command line with leading * and trailing CR.
 * @param body Command body without * prefix or terminator.
 * @return Full command bytes as string (includes CR).
 */
std::string build_command(const std::string& body);

/** Builds *VER query. */
std::string cmd_ver_query();

/** Builds zone name query; z=0 for all zones. */
std::string cmd_zone_name_query(int zone);

/** Builds zone volume query; z=0 for all. */
std::string cmd_zone_volume_query(int zone);

/** Builds zone power query. */
std::string cmd_zone_power_query(int zone);

/** Builds zone enable query. */
std::string cmd_zone_enable_query(int zone);

/** Builds zone treble query. */
std::string cmd_zone_treb_query(int zone);

/** Builds zone bass query. */
std::string cmd_zone_bass_query(int zone);

/** Builds zone balance query. */
std::string cmd_zone_bal_query(int zone);

/** Builds zone loudness query. */
std::string cmd_zone_loudness_query(int zone);

/** Builds zone initial volume query. */
std::string cmd_zone_inivol_query(int zone);

/** Builds zone page volume query. */
std::string cmd_zone_pgvol_query(int zone);

/** Builds zone group query. */
std::string cmd_zone_group_query(int zone);

/** Builds zone mute query. */
std::string cmd_zone_mute_query(int zone);

/** Builds zone source query. */
std::string cmd_zone_src_query(int zone);

/** Builds source name query; s=0 for all. */
std::string cmd_source_name_query(int source);

/** Builds source enable query. */
std::string cmd_source_enable_query(int source);

/** Builds source input gain query. */
std::string cmd_source_ingain_query(int source);

/** Builds source display line query. */
std::string cmd_source_displine_query(int source);

/** Builds group name query; g=0 for all. */
std::string cmd_group_name_query(int group);

/** Builds group type query. */
std::string cmd_group_type_query(int group);

/** Builds network config query. */
std::string cmd_netconfig_query();

/** Builds page state query. */
std::string cmd_page_query();

/** Builds language string query. */
std::string cmd_language_string_query(int index);

/** Sends raw command string (must start with *). */
std::string cmd_raw(const std::string& command);

}  // namespace homepi::hifi_serial
