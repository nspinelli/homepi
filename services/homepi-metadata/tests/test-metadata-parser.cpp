#include <cassert>
#include <iostream>
#include <string>

#include "homepi/metadata/metadata-parser.hpp"

int main() {
  bool saw_title = false;
  bool saw_inline_title = false;
  bool saw_artist = false;
  bool saw_duration = false;
  bool saw_position = false;

  homepi::metadata::MetadataParser parser({
      .on_field =
          [&](const homepi::metadata::MetadataFieldUpdate& update) {
            if (update.field == "title" && update.value == "Test Song") {
              saw_title = true;
            }
            if (update.field == "title" && update.value == "S Wish Grandpas Never Died") {
              saw_inline_title = true;
            }
            if (update.field == "artist" && update.value == "Scotty McCreery") {
              saw_artist = true;
            }
          },
      .on_progress =
          [&](const homepi::metadata::MetadataProgressUpdate& update) {
            if (update.has_duration && update.duration_ms == 182706) {
              saw_duration = true;
            }
            if (update.has_position && update.position_ms == 46) {
              saw_position = true;
            }
          },
  });

  const std::string header =
      "<item><type>636f7265</type><code>6d696e6d</code><length>9</length>\n";
  const std::string data_tag = "<data encoding=\"base64\">\n";
  const std::string payload = "VGVzdCBTb25n\n";
  const std::string footer = "</data></item>\n";
  parser.feed((header + data_tag + payload + footer).data(),
              header.size() + data_tag.size() + payload.size() + footer.size(), true);
  assert(saw_title);

  homepi::metadata::MetadataParser inline_parser({
      .on_field =
          [&](const homepi::metadata::MetadataFieldUpdate& update) {
            if (update.field == "title" && update.value == "S Wish Grandpas Never Died") {
              saw_inline_title = true;
            }
          },
  });

  const std::string inline_item =
      "<item><type>636f7265</type><code>6d696e6d</code><length>26</length>\n"
      "<data encoding=\"base64\">\n"
      "SSBXaXNoIEdyYW5kcGFzIE5ldmVyIERpZWQ=</data></item>\n";
  inline_parser.feed(inline_item.data(), inline_item.size(), true);
  assert(saw_inline_title);

  const std::string glued_item =
      "<item><type>636f7265</type><code>6d706572</code><length>8</length>\n"
      "<data encoding=\"base64\">\n"
      "mXxsNUOPenE=</data><item><type>636f7265</type><code>61736172</code><length>15</length>\n"
      "<data encoding=\"base64\">\n"
      "U2NvdHR5IE1jQ3JlZXJ5</data></item>\n"
      "<item><type>636f7265</type><code>6173746d</code><length>4</length>\n"
      "<data encoding=\"base64\">\n"
      "AALJsg==</data></item>\n"
      "<item><type>73736e63</type><code>70726772</code><length>32</length>\n"
      "<data encoding=\"base64\">\n"
      "MTE0OTA1NjQyMS8xMTQ5MDU4NDc1LzExNTcxMTM3NTU=</data></item>\n";
  parser.feed(glued_item.data(), glued_item.size(), true);
  assert(saw_artist);
  assert(saw_duration);
  assert(saw_position);

  bool saw_fifo_style_progress = false;
  homepi::metadata::MetadataParser fifo_parser({
      .on_progress =
          [&](const homepi::metadata::MetadataProgressUpdate& update) {
            if (update.has_duration && update.duration_ms == 182706 && update.has_position &&
                update.position_ms == 46) {
              saw_fifo_style_progress = true;
            }
          },
  });
  const std::string fifo_chunk =
      "<item><type>636f7265</type><code>6173746d</code><length>4</length>\n"
      "<data encoding=\"base64\">\n"
      "AALJsg==</data></item>\n"
      "<item><type>73736e63</type><code>70726772</code><length>32</length>\n"
      "<data encoding=\"base64\">\n"
      "MTE0OTA1NjQyMS8xMTQ5MDU4NDc1LzExNTcxMTM3NTU=</data></item>\n";
  fifo_parser.feed(fifo_chunk.data(), fifo_chunk.size(), true);
  assert(saw_fifo_style_progress);

  std::cout << "test_metadata_parser: OK\n";
  return 0;
}
