#pragma once

#include <stdexcept>
#include <string>

namespace homepi::storage {

/** Storage repository failure. */
class RepositoryError : public std::runtime_error {
 public:
  explicit RepositoryError(const std::string& message) : std::runtime_error(message) {}
};

}  // namespace homepi::storage
