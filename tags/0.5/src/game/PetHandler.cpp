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
#include "World.h"
#include "ObjectMgr.h"
#include "Log.h"
#include "Opcodes.h"
#include "Spell.h"
#include "ObjectAccessor.h"
#include "MapManager.h"
#include "TargetedMovementGenerator.h"
#include "CreatureAI.h"
#include "Util.h"
#include "Pet.h"

void WorldSession::HandlePetAction( WorldPacket & recv_data )
{

    uint64 guid1;
    uint16 spellid;
    uint16 flag;
    uint64 guid2;
    WorldPacket data;
    recv_data >> guid1;                                     //pet guid
    recv_data >> spellid;
    recv_data >> flag;                                      //delete = 0x0700 CastSpell = C100
    recv_data >> guid2;                                     //tag guid

    Creature* pet=ObjectAccessor::Instance().GetCreature(*_player,guid1);
    sLog.outString( "HandlePetAction.Pet %u flag is %u, spellid is %u, target %u.\n", uint32(GUID_LOPART(guid1)), flag, spellid, uint32(GUID_LOPART(guid2)) );
    if(!pet)
    {
        sLog.outError( "Pet %u not exist.\n", uint32(GUID_LOPART(guid1)) );
        return;
    }

    if(pet != GetPlayer()->GetPet() && pet != GetPlayer()->GetCharm())
    {
        sLog.outError( "HandlePetAction.Pet %u isn't pet of player %s .\n", uint32(GUID_LOPART(guid1)),GetPlayer()->GetName() );
        return;
    }

    switch(flag)
    {
        case 1792:                                          //0x0700
            switch(spellid)
            {
                case 0x0000:                                //flat=1792  //STAY
                    pet->StopMoving();
                    (*pet)->Idle();
                    if(pet->isPet())
                    {
                        ((Pet*)pet)->AddActState( STATE_RA_STAY );
                        ((Pet*)pet)->ClearActState( STATE_RA_FOLLOW );
                    }
                    break;
                case 0x0001:                                //spellid=1792  //FOLLOW
                    pet->AttackStop();
                    pet->addUnitState(UNIT_STAT_FOLLOW);
                    (*pet)->Mutate(new TargetedMovementGenerator(*_player));
                    if(pet->isPet())
                    {
                        ((Pet*)pet)->AddActState( STATE_RA_FOLLOW );
                        ((Pet*)pet)->ClearActState( STATE_RA_STAY );
                    }
                    break;
                case 0x0002:                                //spellid=1792  //ATTACK
                {
                    pet->clearUnitState(UNIT_STAT_FOLLOW);
                    uint64 selguid = _player->GetSelection();
                    Unit *TargetUnit = ObjectAccessor::Instance().GetUnit(*_player, selguid);
                    if(TargetUnit == NULL) return;

                    // not let attack friendly units.
                    if( GetPlayer()->IsFriendlyTo(TargetUnit))
                        return;

                    if(TargetUnit!=pet->getVictim())
                        pet->AttackStop();
                    pet->AI().AttackStart(TargetUnit);
                    data.Initialize(SMSG_AI_REACTION);
                    data << guid1 << uint32(00000002);
                    SendPacket(&data);
                    break;
                }
                case 3:
                    _player->UnsummonPet(pet);
                    break;
                default:
                    sLog.outError("WORLD: unknown PET flag Action %i and spellid %i.\n", flag, spellid);
            }
            break;
        case 1536:
            switch(spellid)
            {
                case 0:                                     //passive
                    if(pet->isPet())
                    {
                        ((Pet*)pet)->AddActState( STATE_RA_PASSIVE );
                        ((Pet*)pet)->ClearActState( STATE_RA_PROACTIVE | STATE_RA_PROACTIVE );
                    }
                    break;
                case 1:                                     //recovery
                    if(pet->isPet())
                    {
                        ((Pet*)pet)->AddActState( STATE_RA_REACTIVE );
                        ((Pet*)pet)->ClearActState( STATE_RA_PASSIVE | STATE_RA_PROACTIVE );
                    }
                    break;
                case 2:                                     //activete
                    if(pet->isPet())
                    {
                        ((Pet*)pet)->AddActState( STATE_RA_PROACTIVE );
                        ((Pet*)pet)->ClearActState( STATE_RA_PASSIVE | STATE_RA_REACTIVE );
                    }
                    break;
            }
            break;
        case 49408:                                         //0xc100    spell
        {
            uint64 selectguid = _player->GetSelection();
            Unit* unit_target=ObjectAccessor::Instance().GetUnit(*_player,selectguid);
            if(!unit_target)
                return;

            // do not spell attack of friends
            if(_player->IsFriendlyTo(unit_target))
                return;

            pet->clearUnitState(UNIT_STAT_FOLLOW);
            SpellEntry *spellInfo = sSpellStore.LookupEntry(spellid );
            if(!spellInfo)
            {
                sLog.outError("WORLD: unknown PET spell id %i\n", spellid);
                return;
            }

            Spell *spell = new Spell(pet, spellInfo, false, 0);
            WPAssert(spell);

            SpellCastTargets targets;
            targets.setUnitTarget( unit_target );
            spell->prepare(&targets);
            break;
        }
        default:
            sLog.outError("WORLD: unknown PET flag Action %i and spellid %i.\n", flag, spellid);
    }
}

void WorldSession::HandlePetNameQuery( WorldPacket & recv_data )
{
    sLog.outString( "HandlePetNameQuery.\n" );
    uint32 who;
    uint64 guid;
    //float state3;

    std::string name = "ERROR_NO_NAME_FOR_PET_GUID";

    recv_data >> who;
    recv_data >> guid;

    Creature* pet=ObjectAccessor::Instance().GetCreature(*_player,guid);
    if(pet && pet->isPet())
    {
        name   = ((Pet*)pet)->GetName();
        //state3 = pet->GetStat(STAT_INTELLECT);
    }
    WorldPacket data;
    data.Initialize(SMSG_PET_NAME_QUERY_RESPONSE);
    //The value is random,This should be (who query):data << uint32(who);
    data << uint32(0x4D4537);
    data << name.c_str();
    //Unknown, Fix me;
    data << uint32(0x426D3DC6);
    _player->GetSession()->SendPacket(&data);
}

void WorldSession::HandlePetSetAction( WorldPacket & recv_data )
{
    sLog.outString( "HandlePetSetAction. CMSG_PET_SET_ACTION\n" );
}

void WorldSession::HandlePetRename( WorldPacket & recv_data )
{
    sLog.outString( "HandlePetRename. CMSG_PET_RENAME\n" );
    /*
    uint32 state2;
    uint64 guid;

    std::string name = "ERROR_NO_NAME_FOR_PET_GUID";

    recv_data >> state2;
    recv_data >> guid;
    recv_data >> name;
    WorldPacket data;

    data.Initialize(SMSG_PET_NAME_QUERY_RESPONSE);
    data << uint32(state2);
    data << name;
    data << uint32(0x426D3DC6);
    _player->GetSession()->SendPacket(&data);
    */
}

void WorldSession::HandlePetAbandon( WorldPacket & recv_data )
{
    uint64 guid;
    recv_data >> guid;                                      //pet guid
    sLog.outString( "HandlePetAbandon. CMSG_PET_ABANDON pet guid is %u", GUID_LOPART(guid) );
    Creature* pet=ObjectAccessor::Instance().GetCreature(*_player, guid);
    if(pet)
    {
        if(pet->GetGUID() == _player->GetPetGUID())
        {
            uint32 feelty = pet->GetPower(POWER_HAPPINESS);
            pet->SetPower(POWER_HAPPINESS ,(feelty-50000) > 0 ?(feelty-50000) : 0);
            _player->UnsummonPet();
        }
        else if(pet->GetGUID() == _player->GetCharmGUID())
        {
            _player->Uncharm();
        }
    }
}
