#include "homepi/storage/migration-runner.hpp"

#include <sqlite3.h>

#include "homepi/storage/database-connection.hpp"
#include "homepi/storage/repository-error.hpp"

namespace homepi::storage {

void MigrationRunner::apply(DatabaseConnection& db, const std::string& sql) {
  char* error_message = nullptr;
  if (sqlite3_exec(db.handle(), sql.c_str(), nullptr, nullptr, &error_message) != SQLITE_OK) {
    const std::string message = error_message != nullptr ? error_message : "migration failed";
    sqlite3_free(error_message);
    throw RepositoryError(message);
  }
}

}  // namespace homepi::storage
