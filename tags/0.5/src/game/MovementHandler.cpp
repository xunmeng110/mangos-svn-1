/* 
 * Copyright (C) 2005,2006 MaNGOS <http://www.mangosproject.org/>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "Common.h"
#include "WorldPacket.h"
#include "WorldSession.h"
#include "Opcodes.h"
#include "Log.h"
#include "World.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "UpdateMask.h"
#include "MapManager.h"

void WorldSession::HandleMoveWorldportAckOpcode( WorldPacket & recv_data )
{
    sLog.outDebug( "WORLD: got MSG_MOVE_WORLDPORT_ACK." );

    GetPlayer()->RemoveFromWorld();
    MapManager::Instance().GetMap(GetPlayer()->GetMapId())->Add(GetPlayer());
    WorldPacket data;
    data.Initialize(SMSG_SET_REST_START);
    data << uint32(8129);
    SendPacket(&data);
    GetPlayer()->SetDontMove(false);
}

void WorldSession::HandleFallOpcode( WorldPacket & recv_data )
{
    uint32 flags, time;
    float x, y, z, orientation;
    Player *Target = GetPlayer();

    uint32 FallTime;

    uint64 guid;
    uint32 damage;

    if(Target->GetDontMove())
        return;

    recv_data >> flags >> time;
    recv_data >> x >> y >> z >> orientation;
    recv_data >> FallTime;

    if ( FallTime > 1100 && !Target->isDead())
    {
        uint32 MapID = Target->GetMapId();
        Map* Map = MapManager::Instance().GetMap(MapID);
        float posz = Map->GetWaterLevel(x,y);
        guid = Target->GetGUID();
        float fallperc = float(FallTime)*10/11000;
        float predamage = (fallperc*fallperc - 1) /9  * Target->GetMaxHealth();
        damage = (uint32)predamage;

        if (damage > 0 && damage < 2* Target->GetMaxHealth())
            Target->EnvironmentalDamage(guid,DAMAGE_FALL, damage);
        DEBUG_LOG("!! z=%f, pz=%f FallTime=%d posz=%f damage=%d" , z, Target->GetPositionZ(),FallTime, posz,damage);
    }

    //handle fall and logout at the sametime
    if (Target->HasFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_DISABLE_ROTATE))
    {
        Target->SetFlag(UNIT_FIELD_BYTES_1,PLAYER_STATE_SIT);
        // Can't move
        WorldPacket data;
        data.Initialize( SMSG_FORCE_MOVE_ROOT );
        data << (uint8)0xFF << Target->GetGUID() << (uint32)2;
        SendPacket( &data );
    }

    WorldPacket data;
    data.Initialize( recv_data.GetOpcode() );
    data << uint8(0xFF) << GetPlayer()->GetGUID();
    data << flags << time;
    data << x << y << z << GetPlayer()->GetOrientation();
    GetPlayer()->SendMessageToSet(&data, false);
}

void WorldSession::HandleMovementOpcodes( WorldPacket & recv_data )
{
    uint32 flags, time;
    float x, y, z, orientation;

    if(GetPlayer()->GetDontMove())
        return;

    recv_data >> flags >> time;
    recv_data >> x >> y >> z >> orientation;

    bool isJumping = GetPlayer()->HasMovementFlags(MOVEMENT_JUMPING);
    uint16 opcode = recv_data.GetOpcode();

    //Movement flag
    if (opcode == MSG_MOVE_HEARTBEAT && isJumping)
        GetPlayer( )->SetMovementFlags(GetPlayer( )->GetMovementFlags() & ~MOVEMENT_JUMPING);
    else
        GetPlayer( )->SetMovementFlags(flags);

    if(!(opcode == MSG_MOVE_HEARTBEAT && isJumping))
    {
        WorldPacket data;
        data.Initialize( opcode );
        data << uint8(0xFF) << GetPlayer()->GetGUID();
        data << flags << time;
        data << x << y << z << orientation;
        GetPlayer()->SendMessageToSet(&data, false);
    }

    GetPlayer( )->SetPosition(x, y, z, orientation);
}

void WorldSession::HandleSetActiveMoverOpcode(WorldPacket &recv_data)
{
    //CMSG_SET_ACTIVE_MOVER
    sLog.outDebug("WORLD: Recvd CMSG_SET_ACTIVE_MOVER");

    uint32 guild, time;
    recv_data >> guild >> time;
}

void WorldSession::HandleMountSpecialAnimOpcode(WorldPacket &recvdata)
{
    //sLog.outDebug("WORLD: Recvd CMSG_MOUNTSPECIAL_ANIM");

    WorldPacket data;
    data.Initialize(SMSG_MOUNTSPECIAL_ANIM);
    data << uint64(GetPlayer()->GetGUID());

    GetPlayer()->SendMessageToSet(&data, false);
}
