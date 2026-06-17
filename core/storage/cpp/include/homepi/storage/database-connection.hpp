#pragma once

#include <memory>
#include <string>

struct sqlite3;

namespace homepi::storage {

/** SQLite open mode. */
enum class DatabaseOpenMode { ReadWrite, ReadOnly };

/** RAII SQLite database connection. */
class DatabaseConnection {
 public:
  /**
   * Opens a SQLite database.
   * @param path Database file path.
   * @param mode Open mode.
   */
  DatabaseConnection(const std::string& path, DatabaseOpenMode mode);
  ~DatabaseConnection();

  DatabaseConnection(const DatabaseConnection&) = delete;
  DatabaseConnection& operator=(const DatabaseConnection&) = delete;

  /**
   * Returns the raw sqlite handle.
   * @return sqlite3 pointer.
   */
  sqlite3* handle() const;

  /**
   * Returns true when the database is open.
   * @return Open state.
   */
  bool is_open() const;

 private:
  sqlite3* db_ = nullptr;
};

}  // namespace homepi::storage
