#pragma once

#include <memory>

#include "homepi/pcm-router/types.hpp"

namespace homepi::pcm_router {

/** pcm-router daemon lifecycle. */
class Service {
 public:
  explicit Service(ServiceConfig config);
  ~Service();

  int run();
  static int validate_and_exit(const ServiceConfig& config, const std::string& mode);

 private:
  void shutdown();
  std::string build_snapshot_json() const;

  ServiceConfig config_;
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace homepi::pcm_router
