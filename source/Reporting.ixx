module;
#pragma warning(disable: 4251)
#include <dpp/dpp.h>

export module Reporting;

import <chrono>;
import <format>;
import <list>;
import <map>;
import <string>;

import Algorithms;
import Commands;
import Config;
import Subspace;


/// <summary>
/// Container for player stats.
/// </summary>
export struct PlayerStats
{
    uint32_t repeller{};
    uint32_t rockets{};
    uint32_t kills{};
    uint32_t deaths{};
    bool isSpectator{};
};


// Reporting config objects
//
// Duration in seconds to pause showing stats the next time
export CounterDuration c_itemsRequestInterval{ 5 };


// State variables
//
// Names of team 100 and team 200 in case of a match
export std::string g_team100Name{ "100" };
export std::string g_team200Name{ "200" };
// Stats of team 100 and team 200 players
export std::map<std::string, PlayerStats> g_team100Stats;  // player name as key
export std::map<std::string, PlayerStats> g_team200Stats;  // player name as key
// Timestamp for the last .items request
export TimeStamp g_lastItemsRequestTimeStamp;
// Flaf to indicate if reporting is currently done during a match
export bool g_isReporting{};

std::string g_lastUpdatedPlayersMessage;
dpp::message g_chatChPlayersMessage;


//////// Reporting ////////

MessageList getPlayersTable()
{
    MessageList retMessages;

    if (g_playerInfos.size() > 0) {
        retMessages.push_back(format("\x1b[0mArena: \x1b[34m{}", g_arena));

        // print the frequencies of non-spectators
        if (g_team100Stats.size() > 0) {
            std::string freqSizeStr{ std::to_string(g_team100Stats.size()) };

            retMessages.push_back(format("\x1b[31m[{}]{}W--L--Re-Ro-{} {}", 
                g_team100Name, std::string(17 - g_team100Name.length(), '-'),
                std::string(3 - freqSizeStr.length(), '-'), freqSizeStr));

            for (auto& [player, stats] : g_team100Stats) {
                retMessages.push_back(format("  \x1b[0m{}  \x1b[0m{} {} {} {}",
                    padRight(player, 15), padRight(std::to_string(stats.kills), 2),
                    padRight(std::to_string(stats.deaths), 2),
                    padRight(std::to_string(stats.repeller), 2),
                    padRight(std::to_string(stats.rockets), 2)));
            }
        }

    }

    return retMessages;
}


//export void updateplayersmessage()
//{
//    messagelist replymessages = getplayerstats();
//
//    if (replymessages.size() > 0) {
//        // the current stats table is not empty, i.e. there are more than two bots in the arena
//        if (g_lastupdatedplayersmessage.empty() && g_chatchannelid) {
//            // send the playerlist as arena message, the message id will be stored in the create-
//            // message command handler on receiving a notification for this post
//            g_lastupdatedplayersmessage = sendmessage(g_chatchannelid,
//                join(replymessages), chatmodes::msg_arena);
//        }
//        else if (g_chatchplayersmessage.id) {
//            // update the players message in the chat channel
//            string formatmsg{ formatmessage(join(replymessages), chatmodes::msg_arena) };
//
//            if (formatmsg != g_lastupdatedplayersmessage) {
//                // if there is a new playerlist message, so update it
//                updatemessage(g_chatchannelid, m_chatchplayersmessage, formatmsg);
//
//                g_lastupdatedplayersmessage = formatmsg;
//            }
//        }
//    }
//    else {
//        if (g_chatchplayersmessage.id && g_chatchannelid) {
//            // there are no players in the arena, so delete the existing players message in the 
//            // chat channel
//            try {
//                m_botclient->message_delete(g_chatchplayersmessage.id, g_chatchannelid);
//            }
//            catch (std::exception& ex) {
//                cout << "error: failed to delete message " << g_chatchplayersmessage.id << ": "
//                    << ex.what() << endl;
//            }
//            g_chatchplayersmessage = message();
//            g_lastupdatedplayersmessage.clear();
//        }
//    }
//}



//////// Setup ////////

/// <summary>
/// Setup reporting.
/// </summary>
export void setupReporting()
{
    if (!g_isReportingEnabled) {
        return;
    }
}

