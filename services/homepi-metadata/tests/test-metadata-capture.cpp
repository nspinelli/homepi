#include <cassert>
#include <fstream>
#include <iostream>
#include <string>

#include "homepi/metadata/metadata-parser.hpp"

#ifndef METADATA_CAPTURE_FIXTURE
#define METADATA_CAPTURE_FIXTURE "tests/fixtures/metadata-capture-zone8.txt"
#endif

int main() {
  std::ifstream input(METADATA_CAPTURE_FIXTURE);
  if (!input) {
    std::cerr << "capture file missing, skipping integration assertions\n";
    return 0;
  }

  std::string capture((std::istreambuf_iterator<char>(input)),
                      std::istreambuf_iterator<char>());

  bool saw_title = false;
  bool saw_artist = false;
  bool saw_progress = false;
  bool saw_duration = false;
  bool saw_cover = false;

  homepi::metadata::MetadataParser parser({
      .on_field =
          [&](const homepi::metadata::MetadataFieldUpdate& update) {
            if (update.field == "title" && update.value == "Five More Minutes") {
              saw_title = true;
            }
            if (update.field == "artist" && update.value == "Scotty McCreery") {
              saw_artist = true;
            }
          },
      .on_progress =
          [&](const homepi::metadata::MetadataProgressUpdate& update) {
            if (update.has_position && update.position_ms > 0) {
              saw_progress = true;
            }
            if (update.has_duration && update.duration_ms > 1000) {
              saw_duration = true;
            }
          },
      .on_cover_art =
          [&](const std::vector<std::uint8_t>& bytes) {
            if (bytes.size() > 1024) {
              saw_cover = true;
            }
          },
  });

  parser.feed(capture.data(), capture.size(), true);
  assert(saw_title);
  assert(saw_artist);
  assert(saw_progress);
  assert(saw_duration);
  assert(saw_cover);
  std::cout << "test_metadata_capture: OK\n";
  return 0;
}
