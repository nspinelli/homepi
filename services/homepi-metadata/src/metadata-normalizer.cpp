#include "homepi/metadata/metadata-normalizer.hpp"

#include <algorithm>
#include <cctype>
#include <regex>
#include <string>

namespace homepi::metadata {

namespace {

bool is_hex_digit(char ch) {
  return (ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'f') || (ch >= 'A' && ch <= 'F');
}

std::string trim_copy(std::string value) {
  auto not_space = [](unsigned char ch) { return std::isspace(ch) == 0; };
  value.erase(value.begin(), std::find_if(value.begin(), value.end(), not_space));
  value.erase(std::find_if(value.rbegin(), value.rend(), not_space).base(), value.end());
  return value;
}

void extract_feat_artist(NowPlayingSnapshot& snapshot) {
  if (!snapshot.artist.empty() || snapshot.title.empty()) {
    return;
  }

  static const std::regex feat_pattern(
      R"(^(.+?)\s*[\(\[](?:feat\.?|ft\.?|featuring)\s+([^)\]]+)[\)\]]\s*$)",
      std::regex::icase);
  std::smatch match;
  if (!std::regex_match(snapshot.title, match, feat_pattern) || match.size() < 3) {
    return;
  }

  const std::string base_title = trim_copy(match[1].str());
  const std::string featured_artist = trim_copy(match[2].str());
  if (!base_title.empty()) {
    snapshot.title = base_title;
  }
  if (!featured_artist.empty()) {
    snapshot.artist = featured_artist;
  }
}

}  // namespace

bool is_persistent_id_like(const std::string& value) {
  const std::string trimmed = trim_copy(value);
  if (trimmed.size() < 4 || trimmed.rfind("0x", 0) != 0) {
    return false;
  }
  for (std::size_t index = 2; index < trimmed.size(); ++index) {
    if (!is_hex_digit(trimmed[index])) {
      return false;
    }
  }
  return trimmed.size() > 2;
}

void normalize_now_playing_snapshot(NowPlayingSnapshot& snapshot) {
  snapshot.title = trim_copy(snapshot.title);
  snapshot.artist = trim_copy(snapshot.artist);
  snapshot.album = trim_copy(snapshot.album);
  snapshot.client_name = trim_copy(snapshot.client_name);

  if (!snapshot.album.empty() && snapshot.album == snapshot.title) {
    snapshot.album.clear();
  }
  if (!snapshot.album.empty() && snapshot.album == snapshot.artist) {
    snapshot.album.clear();
  }
  if (!snapshot.client_name.empty() &&
      (is_persistent_id_like(snapshot.client_name) ||
       snapshot.client_name == snapshot.track_id ||
       snapshot.client_name == snapshot.persistent_id)) {
    snapshot.client_name.clear();
  }

  if (snapshot.track_id.empty() && !snapshot.persistent_id.empty()) {
    snapshot.track_id = snapshot.persistent_id;
  }

  extract_feat_artist(snapshot);
}

}  // namespace homepi::metadata
