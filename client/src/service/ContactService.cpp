#include "service/ContactService.h"
#include "logger/Logger.h"
#include "proto/MessageType.h"
#include "contact.pb.h"
#include "common.pb.h"
#include <algorithm>

ContactService::ContactService(TcpClient*         client,
                                ContactRepository& repo,
                                const User&        current_user,
                                QObject*           parent)
    : QObject(parent)
    , client_(client)
    , repo_(repo)
    , current_user_(current_user)
{
    contacts_ = repo_.findAll(current_user_.server_id);
}

// Helper: emit authExpired if code matches, otherwise emit operationFailed.
static bool checkAuthExpired(ContactService* svc, const novachat::ErrorResponse& er) {
    if (er.code() == novachat::AUTH_EXPIRED) {
        emit svc->authExpired();
        return true;
    }
    return false;
}

void ContactService::syncContacts() {
    novachat::GetContactsRequest req;
    req.set_token(current_user_.token);

    Packet pkt;
    pkt.type = MessageType::GetContactsRequest;
    pkt.payload.resize(req.ByteSizeLong());
    req.SerializeToArray(pkt.payload.data(), static_cast<int>(pkt.payload.size()));

    client_->sendPacket(std::move(pkt), [this](Packet resp) {
        if (resp.type == MessageType::GetContactsResponse) {
            novachat::GetContactsResponse r;
            if (!r.ParseFromArray(resp.payload.data(),
                                  static_cast<int>(resp.payload.size()))) return;

            std::vector<Contact> list;
            list.reserve(r.contacts_size());
            for (const auto& ci : r.contacts()) {
                Contact c;
                c.owner_id     = current_user_.server_id;
                c.user_id      = ci.user_id();
                c.username     = ci.username();
                c.display_name = ci.display_name();
                c.avatar_url   = ci.avatar_url();
                list.push_back(c);
            }
            repo_.replaceAll(current_user_.server_id, list);
            contacts_ = list;
            Logger::info("Contacts synced: {} contacts", list.size());
            emit contactsSynced(contacts_);

        } else if (resp.type == MessageType::ErrorResponse) {
            novachat::ErrorResponse er;
            er.ParseFromArray(resp.payload.data(), static_cast<int>(resp.payload.size()));
            if (!checkAuthExpired(this, er)) {
                emit operationFailed(QString::fromStdString(er.message()));
            }
        }
    });
}

void ContactService::addFriend(const std::string& target_user_id) {
    novachat::AddFriendRequest req;
    req.set_token(current_user_.token);
    req.set_target_user_id(target_user_id);

    Packet pkt;
    pkt.type = MessageType::AddFriendRequest;
    pkt.payload.resize(req.ByteSizeLong());
    req.SerializeToArray(pkt.payload.data(), static_cast<int>(pkt.payload.size()));

    client_->sendPacket(std::move(pkt), [this](Packet resp) {
        if (resp.type == MessageType::AddFriendResponse) {
            novachat::AddFriendResponse r;
            if (!r.ParseFromArray(resp.payload.data(),
                                  static_cast<int>(resp.payload.size()))) return;

            const auto& ci = r.contact();
            Contact c;
            c.owner_id     = current_user_.server_id;
            c.user_id      = ci.user_id();
            c.username     = ci.username();
            c.display_name = ci.display_name();
            c.avatar_url   = ci.avatar_url();
            repo_.save(c);
            contacts_.push_back(c);
            Logger::info("Friend added: {}", c.username);
            emit contactAdded(c);

        } else if (resp.type == MessageType::ErrorResponse) {
            novachat::ErrorResponse er;
            er.ParseFromArray(resp.payload.data(), static_cast<int>(resp.payload.size()));
            if (!checkAuthExpired(this, er)) {
                emit operationFailed(QString::fromStdString(er.message()));
            }
        }
    });
}

void ContactService::deleteFriend(const std::string& target_user_id) {
    novachat::DeleteFriendRequest req;
    req.set_token(current_user_.token);
    req.set_target_user_id(target_user_id);

    Packet pkt;
    pkt.type = MessageType::DeleteFriendRequest;
    pkt.payload.resize(req.ByteSizeLong());
    req.SerializeToArray(pkt.payload.data(), static_cast<int>(pkt.payload.size()));

    client_->sendPacket(std::move(pkt), [this, target_user_id](Packet resp) {
        if (resp.type == MessageType::DeleteFriendResponse) {
            repo_.remove(current_user_.server_id, target_user_id);
            contacts_.erase(
                std::remove_if(contacts_.begin(), contacts_.end(),
                    [&target_user_id](const Contact& c) {
                        return c.user_id == target_user_id;
                    }),
                contacts_.end());
            Logger::info("Friend deleted: {}", target_user_id);
            emit contactDeleted(target_user_id);

        } else if (resp.type == MessageType::ErrorResponse) {
            novachat::ErrorResponse er;
            er.ParseFromArray(resp.payload.data(), static_cast<int>(resp.payload.size()));
            if (!checkAuthExpired(this, er)) {
                emit operationFailed(QString::fromStdString(er.message()));
            }
        }
    });
}

void ContactService::searchUser(const std::string& keyword) {
    novachat::SearchUserRequest req;
    req.set_token(current_user_.token);
    req.set_keyword(keyword);

    Packet pkt;
    pkt.type = MessageType::SearchUserRequest;
    pkt.payload.resize(req.ByteSizeLong());
    req.SerializeToArray(pkt.payload.data(), static_cast<int>(pkt.payload.size()));

    client_->sendPacket(std::move(pkt), [this](Packet resp) {
        if (resp.type == MessageType::SearchUserResponse) {
            novachat::SearchUserResponse r;
            if (!r.ParseFromArray(resp.payload.data(),
                                  static_cast<int>(resp.payload.size()))) return;

            std::vector<Contact> results;
            results.reserve(r.users_size());
            for (const auto& ci : r.users()) {
                Contact c;
                c.user_id      = ci.user_id();
                c.username     = ci.username();
                c.display_name = ci.display_name();
                results.push_back(c);
            }
            emit searchResult(results);
        }
    });
}
