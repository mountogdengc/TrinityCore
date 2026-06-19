/*
 * This file is part of the TrinityCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "AccountService.h"
#include "BattlenetRpcErrorCodes.h"
#include "Session.h"

namespace Battlenet::Services
{
namespace V1
{
Account::Account(Session* session) : AccountService(session)
{
}

uint32 Account::HandleGetAccountState(account::v1::GetAccountStateRequest const* request, account::v1::GetAccountStateResponse* response, std::function<void(ServiceBase*, uint32, ::google::protobuf::Message const*)>& /*continuation*/)
{
    if (!_session->IsAuthed())
        return ERROR_DENIED;

    if (request->options().field_privacy_info())
    {
        response->mutable_state()->mutable_privacy_info()->set_is_using_rid(false);
        response->mutable_state()->mutable_privacy_info()->set_is_visible_for_view_friends(false);
        response->mutable_state()->mutable_privacy_info()->set_is_hidden_from_friend_finder(true);

        response->mutable_tags()->set_privacy_info_tag(0xD7CA834D);
    }

    if (request->options().field_security_status())
    {
        // Report the account as having a Battle.net Authenticator attached. The retail client uses
        // this to unlock the 4 extra backpack slots (and to satisfy other authenticator-gated perks),
        // independently of the NumBackpackSlots field sent by the world server.
        response->mutable_state()->mutable_security_status()->set_sms_protect_enabled(false);
        response->mutable_state()->mutable_security_status()->set_email_verified(true);
        response->mutable_state()->mutable_security_status()->set_authenticator_enabled(true);
        response->mutable_state()->mutable_security_status()->set_sqa_enabled(false);

        // NOTE: best-effort version tag. The exact client constant for this field is unknown; the
        // client reads the state directly for a one-shot GetAccountState, so the value above is what
        // matters. If the client ignores the field, this tag is the first thing to revisit.
        response->mutable_tags()->set_security_status_tag(0xC8D52E61);
    }

    return ERROR_OK;
}

uint32 Account::HandleGetGameAccountState(account::v1::GetGameAccountStateRequest const* request, account::v1::GetGameAccountStateResponse* response, std::function<void(ServiceBase*, uint32, ::google::protobuf::Message const*)>& /*continuation*/)
{
    if (!_session->IsAuthed())
        return ERROR_DENIED;

    if (request->options().field_game_level_info())
    {
        if (GameAccountInfo const* gameAccountInfo = _session->GetGameAccountInfo(request->game_account_id().low()))
        {
            response->mutable_state()->mutable_game_level_info()->set_name(gameAccountInfo->DisplayName);
            response->mutable_state()->mutable_game_level_info()->set_program(5730135); // WoW
        }

        response->mutable_tags()->set_game_level_info_tag(0x5C46D483);
    }

    if (request->options().field_game_status())
    {
        if (GameAccountInfo const* gameAccountInfo = _session->GetGameAccountInfo(request->game_account_id().low()))
        {
            response->mutable_state()->mutable_game_status()->set_is_suspended(gameAccountInfo->IsBanned);
            response->mutable_state()->mutable_game_status()->set_is_banned(gameAccountInfo->IsPermanenetlyBanned);
            response->mutable_state()->mutable_game_status()->set_suspension_expires(uint64(gameAccountInfo->UnbanDate) * 1000000);
        }

        response->mutable_state()->mutable_game_status()->set_program(5730135); // WoW
        response->mutable_tags()->set_game_status_tag(0x98B75F99);
    }

    return ERROR_OK;
}
}
}
