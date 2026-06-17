#include "homepi/storage/database-connection.hpp"

#include <sqlite3.h>

#include "homepi/storage/repository-error.hpp"

namespace homepi::storage {

DatabaseConnection::DatabaseConnection(const std::string& path, DatabaseOpenMode mode) {
  const int flags =
      (mode == DatabaseOpenMode::ReadOnly) ? SQLITE_OPEN_READONLY : (SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE);
  if (sqlite3_open_v2(path.c_str(), &db_, flags, nullptr) != SQLITE_OK) {
    const std::string message = db_ != nullptr ? sqlite3_errmsg(db_) : "unknown sqlite open error";
    if (db_ != nullptr) {
      sqlite3_close(db_);
      db_ = nullptr;
    }
    throw RepositoryError("database open failed: " + message);
  }
  sqlite3_exec(db_, "PRAGMA foreign_keys = ON;", nullptr, nullptr, nullptr);
}

DatabaseConnection::~DatabaseConnection() {
  if (db_ != nullptr) {
    sqlite3_close(db_);
    db_ = nullptr;
  }
}

sqlite3* DatabaseConnection::handle() const { return db_; }

bool DatabaseConnection::is_open() const { return db_ != nullptr; }

}  // namespace homepi::storage
