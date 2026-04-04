export module Events;

import <list>;
import <map>;
import <ranges>;
import <string>;
import <vector>;

import Algorithms;
import Commands;
import Config;
import Discord;
import Recording;
import Subspace;


//////// Event helper ////////


/// <summary>
/// Shutdown the bot.
/// <param name="isConnectionLost">True, if the shutdown occurs due to a connection loss.</param>
/// </summary>
export void shutdown(bool isConnectionLost = false)
{
    stopRecording();
    stopObserving();
    writePersistentDataFile();
    writePlayerSquads();

    if (!c_specialFunctions.contains("recording")) {
        // press ALT+F4 to exit the chat window, only when not in recording mode (chat window)
        sendKey("!{F4}");
    }

    // the to press ESC+q to exit the game, in both modes (just in case)
    sendKey("{ESC}");
    sendKey("q");

    shutdownDiscord(isConnectionLost);
    g_isActive = false;
}


/// <summary>
/// Parse the player info (name, repeller, rockets) from an items info log line.
/// </summary>
/// <param name="logMsg">Log message.</param>
/// <param name="teamStats">Team stats container.</param>
void parseItemsInfo(std::string_view logMsg, TeamStats& teamStats)
{
    // preset all team members to consider non-active team members who are still present
    for (PlayerStats playerStats : teamStats | std::views::values) {
        playerStats.isSpectator = true;
    }

    std::string msg{ logMsg.substr(logMsg.find(":") + 2) };
    std::vector<std::string> splitMsg{ split(msg, ',') };

    for (std::string& playerInfo : splitMsg) {
        std::string cleanInfo{ trim(playerInfo) };

        std::string player{ cleanInfo.substr(0, cleanInfo.find("/") - 2) };
        std::string itemInfo{ cleanInfo.substr(cleanInfo.find("/") - 1) };
        uint32_t repeller{ (uint32_t)std::stoi(itemInfo.substr(0, 1)) };
        uint32_t rockets{ (uint32_t)std::stoi(itemInfo.substr(2, 1)) };

        if (!teamStats.contains(player)) {
            // store initial player info
            teamStats[player] = PlayerStats{ repeller, rockets };
        }
        else {
            // update existing player info
            teamStats[player].repeller = repeller;
            teamStats[player].rockets = rockets;

            if (teamStats[player].deaths < 3) {
                // account for players who specced and returned
                teamStats[player].isSpectator = false;
            }
        }
    }
}


/// <summary>
/// Update the kill or death score of a team player.
/// </summary>
/// <param name="player">Player name.</param>
/// <param name="isKilled">True, if the player has been killed, otherwise false.</param>
void updatePlayerScore(std::string& player, bool isKilled)
{
    if (g_team100Stats.contains(player)) {
        if (isKilled) {
            g_team100Stats[player].deaths += 1;

            if (g_team100Stats[player].deaths == 3)
                g_team100Stats[player].isSpectator = true;
        }
        else {
            g_team100Stats[player].kills += 1;
        }
    }
    else if (g_team200Stats.contains(player)) {
        if (isKilled) {
            g_team200Stats[player].deaths += 1;

            if (g_team200Stats[player].deaths == 3)
                g_team200Stats[player].isSpectator = true;
        }
        else {
            g_team200Stats[player].kills += 1;
        }
    }
}


//////// Reporting Event handler ////////

/// <summary>
/// Send an .items command if either a match has just started or a configured time interval has 
/// passed after the last request.
/// </summary>
/// <param name="logMsg">Log message.</param>
/// <param name="timeStamp">Current system time.</param>
void checkEventRequestResults(std::string_view logMsg, TimeStamp& timeStamp)
{
    if (!g_isFetchingPlayerNames && !g_isRecordingEnabled) {
        if (!g_isReporting && logMsg.find("  GO!") != -1) {
            // a match has started with a GO! 
            g_team100Stats.clear();
            g_team200Stats.clear();
            g_isReporting = true;
            g_lastItemsRequestTimeStamp = std::chrono::system_clock::now();
        }
        else if (g_isReporting 
            && timeStamp - g_lastItemsRequestTimeStamp >= c_itemsRequestInterval) {
            // a score has been sent or it's time to issue the next .items infor request
            if (!g_isNexusEnabled)
                sendPrivate(g_dzBotName, ".items");
            else
                sendPublic("?items");
            g_lastItemsRequestTimeStamp = std::chrono::system_clock::now();
        }
    }
}


/// <summary>
/// Check for reply to .items command to update players stats. Also update score, lagouts and 
/// returns. Applicable only for DiscordServerType 'svsproleage'.
/// </summary>
/// <param name="logMsg">Log message.</param>
/// <param name="timeStamp">Current system time.</param>
/// <param name="isProcessed">True, if processing is finished in this main loop cycle, otherwise
/// false.</param>
void checkEventPlayerStatsUpdate(std::string_view logMsg, TimeStamp& timeStamp, bool& isProcessed)
{
    std::string x;

    if (g_isReporting && isPrivateBotMessage(logMsg, g_dzBotName)) {
        if (logMsg.find("(100):") != -1) {
            // update repeller and rocket stats of team 100
            parseItemsInfo(logMsg, g_team100Stats);
        }
        else if (logMsg.find("(200):") != -1) {
            // update repeller and rocket stats of team 200
            parseItemsInfo(logMsg, g_team200Stats);
            updatePlayersMessage();
        }
    }
    else if (g_isReporting && !isPrivateBotMessage(logMsg, g_dzBotName)) {
        if (logMsg.find(" kb ") != -1) {
            // update kills and deaths of the related players
            std::string killedName{ logMsg.substr(2, logMsg.find(" kb ") - 2) };
            std::string killerName{ logMsg.substr(logMsg.find(" kb ") + 4) };

            if (killerName.find(" -- ") != -1) {
                // get rid of the assist info
                killerName = killerName.substr(0, killerName.find(" -- "));
            }

            updatePlayerScore(killedName, true);
            updatePlayerScore(killerName, false);
            updatePlayersMessage();
        }
        else if (logMsg.find("has Lagged out") != -1) {
            std::string laggedName{ logMsg.substr(2, logMsg.find(" has Lagged") - 2) };

            if (g_team100Stats.contains(laggedName)) {
                g_team100Stats[laggedName].isLaggedOut = true;
            }
            else if (g_team200Stats.contains(laggedName)) {
                g_team200Stats[laggedName].isLaggedOut = true;
            }
        }
        else if (logMsg.find("returns to the game") != -1) {
            std::string returnedName{ logMsg.substr(2, logMsg.find(" returns to") - 2) };

            if (g_team100Stats.contains(returnedName)) {
                g_team100Stats[returnedName].isLaggedOut = false;
            }
            else if (g_team200Stats.contains(returnedName)) {
                g_team200Stats[returnedName].isLaggedOut = false;
            }
        }
    }
}


/// <summary>
/// Check for reply to ?items command to update players stats. Also update score, lagouts and 
/// returns. Applicable only for DiscordServerType 'nexus'.
/// </summary>
/// <param name="logMsg">Log message.</param>
/// <param name="timeStamp">Current system time.</param>
/// <param name="isProcessed">True, if processing is finished in this main loop cycle, otherwise
/// false.</param>
void checkEventPlayerStatsUpdateNexus(std::string_view logMsg, TimeStamp& timeStamp, bool& isProcessed)
{
    std::string x;

    if (g_isReporting && logMsg.find("Freq ") != -1) {
        if (logMsg.find("  Freq 10: ") == 0) {
            // update repeller and rocket stats of team 100
            parseItemsInfo(logMsg, g_team100Stats);
        }
        else if (logMsg.find("  Freq 11: ") == 0) {
            // update repeller and rocket stats of team 200
            parseItemsInfo(logMsg, g_team200Stats);
            updatePlayersMessage();
        }
    }
    else if (g_isReporting) {
        if (logMsg.find(" kb ") != -1) {
            // update kills and deaths of the related players
            std::string killedName{ logMsg.substr(2, logMsg.find(" kb ") - 2) };
            std::string killerName{ logMsg.substr(logMsg.find(" kb ") + 4) };

            if (killerName.find(" (") != -1) {
                // get rid of the assist info
                killerName = killerName.substr(0, killerName.find(" ("));
            }

            updatePlayerScore(killedName, true);
            updatePlayerScore(killerName, false);
            updatePlayersMessage();
        }
        else if (logMsg.find("has Lagged out") != -1) {
            std::string laggedName{ logMsg.substr(2, logMsg.find(" has Lagged") - 2) };

            if (g_team100Stats.contains(laggedName)) {
                g_team100Stats[laggedName].isLaggedOut = true;
            }
            else if (g_team200Stats.contains(laggedName)) {
                g_team200Stats[laggedName].isLaggedOut = true;
            }
        }
        else if (logMsg.find("returns to the game") != -1) {
            std::string returnedName{ logMsg.substr(2, logMsg.find(" returns to") - 2) };

            if (g_team100Stats.contains(returnedName)) {
                g_team100Stats[returnedName].isLaggedOut = false;
            }
            else if (g_team200Stats.contains(returnedName)) {
                g_team200Stats[returnedName].isLaggedOut = false;
            }
        }
    }
}


/// <summary>
/// Check for the end of a match to stop reporting.
/// </summary>
/// <param name="logMsg">Log message.</param>
void checkEventEndReporting(std::string_view logMsg)
{
    // check for the LVP info the end of a match, but not from a .stats command
    if (g_isReporting && (!isPrivateBotMessage(logMsg, g_dzBotName) && (logMsg.find("  LVP:") == 0)
        || logMsg.find("No game in progress") != -1)) {
        // end of match detected, stop reporting scores
        g_isReporting = false;
        g_team100Stats.clear();
        g_team200Stats.clear();
        updatePlayersMessage();
    }
}


//////// Discord Event handler ////////

/// <summary>
/// Propagate public, team and arena messages to Discord.
/// </summary>
/// <param name="logMsg">Log message.</param>
void handleEventSendToDiscord(std::string_view logMsg)
{
    if (logMsg.length() > 0) {
        ChatMode chatMode{ ChatMode::Arena };
        std::string player{ parsePlayerName(logMsg) };
        std::string msg{ logMsg.substr(2) };

        if (!player.empty()) {
            if (logMsg.starts_with("  ")) {
                chatMode = ChatMode::Public;
            }
            else if (logMsg.starts_with("T ")) {
                chatMode = ChatMode::Team;
            }
            else if (logMsg.starts_with("P ")) {
                chatMode = ChatMode::Private;
            }

            msg = logMsg.substr(logMsg.find(">") + 2);
        }

        handleEventChat(chatMode, player, msg);
    }
}


//////// Recording Event handler ////////

/// <summary>
/// Check for match starting info to extract the game title. Also applied for reporting.
/// </summary>
/// <param name="logMsg">Log message.</param>
void checkEventMatchStartInfo(std::string_view logMsg)
{
    if (!g_isMatchGoing && logMsg.find("Pro League Match starting in 60 seconds") != -1) {
        // game title for the review mark file
        g_gameTitle = g_logBuffer[0].substr(2);
        // team names for freq 100 and 200
        if (g_gameTitle.find(" vs ") != std::string::npos) {
            g_team100Name = g_gameTitle.substr(0, g_gameTitle.find(" vs "));
            g_team200Name = g_gameTitle.substr(g_gameTitle.find(" vs ") + 4);
        }
    }
}


/// <summary>
/// Check for the start of a match.
/// </summary>
/// <param name="logMsg">Log message.</param>
/// <param name="timeStamp">Current system time.</param>
void checkEventMatchStartRecording(std::string_view logMsg, TimeStamp& timeStamp)
{
    if (!g_isMatchGoing && logMsg.find("  -1-") == 0) {
        if (g_isRecording) {
            // still recording during the stats+ESC period after a match, stop it
            stopRecording();
        }
        // the match has begun, start recording with OBS
        startRecording();
        // initialize those timestamps that need to have the current system time from the start
        g_statsPauseTimeStamp = timeStamp;
        g_itemsWaitTimeStamp = timeStamp;
    }
}


/// <summary>
/// Check for the end of a match. ESC is hit for a configured duration to show the stats.
/// </summary>
/// <param name="logMsg">Log message.</param>
/// <param name="timeStamp">Current system time.</param>
void checkEventMatchEnd(std::string_view logMsg, TimeStamp& timeStamp)
{
    // check for the LVP info the end of a match, but not from a .stats command
    if (g_isMatchGoing && logMsg.find("  LVP:") == 0 
        && !isPrivateBotMessage(logMsg, g_dzBotName)) {
        // end of match detected, hit ESC for a few seconds to show the stats
        g_isMatchGoing = false;

//        if (!g_isShowingStats) {
            // reactivate public chat
            sendPublic("?nopubchat");
            // hit ESC to show the stats table at the end of the match
            sendKey("{ESC}", 100);
            g_isShowingFinalStats = true;
//        }
        // set a new timestamp for showing stats with full-screen chat after match end
        g_statsShowTimeStamp = timeStamp;
    }
}


/// <summary>
/// Check for the end of a match. ESC is hit for a configured duration to show the stats.
/// </summary>
/// <param name="logMsg">Log message.</param>
/// <param name="timeStamp">Current system time.</param>
void checkEventMatchEndRecording(std::string_view logMsg, TimeStamp& timeStamp)
{
    if (g_isRecording && !g_isMatchGoing && g_isShowingFinalStats
        && timeStamp - g_statsShowTimeStamp >= c_endStatsShowInterval) {
        logDiscordBot("Ende Recording");
        execCmd("Send", "{ESC}");
        stopRecording();
        g_isRecording = false;
        g_isShowingFinalStats = false;
        g_isFetchingStats = false;
    }
}


/// <summary>
/// Check if a sub info was printed.
/// </summary>
/// <param name="logMsg">Log message.</param>
void checkEventSubInfo(std::string_view logMsg)
{
    if (g_isMatchGoing && logMsg.find(" in for ") != -1 && logMsg.find(" on Freq ") != -1) {
        // create a review mark for the sub info
        size_t pos1 = logMsg.find("[Time: ");
        std::string part1{ logMsg.substr(2, pos1 - 2) };
        size_t pos2 = logMsg.find("]", pos1);
        std::string part2{ logMsg.substr(pos2 + 2) };

        storeReviewMessage("", part1 + part2);
    }
}


/// <summary>
/// Check if a new score was printed after a kill.
/// </summary>
/// <param name="logMsg">Log message.</param>
/// <param name="timeStamp">Current system time.</param>
/// false.</param>
void checkEventNewScore(std::string_view logMsg, TimeStamp& timeStamp)
{
    // look for a score info, but not inside a stats table
    if (g_isMatchGoing && logMsg.find("  Score:") == 0) {
        if (!g_isWaitingForShowStats
            && timeStamp - g_statsPauseTimeStamp >= c_statsPauseInterval) {
            // let a pause interval pass before showing the stats
            g_statsWaitTimeStamp = timeStamp;
            g_isWaitingForShowStats = true;
        }
        // create a review mark for the kill
        if (logMsg.find("- ") != -1) {
            std::string msg{ logMsg.substr(2, logMsg.find("- ")) };

            storeReviewMessage("", msg + g_logBuffer[1].substr(2));
        }
    }
}


/// <summary>
/// Check for the time to show in-game stats.
/// </summary>
/// <param name="logMsg">Log message.</param>
/// <param name="timeStamp">Current system time.</param>
/// <param name="isProcessed">True, if processing is finished in this main loop cycle, otherwise
/// false.</param>
void checkEventShowStats(std::string_view logMsg, TimeStamp& timeStamp, bool& isProcessed)
{
    if (isProcessed) {
        // processing finished for this timeslot, try next time
        return;
    }

    if (g_isMatchGoing && g_isWaitingForShowStats) {
        if (timeStamp - g_statsWaitTimeStamp >= c_statsWaitInterval) {
            g_isWaitingForShowStats = false;
            // show the stats
            sendPrivate(g_dzBotName, ".stats");
            // set the timestamp for the next possibility to show stats
            g_statsPauseTimeStamp = timeStamp;
            // hit ESC to show the stats for a few seconds
//            sendKey("{ESC}");
            // set the timestamp for keeping full-screen chat
            g_statsShowTimeStamp = timeStamp;
            g_isShowingStats = true;
            // end of processing of events for this main loop cycle
            isProcessed = true;
        }
    }
}


/// <summary>
/// Check if in-game stats have been shown during a match. If stats have been shown, print a 
/// lean stats table.
/// </summary>
/// <param name="logMsg">Log message.</param>
/// <param name="timeStamp">Current system time.</param>
/// <param name="isProcessed">True, if processing is finished in this main loop cycle, otherwise
/// false.</param>
//void checkEventStatsShown(std::string_view logMsg, TimeStamp& timeStamp, bool& isProcessed)
//{
//    if (isProcessed) {
//        // processing finished for this timeslot, try next time
//        return;
//    }
//
//    if (g_isMatchGoing && g_isShowingStats && !logMsg.empty()) {
//        std::string msg{ logMsg };
//
//        if (logMsg.find("> ") != -1 && isPrivateBotMessage(logMsg, g_dzBotName)) {
//            msg = logMsg.substr(logMsg.find("> ") + 2);
//        }
//
//        if (!g_isFetchingStats) {
//            if (msg.starts_with("Score:")) {
//                g_stats100Buffer.clear();
//                g_stats100Buffer.push_back(msg);
//                g_stats200Buffer.clear();
//                g_stats200Buffer.push_back("");
//                g_isFetchingStats = true;
//                g_fetchedTablesCount = 0;
//                // switch to a separate freq to prevent team messages
//                sendPublic("=9");
//                sleep(200);
//            }
//        }
//
//        if (g_isFetchingStats && !msg.starts_with("+---") && !msg.starts_with("Score:")) {
//            if (msg.starts_with("|")) {
//                msg = replaceString(msg.substr(2, 84), "|", "");
//            }
//
//            if (msg.find("Ki/De") != -1) {
//                msg = replaceString(msg, "Ki/De", " Ki/De");
//            }
//            else if (msg.find("/") != -1) {
//                msg = " " + msg;
//            }
//
//            msg = replaceString(msg, "+", "{+}");
//
//            if (g_fetchedTablesCount == 0) {
//                // buffer the stats table for freq 100
//                g_stats100Buffer.push_back(msg);
//
//                if (msg.starts_with(" TOTAL:")) {
//                    g_fetchedTablesCount++;
//                }
//            }
//            else if (g_fetchedTablesCount == 1) {
//                // buffer the stats table for freq 200
//                g_stats200Buffer.push_back(msg);
//
//                if (msg.starts_with(" TOTAL:")) {
//                    g_fetchedTablesCount++;
//                }
//            }
//            else if (msg.find("MVP:") != -1) {
//                // fetch MVP and runner up
//                g_statsRanking = std::format("{} -- {}   ", msg.substr(0, msg.find(")") + 1),
//                    msg.substr(msg.find("--") + 3));
//
//                for (size_t i = g_stats100Buffer.size(); i < g_stats200Buffer.size(); i++) {
//                    g_stats100Buffer.push_back(std::string(83, ' '));
//                }
//
//                for (size_t i = g_stats200Buffer.size(); i < g_stats100Buffer.size(); i++) {
//                    g_stats200Buffer.push_back("");
//                }
//            }
//            else if (msg.find("LVP:") != -1) {
//                // fetch LVP and runner up
//                g_statsRanking += std::format("{} -- {}   ", msg.substr(0, msg.find(")") + 1),
//                    msg.substr(msg.find("--") + 3));
//                g_isFetchingStats = false;
//
//                if (g_stats100Buffer.size() + 1 > g_chatLinesCount) {
//                    // print a lean stats table
//                    sendPublic(std::format("?lines={}", g_stats100Buffer.size() + 1));
//                    sleep(200);
//                }
//
//                std::list<std::string>::iterator it100{ g_stats100Buffer.begin() };
//                std::list<std::string>::iterator it200{ g_stats200Buffer.begin() };
//
//                for (int32_t i = 1; it100 != g_stats100Buffer.end(); i++) {
//                    if (i == 1) {
//                        sendPrivate(g_dzBotName, *it100);
//                    }
//                    else {
//                        sendPrivate(g_dzBotName, std::format("{}  |  {}", *it100, *it200));
//                    }
//
//                    if (i % 5 == 0)
//                        sleep(1200);
//                    else {
//                        sleep(200);
//                    }
//                    it100++;
//                    it200++;
//                }
//                sendPrivate(g_dzBotName, g_statsRanking);
//                // end of processing of events for this main loop cycle
//                isProcessed = true;
//            }
//        }
//    }
//}


/// <summary>
/// Check for the time to end full-screen chat to stop showing stats. This event handler applies 
/// for showing stats during and after a match.
/// </summary>
/// <param name="logMsg">Log message.</param>
/// <param name="timeStamp">Current system time.</param>
/// <param name="isProcessed">True, if processing is finished in this main loop cycle, otherwise
/// false.</param>
void checkEventEndShowStats(std::string_view logMsg, TimeStamp& timeStamp, bool& isProcessed)
{
    if (isProcessed) {
        // processing finished for this timeslot, try next time
        return;
    }

    if (g_isMatchGoing && g_isShowingStats && timeStamp - g_statsShowTimeStamp >= c_statsShowInterval) {
        g_isShowingStats = false;
        // return to the public team channel
        sendPublic("=8025");
        // reset the .items timestamp to not call it immediately
        g_itemsWaitTimeStamp = timeStamp;
        // end of processing of events for this main loop cycle
        isProcessed = true;
    }
}


///// <summary>
///// Check for the time to end full-screen chat to stop showing stats. This event handler applies 
///// for showing stats during and after a match.
///// </summary>
///// <param name="logMsg">Log message.</param>
///// <param name="timeStamp">Current system time.</param>
///// <param name="isProcessed">True, if processing is finished in this main loop cycle, otherwise
///// false.</param>
//void checkEventEndShowStats_(std::string_view logMsg, TimeStamp& timeStamp, bool& isProcessed)
//{
//    if (isProcessed) {
//        // processing finished for this timeslot, try next time
//        return;
//    }
//
//    if (g_isMatchGoing && g_isShowingStats && timeStamp - g_statsShowTimeStamp >= c_statsShowInterval) {
//        g_isShowingStats = false;
//        // hit ESC to end full-screen chat
//        sendKey("{ESC}");
//        // end of processing of events for this main loop cycle
//        isProcessed = true;
//    }
//}


/// <summary>
/// Check for the time interval to show the items.
/// </summary>
/// <param name="logMsg">Log message.</param>
/// <param name="timeStamp">Current system time.</param>
/// <param name="isProcessed">True, if processing is finished in this main loop cycle, otherwise
/// false.</param>
void checkEventShowItems(std::string_view logMsg, TimeStamp& timeStamp, bool& isProcessed)
{
    if (isProcessed) {
        // processing finished for this timeslot, try next time
        return;
    }

    if (g_isMatchGoing && timeStamp - g_itemsWaitTimeStamp >= c_itemsWaitInterval) {
        // take care to not show items while showing stats with full-screen chat
        if (!g_isShowingStats) {
            sendPrivate(g_dzBotName, ".items");
            // set a new timestamp for showing items next time
            g_itemsWaitTimeStamp = timeStamp;
            // end of processing of events for this main loop cycle
            isProcessed = true;
        }
    }
}


//////// Maintainance Event handler ////////

/// <summary>
/// Check if it's time to fetch the squadname of the next present player in the arena who does
/// not have a squadname yet.
/// </summary>
/// <param name="timeStamp">Current system time.</param>
void checkEventRequestSquadName(TimeStamp& timeStamp)
{
    if (!g_isFetchingPlayerNames && !g_arena.empty()
        && timeStamp - g_lastSquadQueryTimeStamp >= c_squadQueryInterval) {
        // we are not in the process of fetching player names and it's not the public arena
        fetchNextSquadName();
        g_lastSquadQueryTimeStamp = timeStamp;
    }
}


/// <summary>
/// Check if a message with a reply on ?squad <player> has been received. If so, store the squad 
/// name for the related player.
/// </summary>
/// <param name="logMsg"></param>
/// <param name="timeStamp">Current system time.</param>
/// <param name="isProcessed">True, if processing is finished in this main loop cycle, otherwise
/// false.</param>
void checkEventSquadName(std::string_view logMsg, TimeStamp& timeStamp, bool& isProcessed)
{
    if (isProcessed) {
        // processing finished for this timeslot, try next time
        return;
    }

    if (logMsg.starts_with("  Squad: ")) {
        // it's a reply to a ?squad <player> message
        std::string squadName{ logMsg.substr(9) };

        if (squadName.empty()) {
            // use a single blank for an empty squad name, so fetchNextSquadName() will skip it
            squadName = " ";
        }
        
        if (g_playerInfos.contains(g_squadFetchPlayerName)) {
            // ok, the player for whom we fetched the squad name is actually in the arena
            g_playerInfos[g_squadFetchPlayerName] = squadName;
        }
        g_squadInfos[g_squadFetchPlayerName] = std::pair(squadName, ++g_squadUpdateCount);
        updatePlayersMessage();
        isProcessed = true;
    }
    else if (logMsg.find("commands flooding") != -1) {
        // have a break to prevent getting kicked for command flooding
        c_squadQueryInterval = c_squadQueryInterval*2;
        logDiscordBot("Bot warned for message flooding!");
    }
    else if (logMsg.find("Unknown player") != -1) {
        // for some reason the squad name of an unknown player was requested, this can happen 
        // if not all AHK characters were replaced in a player name or if the bot sent a message
        // that ended with " in arena", log the name and just remove it from the player list
        logDiscordBot("Failed to request squad name for player: " + g_squadFetchPlayerName);
        g_playerInfos.erase(g_squadFetchPlayerName);
        g_squadInfos.erase(g_squadFetchPlayerName);
        isProcessed = true;
    }
}


/// <summary>
/// Check if a player has entered the arena to update the player info map and activity timestamp.
/// </summary>
/// <param name="logMsg">Log message.</param>
/// <param name="timeStamp">Current system time.</param>
void checkEventPlayerEnteringArena(std::string_view logMsg, TimeStamp& timeStamp)
{
    if (!g_arena.empty() && logMsg.find(">") == -1 && logMsg.ends_with(" entered arena")) {
        // a player entered the arena and it's not the public arena
        std::string playerName{ logMsg.substr(2, logMsg.find(" entered arena") - 2) };

        if (!g_squadInfos.contains(playerName)) {
            g_playerInfos[playerName] = "";
            g_squadInfos[playerName] = std::pair("", 0);
        }
        else {
            g_playerInfos[playerName] = g_squadInfos[playerName].first;
        }

        g_lastActivityTimeStamp = timeStamp;
        updatePlayersMessage();
    }
}


/// <summary>
/// Check if a player has left the arena to update the player info map and activity timestamp.
/// </summary>
/// <param name="logMsg">Log message.</param>
void checkEventPlayerLeavingArena(std::string_view logMsg)
{
    if (!g_arena.empty() && logMsg.find(">") == -1 && logMsg.ends_with(" left arena")) {
        // a player left the arena and it's not the public arena
        std::string playerName{ logMsg.substr(2, logMsg.find(" left arena") - 2) };

        g_playerInfos.erase(playerName);
        g_team100Stats.erase(playerName);
        g_team200Stats.erase(playerName);
        updatePlayersMessage();
    }
}


/// <summary>
/// Check if a new arena has been entered to issue a ?find discobot command.
/// </summary>
/// <param name="logMsg">Log message.</param>
/// <param name="isProcessed">True, if processing is finished in this main loop cycle, otherwise
/// false.</param>
void checkEventBotEnteringArena(std::string_view logMsg, bool& isProcessed)
{
    if (isProcessed) {
        // processing finished for this timeslot, try next time
        return;
    }

    if (logMsg.find(std::format("> Welcome {}", c_discordBotName)) != -1
        || logMsg.find("Use ?next to play in the next") != -1) {
        //if (g_isRecordingEnabled) {
        //    // when reporting, we are not in the Subspace chat window, so hit page-down once to
        //    // show the player list, needed for the following name ticks
        //    sendKey("{PgDn}");
        //    sendKey("{PgDn}");
        //}

        // issue a find command to be processed in checkEventArenaName
        sendPublic(std::format("?find {}", c_discordBotName));
        g_isFindIssued = true;
//        g_lastTickName = "";
        isProcessed = true;
    }
}


/// <summary>
/// Check if a reply to a ?find discobot-x command has been received to parse the arena name.
/// </summary>
/// <param name="logMsg">Log message.</param>
void checkEventArenaName(std::string_view logMsg)
{
    if (g_isFindIssued && 
          (logMsg.find(std::format("  {}", c_discordBotName)) != -1 && logMsg.find(" - ") != -1) 
        || logMsg.find(std::format("  {} is in arena", c_discordBotName)) != -1) {  // Nexus
        // get the arena name
        if (!g_isNexusEnabled)
            g_arena = logMsg.substr(logMsg.find(" - ") + 3);
        else {
            g_arena = logMsg.substr(c_discordBotName.length() + 15);
            g_arena = g_arena.substr(0, g_arena.length() - 1);
        }

        writePersistentDataFile();
        // set the chat channel Id for message transfer depending on the arena
        updateChatChannelId();

        // determine the index of the current arena in c_switchArenas
        int32_t i{};

        g_arenaIndex = -1;

        for (std::string_view arena : c_switchArenas) {
            if (arena == g_arena) {
                g_arenaIndex = i;
                break;
            }
            i++;
        }

        if (!g_isContinuumEnabled && !g_isRecordingEnabled) {
            // initiate fetching player names of this arena
            fetchNextPlayerName();
        }

        //// determine the DZ bot's name depending on the arena
        //static std::set<std::string> dz_bot_arenas{ "4v4a", "4v4b" , "4v4c" , "4v4d",
        //    "4v4handicap", "4v4prac1", "4v4prac2", "4v4squad1", "4v4squad2", "4v4squad3" };
        //static std::set<std::string> sbt_bot_arenas{ 
        //    "4v4sbt1", "4v4sbt2" , "4v4sbt3" , "4v4sbt4", "4v4pracopen", "2v2sbt1", "2v2sbt2" };

        //if (dz_bot_arenas.contains(g_arena)) {
        //    g_dzBotName = "DZ-4v4";
        //}
        //else if (sbt_bot_arenas.contains(g_arena)) {
        //    g_dzBotName = "4v4Bot";
        //}
        //else {
        //    g_dzBotName = "unknown_bot";
        //}
        // !not necessary any more, since the 4v4 bot name now always starts with "4v4bot"
        g_isFindIssued = false;
    }
}


/// <summary>
/// Check if a message with a ticked name has been received. If so, add a player info to the map.
/// In case the last name has been ticked and thne arena does not have players, switch the arena.
/// </summary>
/// <param name="logMsg"></param>
/// <param name="timeStamp">Current system time.</param>
/// <param name="isProcessed">True, if processing is finished in this main loop cycle, otherwise
/// false.</param>
void checkEventTickedName(std::string_view logMsg, TimeStamp& timeStamp, bool& isProcessed)
{
    if (isProcessed) {
        // processing finished for this timeslot, try next time
        return;
    }

    if (logMsg.find("> ") != -1) {
        // it's a private message
        std::string msgName{ logMsg.substr(2, logMsg.find(">")) };

        if (msgName.find(c_discordBotName) != -1 && logMsg.ends_with(" in arena")) {
            // it's an %tickname message the bot sent privately to the DZ bot
            std::string msg{ logMsg.substr(logMsg.find("> ") + 2) };
            std::string tickName{ msg.substr(0, msg.find(" in arena")) };

            if (tickName == g_lastTickName) {
                // same ticked name received, we are at the end of the namelist
                g_initialPlayerCount = g_playerInfos.size();
                g_lastActivityTimeStamp = timeStamp;
                g_isFetchingPlayerNames = false;

                if (g_playerInfos.size() == 0) {
                    // only bots in this arena, switch to the next arena
                    switchArena(true);
                    // reset the players table message
                    g_team100Stats.clear();
                    g_team200Stats.clear();
                }
                updatePlayersMessage();
            }
            else {
                // store a player info for the ticked name, in case it's not a bot name
                if (!tickName.starts_with(g_dzBotName)
                    && !tickName.starts_with(c_discordBotName)) {
                    if (!g_squadInfos.contains(tickName)) {
                        g_squadInfos[tickName] = std::pair("", 0);
                        g_playerInfos[tickName] = "";
                    }
                    else {
                        // add the new player with squad name to the list of players
                        g_playerInfos[tickName] = g_squadInfos[tickName].first;
                    }
                }
                g_lastTickName = tickName;
                fetchNextPlayerName();
            }
            isProcessed = true;
        }
    }
}


/// <summary>
/// Check if a message with a ticked name has been received. If so, add a player info to the map.
/// In case the last name has been ticked and thne arena does not have players, switch the arena.
/// </summary>
/// <param name="logMsg"></param>
/// <param name="timeStamp">Current system time.</param>
/// <param name="isProcessed">True, if processing is finished in this main loop cycle, otherwise
/// false.</param>
void checkEventTickedNameNexus(std::string_view logMsg, TimeStamp& timeStamp, bool& isProcessed)
{
    if (isProcessed) {
        // processing finished for this timeslot, try next time
        return;
    }

    std::string searchStr{ std::format("P :{}:", g_dzBotName) };

    if (logMsg.find(searchStr) != -1 && logMsg.ends_with(" in arena")) {
        // it's an %tickname message the bot sent privately to the DZ bot
        std::string msg{ logMsg.substr(searchStr.length()) };
        std::string tickName{ msg.substr(0, msg.find(" in arena")) };

        if (tickName == g_lastTickName) {
            // same ticked name received, we are at the end of the namelist
            g_initialPlayerCount = g_playerInfos.size();
            g_lastActivityTimeStamp = timeStamp;
            g_isFetchingPlayerNames = false;

            if (g_playerInfos.size() == 0) {
                // only bots in this arena, switch to the next arena
                switchArena(true);
                // reset the players table message
                g_team100Stats.clear();
                g_team200Stats.clear();
            }
            updatePlayersMessage();
        }
        else {
            // store a player info for the ticked name, in case it's not a bot name
            if (!tickName.starts_with(g_dzBotName)
                && !tickName.starts_with(c_discordBotName)) {
                if (!g_squadInfos.contains(tickName)) {
                    g_squadInfos[tickName] = std::pair("", 0);
                    g_playerInfos[tickName] = "";
                }
                else {
                    // add the new player with squad name to the list of players
                    g_playerInfos[tickName] = g_squadInfos[tickName].first;
                }
            }
            g_lastTickName = tickName;
            fetchNextPlayerName();
        }
        isProcessed = true;
    }
}


/// <summary>
/// Check the last activity timeout for switching arenas.
/// </summary>
/// <param name="timeStamp">Current system time.</param>
void checkEventSwitchArena(TimeStamp& timeStamp)
{
    if (!g_isFetchingPlayerNames 
        && timeStamp - g_lastActivityTimeStamp >= c_switchTimeoutInterval) {
        if (g_playerInfos.size() < 8 || g_initialPlayerCount >= g_playerInfos.size()) {
            switchArena(true);
        }
        else {
            // inactivity timeout exceeded, but more players have entered the arena, so just 
            // reset the initial player count and activity timestamp
            g_initialPlayerCount = g_playerInfos.size();
            g_lastActivityTimeStamp = timeStamp;
        }
    }
}


/// <summary>
/// Check if the reply to an !sbtline command has been received from the DZ bot. If so, the 
/// information is evaluated and the spam propagated to Discord.
/// </summary>
/// <param name="logMsg">Log message.</param>
void checkEventSpamInfoReceived(std::string_view logMsg)
{
    if (g_isSpamInfoIssued) {
        size_t approxNeedIx{ logMsg.find("APPROXIMATELY Need") };

        if (approxNeedIx != -1) {
            std::string neededCountStr{ logMsg.substr(approxNeedIx + 19, 1) };
            std::string playerStr{ neededCountStr == "1" ? "player" : "players" };

            if (neededCountStr == "-") {
                neededCountStr = logMsg.substr(approxNeedIx + 19, 2);
            }
            sendRoleMention(g_arena, 
                std::format("{} {} needed for a 4v4 practice in ?go {} --{}", neededCountStr, 
                    playerStr, g_arena, g_spamPlayerName));

            if (neededCountStr != "0")
                sendPublic(std::format("A spam message has been forwarded to Discord channel "
                    "4v4prac. We need {} more {}.", neededCountStr, playerStr));
            else
                sendPublic("A spam message has been forwarded to Discord channel 4v4prac. "
                    "We need no more players.");
            g_isSpamInfoIssued = false;
        }
    }
}


/// <summary>
/// Check if the game client lost connection and in this case quit Subspace and shutdown the bot.
/// </summary>
/// <param name="logMsg">Log message.</param>
/// <param name="isProcessed">True, if processing is finished in this main loop cycle, otherwise
/// false.</param>
void checkEventConnectionLost(std::string_view logMsg, bool& isProcessed)
{
    if (isProcessed) {
        // processing finished for this timeslot, try next time
        return;
    }

    if (logMsg.find("WARNING: Disconnected from server") != -1) {
        logDiscordBot(logMsg);
        //reconnect();  reconnect doesn't work, so we just shut down the bot!
        shutdown(true);
        isProcessed = true;
    }
}


/// <summary>
/// Read the Operators.txt file and update the operator list.
/// </summary>
/// <param name="timeStamp">Current system time.</param>
void checkEventUpdateOperators(TimeStamp& timeStamp)
{
    if (timeStamp - g_updateOpsTimeStamp >= CounterDuration{ 5 }) {
        readOperators();
        g_updateOpsTimeStamp = timeStamp;
    }
}


//////// Event processing ////////

/// <summary>
/// Process events for a log message.
/// </summary>
/// <param name="logMsg">Log message.</param>
/// <returns>True, if an event has been processed, otherwise false.</returns>
export bool processEvents(std::string_view logMsg)
{
    TimeStamp timeStamp{ std::chrono::system_clock::now() };
    bool isProcessed{};

    try {
        // flush the arena message buffer
        handleEventTick(timeStamp);
        // propagate public, team and arena messages to Discord
        handleEventSendToDiscord(logMsg);

        // check if a new arena has been entered to issue a ?find discobot-x command
        checkEventBotEnteringArena(logMsg, isProcessed);
        // check if a reply to a ?find discobot-x command has been received
        checkEventArenaName(logMsg);
        // check for an update of the operator list
        checkEventUpdateOperators(timeStamp);

        if (!g_isContinuumEnabled) {
            if (g_isReportingEnabled || g_isRecordingEnabled) {
                // check for match starting info to extract game title
                checkEventMatchStartInfo(logMsg);
            }

            if (g_isReportingEnabled) {
                // check if a message with a ticked name has been received
                if (!g_isNexusEnabled) {
                    checkEventTickedName(logMsg, timeStamp, isProcessed);
                }
                // check if a message with a ticked name has been received on Nexus server
                else {
                    checkEventTickedNameNexus(logMsg, timeStamp, isProcessed);
                }
                // check if a player has entered the arena to update the activity timestamp
                checkEventPlayerEnteringArena(logMsg, timeStamp);
                // check if a player has left the arena to update the player info map and activity
                checkEventPlayerLeavingArena(logMsg);
                // check if it's time to fetch the squadname of the next present player
                checkEventRequestSquadName(timeStamp);
                // check if a message with a reply on ?squad <player> has been received
                checkEventSquadName(logMsg, timeStamp, isProcessed);
                // check the last activity timeout for switching arenas
                checkEventSwitchArena(timeStamp);
                // check if the reply to an !sbtline command has been received from the DZ bot
                checkEventSpamInfoReceived(logMsg);
                // check if it's time to send an .item request to obtain player names and items
                checkEventRequestResults(logMsg, timeStamp);
                // check for .items info to update players stats
                if (!g_isNexusEnabled) {
                    checkEventPlayerStatsUpdate(logMsg, timeStamp, isProcessed);
                }
                // check for ?items info to update players stats on Nexus server
                else {
                    checkEventPlayerStatsUpdateNexus(logMsg, timeStamp, isProcessed);
                }
                // check for the end of a match to stop reporting stats
                checkEventEndReporting(logMsg);
            }

            if (g_isRecordingEnabled) {
                // check for match start to begin recording
                checkEventMatchStartRecording(logMsg, timeStamp);
                // check for match end
                checkEventMatchEnd(logMsg, timeStamp);
                // check for end of showing stats after match end to stop recording
                checkEventMatchEndRecording(logMsg, timeStamp);
                // check for a sub info to store a related review mark
                checkEventSubInfo(logMsg);
                // check for a new score to store a related review mark
                checkEventNewScore(logMsg, timeStamp);
                // check for the time interval to show the items
                checkEventShowItems(logMsg, timeStamp, isProcessed);
                // check for the time to show stats
                //checkEventShowStats(logMsg, timeStamp, isProcessed);
                // check if the stats have been shown during a match to print a reformatted table
                //checkEventStatsShown(logMsg, timeStamp, isProcessed);
                // check for the time to end full-screen chat to stop show stats
                //checkEventEndShowStats(logMsg, timeStamp, isProcessed);

                // preserve the last few messages for later use
                g_logBuffer[1] = g_logBuffer[0];
                g_logBuffer[0] = logMsg;
            }

            // check if the game client lost connection, in this case shutdown
            checkEventConnectionLost(logMsg, isProcessed);
        }

        if (g_isRestarted) {
            // a restart has been requested with the !restart command, just shut down the bot and
            // let the monitor app restart it after a minute
            shutdown(true);
        }
    }
    catch (std::exception& ex) {
        logDiscordBot(ex.what());
    }

    return isProcessed;
}

