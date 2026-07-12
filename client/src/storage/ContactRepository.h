#pragma once
#include "core/Contact.h"
#include "storage/DatabaseManager.h"
#include <optional>
#include <string>
#include <vector>

// CRUD operations on the contacts table.
// All methods must be called from the owning (DB) thread.
class ContactRepository {
public:
    explicit ContactRepository(DatabaseManager& db);

    // Insert or update (conflict on owner_id + user_id updates display fields).
    void save(const Contact& contact);

    // Replace all contacts for owner_id atomically (DELETE + INSERT in one transaction).
    void replaceAll(const std::string&         owner_id,
                    const std::vector<Contact>& contacts);

    // Delete a single contact relationship.
    void remove(const std::string& owner_id, const std::string& user_id);

    // Return all contacts for owner_id, ordered by display_name (case-insensitive).
    std::vector<Contact> findAll(const std::string& owner_id);

    // Return a single contact, or nullopt if not found.
    std::optional<Contact> findById(const std::string& owner_id,
                                    const std::string& user_id);

private:
    DatabaseManager& db_;

    static Contact rowToContact(sqlite3_stmt* stmt);
};
