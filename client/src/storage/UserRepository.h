#pragma once
#include "core/LocalAccount.h"
#include "storage/DatabaseManager.h"
#include <optional>
#include <string>

// CRUD operations on the local_accounts table.
// All methods must be called from the owning (DB) thread.
class UserRepository {
public:
    explicit UserRepository(DatabaseManager& db);

    // Insert or update (conflict on server_id updates username/display_name/avatar_url/token).
    void save(const LocalAccount& account);

    // Update only the token for the given server_id.
    void updateToken(const std::string& server_id, const std::string& token);

    // Mark the given account as active, clearing the active flag on all others.
    void setActive(const std::string& server_id);

    // Clear the active flag on all accounts.
    void clearActive();

    // Return the currently active account, or nullopt if none.
    std::optional<LocalAccount> findActive();

    // Return the account with the given server_id, or nullopt if not found.
    std::optional<LocalAccount> findById(const std::string& server_id);

private:
    DatabaseManager& db_;

    static std::optional<LocalAccount> rowToAccount(sqlite3_stmt* stmt);
};
