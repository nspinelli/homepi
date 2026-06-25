#include <cassert>
#include <cstdint>
#include <iostream>
#include <vector>

#include "homepi/metadata/progress-parser.hpp"

int main() {
  const auto prgr = homepi::metadata::parse_prgr_progress("1000/2000/5000", 44100);
  if (!prgr.has_position || !prgr.has_duration || prgr.position_ms != 22 || prgr.duration_ms != 90) {
    std::cerr << "prgr parse failed\n";
    return 1;
  }

  const auto phbt = homepi::metadata::parse_phbt_progress("1000/2000", 44100, 1000);
  if (!phbt.has_position || phbt.position_ms != 22) {
    std::cerr << "phbt parse failed\n";
    return 1;
  }

  if (homepi::metadata::parse_progress_current_rtp("1000/2000/5000") != 2000) {
    std::cerr << "progress current parse failed\n";
    return 1;
  }

  const auto phbt_baseline =
      homepi::metadata::parse_phbt_progress("1326959397/1327004453", 44100, 1326959397);
  if (!phbt_baseline.has_position || phbt_baseline.position_ms <= 0) {
    std::cerr << "phbt baseline parse failed\n";
    return 1;
  }

  const std::vector<std::uint8_t> astm = {0, 0, 2, 0x2b};
  const auto duration = homepi::metadata::parse_astm_duration(astm);
  if (!duration.has_duration || duration.duration_ms != 555) {
    std::cerr << "astm parse failed\n";
    return 1;
  }

  if (homepi::metadata::parse_progress_start_rtp("1000/2000/5000") != 1000) {
    std::cerr << "progress start parse failed\n";
    return 1;
  }

  const auto zero_prgr = homepi::metadata::parse_prgr_progress("0/0/0", 44100);
  if (zero_prgr.has_position || zero_prgr.has_duration) {
    std::cerr << "zero prgr should be ignored\n";
    return 1;
  }

  return 0;
}
