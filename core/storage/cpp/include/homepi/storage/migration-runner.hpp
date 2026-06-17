#pragma once

#include <string>

namespace homepi::storage {

class DatabaseConnection;

/** Applies SQL migration scripts to a database connection. */
class MigrationRunner {
 public:
  /**
   * Executes a migration SQL script.
   * @param db Database connection.
   * @param sql Migration SQL contents.
   */
  static void apply(DatabaseConnection& db, const std::string& sql);
};

}  // namespace homepi::storage
