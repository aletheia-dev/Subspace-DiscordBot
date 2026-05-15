module;
#pragma warning(disable: 4251)
#include <dpp/dpp.h>
#include <filesystem>
#include <ranges>
#include <utility>

export module Discord;

import Algorithms;
import Commands;
import Config;
import Subspace;

import <format>;
import <iostream>;
import <list>;
import <ranges>;
import <set>;
import <vector>;
import <utility>;


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
    bool isLaggedOut{};
};

/// <summary>
/// Subspace chat mode.
/// </summary>
export enum class ChatMode
{
    Arena,
    Public,
    Team,
    Private
};

// pair of Discord user name and DM channel Id
export typedef std::pair<std::string, dpp::snowflake> DMChannelInfo;
// map of player name as key and DMChannelInfo as value
export typedef std::map<std::string, DMChannelInfo> DMChannelMap;
// map of player name as key and player stats as value
export typedef std::map<std::string, PlayerStats> TeamStats;


// constants
//
// max allowed length of a Discord message
const uint32_t c_maxDiscordMessageLength{ 1970 };
// prefixes for bot commands
const std::set<char> c_commandPrefixes{ '.', '!', '@', '/' };
const std::set<std::string> c_unlinkedCommands{ "help", "about", "version" };
// Duration in seconds to pause between .items info request for reporting player stats
export CounterDuration c_itemsRequestInterval{ 4 };
// Duration in seconds to pause between ?lsq info request for queued players on Nexus
export CounterDuration c_lsqRequestInterval{ 4 };


// Discord config objects - INI file
//
// Discord bot token
std::string c_botToken;
// Discord server Id
uint64_t c_serverId{};
// Id of the Discord channel for message transer from the current arena
uint64_t c_chatChannelId{};
// Id of the Discord channel for posting the player stats table
uint64_t c_tableChannelId{};
// Ids of the Discord channels for propagation of chat messages from specific arenas
std::map<std::string, uint64_t> c_chatChannelIds{};
// Id of the Discord prac channel
uint64_t c_pracChannelId{};
// Discord icon Id for a public message
uint64_t c_publicMessageIconId{};
// Discord icon Id for a team message
uint64_t c_teamMessageIconId{};
// mentions that should be formatted for Discord
std::list<std::string> c_mentionWhitelist;
// mentions that must not appear in Discord
std::list<std::string> c_mentionBlacklist;


// Discord config objects - MessageBlacklist file
//
// messages that must not be propagated to Discord
std::list<std::string> g_messageBlacklist;  // substrings of blacklisted messages from file
std::list<std::string> g_obligatoryBlacklist;  // hard-coded substrings of blacklisted messages


// State variables
//
// Id of the Discord channel for message transfer
export uint64_t g_chatChannelId{};
// Private direct message channels, key is player name
export DMChannelMap g_privateDMChannels;
// Timestamp of the last sent message to Discord, used to check the time interval for sending
export TimeStamp g_messageSentTimestamp;
// Timestamp for pausing after a prac or match announcement has been sent
export TimeStamp g_announceSentTimeStamp;
// Time interval in seconds to check for sending messages to Discord
CounterDuration g_messageSendInterval{ 1 };
// Time interval in seconds to check for sending a prac or match announcement to Discord
CounterDuration g_announceSendInterval{ 60*15 };
// If true, the player stats should be updated due to an entering or leaving player
bool g_pendingPlayerStatsUpdate{};
// Buffer for a message containing a link, used for printing stats tables
std::string g_gameStatsUrl;
// Buffer for the name of a player who issued a !spam command
export std::string g_spamPlayerName;
// Flag to indicate if a !sbtline command das been sent to the DZ bot
export bool g_isSpamInfoIssued{};
// Buffer for the last transfered arena message to detect new spam messages
export std::string g_lastArenaMessage{};
// Flag to indicate if the first team freq as reply to an ?items request has been parsed
export bool g_isFirstItemsFreqParsed{};
// Timestamp for the last ?lsq request
export TimeStamp g_lastLsqRequestTimeStamp;
// Current counter for parsing the response to ?lsq (list queue) on Nexus
export uint32_t g_parseLsqCount{};
// Current players in the practice queue
export std::list<std::string> g_playerPracQueue;

// Reporting state variables
//
// Names of team 100 and team 200 in case of a match
export std::string g_team100Name{ "100" };
export std::string g_team200Name{ "200" };
// Stats of team 100 and team 200 players
export TeamStats g_team100Stats;  // player name as key
export TeamStats g_team200Stats;  // player name as key
// Timestamp for the last .items request
export TimeStamp g_lastItemsRequestTimeStamp;
// Flag to indicate if reporting is currently done during a match
export bool g_isReporting{};

// Players table message state variables
//
// Current players table message text of the player message in the discobot channel
std::string g_lastUpdatedPlayersMessageChat;
// Current players table message text of the player message in the player-table channel
std::string g_lastUpdatedPlayersMessageTable;
// Current players table message in the discobot channel
dpp::message g_chatChPlayersMessage;
// Current players table message in the player-table channel
dpp::message g_tableChPlayersMessage;

// Container
dpp::role_map g_roles;  // Discord roles for translating mentions
std::map<std::string, dpp::snowflake> g_linkRequests;
//dpp::members_container m_verifiedMembers;

// Dynamic objects
dpp::cluster* g_discordCluster{};
dpp::commandhandler* g_commandHandler{};

// Message handling
std::list<std::string> g_arenaMessageBuffer;  // buffer for arena messages
bool g_isTableStarted = false;  // true, if processing of a stats table has started
bool g_isTableHeaderPrinted = false;  // true, if processing of a stats table header has finished
bool g_isTableBodyPrinted = false;  // true, if processing of a stats table body has finished
std::string g_tableSeparator;  // stats table separator line


//////// Discord to Subspace ////////

/// <summary>
/// Replace all mentions of groups in a text message with the corresponding role Ids.
/// </summary>
/// <param name="msg">Text message.</param>
/// <returns>Message with replaced mentions.</returns>
std::string replaceRoleMentionsToIDs(std::string_view msg)
{
    std::string newMsg{ msg };

    if (msg.find(" @") != std::string::npos) {
        for (auto& roleInfo : g_roles | std::views::values) {
            size_t ix = newMsg.find(" @" + roleInfo.name);

            if (ix != std::string::npos) {
                std::string repRoleId = std::format("<@&{}>", roleInfo.id.str());

                newMsg = newMsg.replace(ix, roleInfo.name.length() + 2, repRoleId);
            }
        }
    }

    return newMsg;
}


/// <summary>
/// Replace all user Ids in a text message with the corresponding user names.
/// </summary>
/// <param name="msg">Text message.</param>
/// <returns>Message with replaced user Ids.</returns>
std::string replaceUserIDsToMentions(std::string_view msg)
{
    std::string newMsg{ msg };
    size_t ix = msg.find("<@");

    if (ix != std::string::npos) {
        std::string userId{ msg.substr(ix + 2, msg.find(">", ix) - ix - 2) };
        std::string name{ getLinkedUserName(dpp::snowflake(userId)) };

        if (!name.empty()) {
            newMsg = newMsg.replace(ix, userId.length() + 3, std::format("@{}", name));
        }

        ix = msg.find("<@");
    }

    return newMsg;
}


/// <summary>
/// Discord message creation callback. This function is called when a Discord user enters a text
/// in the observed Discord channel
/// </summary>
/// <param name="evt">Message creation event.</param>
void messageCreateCallback(const dpp::message_create_t& evt)
{
    if (evt.msg.author.username != g_discordCluster->me.username) {
        // it's either public or a direct message that has been created by a user
        std::string msg = replaceUserIDsToMentions(evt.msg.content);
        std::string name = getLinkedUserName(evt.msg.author.id);
        
        if (name.empty()) {
            // if a user is not linked, apply the nickname or as a last resort the user name
            //if (!evt.msg.member.nickname.empty())
            //    name = evt.msg.member.nickname;
            //else
                name = evt.msg.author.username;
        }

        if (!name.empty()) {
            if (g_chatChannelId == evt.msg.channel_id) {
                // the message has been created in the Discord channel for this arena
                if (msg.starts_with("//")) {
                    // checked team message first, as we filter the slash out in the check below
                    sendTeam(std::format("{}> {}", name, msg.substr(2)));
                }
                else if (!msg.starts_with('`') && !msg.starts_with(':')
                    && !c_commandPrefixes.contains(msg[0]) && msg[0] != '?') {
                    // ignore self-created messages that start with special characters as well 
                    // as all messages that start with a subspace server command character '?'
                    sendPublic(std::format("{}> {}", name, msg));
                }
            }
            else {
                // if the message has been created in a direct message channel to the bot and 
                // there exists a private communication with a player, then propagate the message
                // to that player
                for (auto& [player, DMChannelInfo] : g_privateDMChannels) {
                    if (DMChannelInfo.second == evt.msg.channel_id) {
                        sendPrivate(player, std::format("{}> {}", DMChannelInfo.first, msg));
                    }
                }
            }
        }
    }
    else if (!g_chatChPlayersMessage.id && evt.msg.content == g_lastUpdatedPlayersMessageChat) {
        // it's an initial player table posted by the bot in the discibit channel, store the 
        // message and it's id
        g_chatChPlayersMessage = evt.msg;  // the message object is needed for updating
        g_chatChPlayersMessageId = evt.msg.id;  // used to provide a persistant message Id

        // pin the playerlist message for the chat channel
        g_discordCluster->message_pin(g_chatChannelId, evt.msg.id);
    }
    else if (!g_tableChPlayersMessage.id && evt.msg.content == g_lastUpdatedPlayersMessageTable) {
        // it's an initial player table posted by the bot in the player-table channel, store the 
        // message and it's id
        g_tableChPlayersMessage = evt.msg;  // the message object is needed for updating
        g_tableChPlayersMessageId = evt.msg.id;  // used to provide a persistant message Id
    }
    else if (evt.msg.content.starts_with("**")) {
        // it's a private message from a Subspace player that is mimicked as a direct message
        // from the Discord bot
        size_t ix = evt.msg.content.find(">");

        if (ix != std::string::npos) {
            // extract his name from the message
            std::string player{ resolvePlayerName(evt.msg.content.substr(2, ix - 2)) };
            // store the id of the direct message channel to the bot
            g_privateDMChannels[player].second = evt.msg.channel_id;
        }
    }
};


/// <summary>
/// Format message to show public and team message icons in Discord.
/// </summary>
/// <param name="msg">Text message.</param>
/// <param name="chatMode">Chat mode.></param>
/// <param name="player">Player name.</param>
/// <returns>Text message formatted for Discord.</returns>
std::string formatDiscordMessage(std::string_view msg, ChatMode chatMode,
    std::string_view player = "")
{
    // truncate the message to it's maximum length and convert it to ascii
    std::string formatMsg = convertStringToASCII(msg.substr(0, c_maxDiscordMessageLength));

    // send the message to discord in accordance with it's type
    if (chatMode == ChatMode::Arena) {
        // 'Disconnected from server' messages have a mention added, so don't format them
        if (formatMsg.find("WARNING: Disconnected from server") == std::string::npos) {
            formatMsg = std::format("```ansi\n{}```", formatMsg);
        }
    }
    else if (!player.empty()) {
        formatMsg = std::format("**{}> **{}", player, replaceRoleMentionsToIDs(formatMsg));

        if (chatMode == ChatMode::Public) {
            if (c_publicMessageIconId) {
                formatMsg = std::format("<:publicmsg:{}>{}", c_publicMessageIconId, formatMsg);
            }
        }
        else if (chatMode == ChatMode::Team) {
            if (c_teamMessageIconId) {
                formatMsg = std::format("<:teammsg:{}>{}", c_teamMessageIconId, formatMsg);
            }
        }
    }

    return formatMsg;
}


/// <summary>
/// Handle a slash command that has been entered in Discord.
/// </summary>
/// <param name="cmd">Command.</param>
/// <param name="params">Parameter definition.</param>
/// <param name="src">Command source.</param>
void handleDiscordCommand(const dpp::slashcommand_t& evt)
{
    // get the discord user name
    dpp::snowflake userId{ evt.command.usr.id };
    std::string player{ getLinkedUserName(userId) };

    if (player.empty()) {
        //if (!evt.command.member.nickname.empty())
        //    player = evt.command.member.nickname;
        //else
            player = evt.command.usr.username;
    }

    std::string command{ std::get<dpp::command_interaction>(evt.command.data).name };
    MessageList retMessages;

    for (auto& [cmdHandler, cmdInfo] : getCommandInfos(command)) {
        // make sure that subspace bot commands can only be issued by a linked player
        if (isLinkedUser(player) || c_unlinkedCommands.contains(command)) {
            // gather the command parameters, switches are not allowed for discord parameters
            std::string cmdLine{ command };
            std::vector<dpp::command_data_option> params{
                std::get<dpp::command_interaction>(evt.command.data).options };
            bool addSeparator{};

            for (const dpp::command_data_option& param : params) {
                cmdLine += addSeparator ? ":" : " ";
                addSeparator = true;
                cmdLine += std::get<std::string>(param.value);
            }

            // execute the command
            Command cmd(cmdLine, isFinalOnlyCommand(command));

            retMessages = executeCommand(player, cmd, CommandScope::External, userId);
        }
        else {
            retMessages.push_back("You need to /link your discord account to your player name "
                "to be able to execute Subspace bot commands.");
        }

        break;
    }

    if (retMessages.size()) {
        // the ephemeral reply to the user will be in arena message format
        std::string formatMsg{ formatDiscordMessage(join(retMessages), ChatMode::Arena) };

        evt.reply(dpp::message(formatMsg).set_flags(dpp::m_ephemeral));
    }
}


//////// Config Files ////////

/// <summary>
/// Read configuration parameters for this module from the plugin configuration file.
/// </summary>
/// <param name="fileName">Configuration file name.</param>
void readConfigParams(std::string_view fileName)
{
    // obtain the folder path of the bot dll and read the config parameters
    std::string filePath{ (std::filesystem::current_path() / fileName).string() };

    readConfigParam("Discord", "BotToken", c_botToken, filePath);
    readConfigParam("Discord", "ServerId", c_serverId, filePath);
    readConfigParam("Discord", "ChatChannelId", c_chatChannelId, filePath);
    readConfigParam("Discord", "TableChannelId", c_tableChannelId, filePath, "0");
    readConfigParam("Discord", "ChatChannelIds", c_chatChannelIds, filePath, "");
    readConfigParam("Discord", "PracChannelId", c_pracChannelId, filePath, "0");
    readConfigParam("Discord", "PublicMessageIconId", c_publicMessageIconId, filePath, "0");
    readConfigParam("Discord", "TeamMessageIconId", c_teamMessageIconId, filePath, "0");
    readConfigParam("Discord", "MentionWhitelist", c_mentionWhitelist, filePath, "");
    readConfigParam("Discord", "MentionBlacklist", c_mentionBlacklist, filePath, "");
    // set the default chat channel Id, just in case
    g_chatChannelId = c_chatChannelId;
}


/// <summary>
/// Read the MessageBlacklist.txt file.
/// </summary>
void readMessageBlacklist()
{
    g_messageBlacklist.clear();
    readDataLines("MessageBlacklist.txt", [](std::string_view line) {
        if (trim(line).size()) g_messageBlacklist.push_back(trim(toLower(line))); });

    // add some obligatory blacklist messages
    g_obligatoryBlacklist.clear();
    g_obligatoryBlacklist.push_back(toLower(c_discordBotName)); // ?find request for the bot
    g_obligatoryBlacklist.push_back("this arena is continuum - only");  // initial server message
    g_obligatoryBlacklist.push_back("you have been disconnected");  // disconnect message
    g_obligatoryBlacklist.push_back("log file");  // reply to ?log
    g_obligatoryBlacklist.push_back("message name length");  // reply to ?namelen=..
    g_obligatoryBlacklist.push_back("message lines");  // reply to ?lines=..
    g_obligatoryBlacklist.push_back("public chat");  // reply to ?nopubchat
    g_obligatoryBlacklist.push_back("recording stopped");  // reply to ?nopubchat
    g_obligatoryBlacklist.push_back("squad:");  // reply to ?squad
    g_obligatoryBlacklist.push_back("unknown player");  // reply to ?squad
    g_obligatoryBlacklist.push_back("unknown server command");
    g_obligatoryBlacklist.push_back("player is not online");
}


/// <summary>
/// Store a new message sustring to the blacklist and write the blacklist to the file.
/// </summary>
/// <param name="msg">Substring of a message to be blacklisted.</param>
void storeBlacklistMessage(const std::string& msg)
{
    const std::set<std::string> excludeSet{
        "is ready for [4v4] prac to begin"
    };

    std::string lowerMsg{ toLower(msg) };

    // consider all messages with more than 80 characters that do not have a substring from the 
    // exclude set
    if (lowerMsg.length() < 80)
        return;

    for (const std::string& excludeMsg : excludeSet) {
        if (lowerMsg.find(excludeMsg) != std::string::npos)
            return;
    }

    // add the lower case message to the blacklist and append it to the blacklist message file
    g_messageBlacklist.push_back(lowerMsg);

    std::ofstream perstDataFile;
    std::string filePath{ "MessageBlacklist.txt" };

    perstDataFile.open(filePath, std::ios::out);

    if (perstDataFile.is_open()) {
        for (const std::string& blacklistMsg : g_messageBlacklist) {
            perstDataFile << blacklistMsg << std::endl;
        }
        perstDataFile.close();
    }
}


//////// Roles ////////

/// <summary>
/// Callback function to receive the Discord role map.
/// </summary>
/// <param name="evt">Callback event.</param>
void rolesGetCallback(const dpp::confirmation_callback_t& evt)
{
    g_roles = std::get<dpp::role_map>(evt.value);

    //// get verified user names to check abbreviations in private messages to discord users
    //for (auto& [roleId, userRole] : g_roles) {
    //    if (userRole.name == "verified") {
    //        m_verifiedMembers = userRole.get_members();
    //        break;
    //    }
    //}
}


/// <summary>
/// Get the role Id for s specified role name.
/// </summary>
/// <param name="role">Role name.</param>
/// <returns>Role Id for the specified role name or 0, if the name is unknown.</returns>
dpp::snowflake getRoleId(std::string_view role)
{
    for (auto& roleInfo : g_roles | std::views::values) {
        if (roleInfo.name == role) {
            return roleInfo.id;
        }
    }

    return 0;
}


//////// Messaging ////////

/// <summary>
/// Set the chat channel Id for message transfer depending on the current arena.
/// </summary>
export void updateChatChannelId()
{
    if (c_chatChannelIds.contains(g_arena)) {
        // it's a match arena, apply a specific Discord channel
        g_chatChannelId = c_chatChannelIds[g_arena];
    }
    else {
        // no match arena, apply the default Discord channel
        g_chatChannelId = c_chatChannelId;
    }
}


/// <summary>
/// Filter out arena messages that shall not be propagated to Discord.
/// </summary>
/// <param name="msg">Text message.</param>
/// <returns>Either the same message or an empty string in case the message shall not be 
/// sent.</returns>
std::string filterMessage(std::string_view msg)
{
    std::string filteredMsg{ msg };

    // filter out messages to discord that start with a command prefix
    if (filteredMsg.empty() 
        || c_commandPrefixes.contains(filteredMsg[0])) {
        return {};
    }

    // filter out messages that contain a blacklisted substring
    std::string lcFilteredMsg{ toLower(filteredMsg) };

    for (std::string_view ignoreSubstr : g_messageBlacklist) {
        if (lcFilteredMsg.find(ignoreSubstr) != std::string::npos) {
            return {};
        }
    }
    for (std::string_view ignoreSubstr : g_obligatoryBlacklist) {
        if (lcFilteredMsg.find(ignoreSubstr) != std::string::npos) {
            return {};
        }
    }
    
    //// filter out a connection loss message, if it contains no @discomod mention
    //if (filteredMsg.find("WARNING: Disconnected from server") != std::string::npos) {
    //    dpp::snowflake roleId{ getRoleId("Discomod") };

    //    if (roleId) {
    //        return std::format("{} <@&{}>", filteredMsg, roleId.str());
    //    }
    //}

    // filter out messages that contain blacklisted discord mentions
    size_t index = 0;

    for (std::string filterMention : c_mentionBlacklist) {
        while ((index = toLower(filteredMsg).find(filterMention, index)) != std::string::npos) {
            filteredMsg.replace(index, filterMention.length(), "");
        }
    }

    return filteredMsg;
}


/// Send a public, team or arena message to the specified discord channel. The message is 
/// additionally formatted to show an icon in Discord and truncated if necessary. For team and 
/// arena messages, the player is null.
/// </summary>
/// <param name="channelId">Id of the Discord channel to send the message to.</param>
/// <param name="msg">Text message.</param>
/// <param name="chatMode">Chat mode.</param>
/// <param name="player">Player name.</param>
/// <returns>Formatted message as sent to Discord.</returns>
std::string sendMessage(dpp::snowflake channelId, std::string_view msg, ChatMode chatMode,
    std::string_view player = "")
{
    if (msg.empty() || !channelId) {
        return std::string(msg);
    }

    // truncate the message to it's maximum length, convert it to ascii and format it for output 
    // in discord according the the chat mode
    std::string formatMsg{ formatDiscordMessage(msg, chatMode, player) };
    dpp::message discordMsg{ channelId, formatMsg };

    g_discordCluster->message_create(discordMsg);

    if (chatMode != ChatMode::Arena) {
        g_lastActivityTimeStamp = std::chrono::system_clock::now();
    }

    return formatMsg;
}


/// <summary>
/// Send a text with formatted role mention to the Discord channel.
/// </summary>
/// <param name="roleName">Role name.</param>
/// <param name="msg">Text message containing the role name.</param>
export void sendRoleMention(std::string_view roleName, std::string_view msg)
{
    dpp::snowflake roleId{ getRoleId(roleName) };

    if (roleId) {
        std::string roleMention{ std::format("<@&{}>", roleId.str()) };
        std::string newMsg{ replaceString(msg, roleName, roleMention) };

        if (c_pracChannelId) {
            sendMessage(c_pracChannelId, newMsg, ChatMode::Public);
        }
    }
    else {
        // the mentioned role could not be found, append the standard role to the message
        roleId = getRoleId("4v4prac");

        if (roleId) {
            std::string roleMention{ std::format("<@&{}>", roleId.str()) };
            std::string newMsg{ std::format("{} {}", msg, roleMention) };

            if (c_pracChannelId) {
                sendMessage(c_pracChannelId, newMsg, ChatMode::Public);
            }
        }
    }

    //if (roleId) {
    //    // must be sent as public message, otherwise the group mention will not be shown
    //    std::string roleMention{ std::format("<@&{}>", roleId.str()) };
    //    std::string newMsg{ replaceString(msg, roleName, roleMention) };

    //    sendMessage(c_pracChannelId, format("```{}```{}", newMsg, roleMention),
    //        ChatMode::Public);
    //    return;
    //}
    //else {
    //    // the mentioned role could not be found, append the standard role to the message
    //    roleId = getRoleId("4v4prac");

    //    if (roleId) {
    //        // must be sent as a public message, otherwise the role mention will not be shown
    //        //sendMessage(c_pracChannelId, format("```{}```<@&{}>", msg, roleId.str()),
    //        //    ChatMode::Public);
    //        sendMessage(c_pracChannelId, format("```{}```<@&{}>", msg, roleId.str()),
    //            ChatMode::Public);
    //    }
    //}
}


/// <summary>
/// Send all buffered arena messages to Discord. These arena messages get printed inside a box 
/// with constant font width.
/// </summary>
void flushArenaMessages()
{
    if (!g_arenaMessageBuffer.empty()) {
        sendMessage(g_chatChannelId, join(g_arenaMessageBuffer), ChatMode::Arena);
        g_arenaMessageBuffer.clear();
    }
}


/// <summary>
/// Process the arena message queue for the sending of formatted tables. Arena messages with 
/// tables are formatted by flushing the queue and inserting extra separator lines.
/// </summary>
/// <param name="msg">Text message.</param>
void processArenaMessages(std::string_view msg)
{
    bool flushMessage{};
    bool isStatsUrlMessage{};

    if (!g_isTableStarted) {
        // output of a final score, take care to show this in a separate message box
        if (msg.starts_with("+---")) {
            // upper border of a table, store it for use as a separator
            g_isTableStarted = true;
            g_tableSeparator = msg;
            flushArenaMessages();
        }
        else if (msg.starts_with("LVP: ")) {
            // flush the message queue at the end of mvp/lvp infos that follow a stats table
            flushMessage = true;
        }
        else if (msg.find("needed for a 4v4 practice") != std::string::npos) {
            // a reply to someone's !spam command, mention discord users of the practice group
            std::string helpRole{ msg.substr(msg.find("?go ") + 4) };
            std::string roleName{ helpRole.substr(0, helpRole.find(" ")) };

            sendRoleMention(roleName, msg);
        }
        else if (msg.starts_with("-1-") || msg.starts_with("-2-") || msg.starts_with("-3-")
            || msg.starts_with("GO!") || msg.starts_with("Score:")) {
            // separate the match countdown messages
            flushMessage = true;
        }
    }
    else {
        if (msg.starts_with("+---")) {
            if (!g_isTableHeaderPrinted) {
                // separator after a table header, now comes the table body
                g_isTableHeaderPrinted = true;
                g_isTableBodyPrinted = false;
            }
            else {
                // lower border of the current table
                g_isTableBodyPrinted = true;
            }
        }
        else if (msg.starts_with("| Freq ")
            // || msg.find("(") != std::string::npos  doesn't work for Nexus!
            ) {
            if (g_isTableBodyPrinted) {
                // another header within the same table, add another separator
                flushArenaMessages();
                g_arenaMessageBuffer.push_back(g_tableSeparator);
            }
            g_isTableHeaderPrinted = false;
        }
        else if (g_isTableHeaderPrinted && g_isTableBodyPrinted) {
            // the current line succeeds a finished table body and is not another header, the 
            // table is finished
            g_isTableHeaderPrinted = false;
            g_isTableBodyPrinted = false;
            g_isTableStarted = false;
            flushArenaMessages();

            if (msg.starts_with("Stats for this game")) {
                // store the message line with the game id Url to be printed after LVP message
                g_gameStatsUrl = msg.substr(msg.find("svssubspace.com"));
                // suppress the message line with the game id Url at this point
                isStatsUrlMessage = true;
            }
        }
    }

    // queue the current arena message and flush at the end of a stats table for faster output
    if (!isStatsUrlMessage) {  // skip the game stats url within a stats table
        g_arenaMessageBuffer.push_back(std::string(msg));
        g_lastArenaMessage = msg;
    }

    if (flushMessage) {
        flushArenaMessages();
    }

    if (msg.starts_with("LVP: ") && !g_gameStatsUrl.empty()) {
        // send the link-message as a public message after the table to make the link usable
        std::string hlinkMsg{ format("[>> Click for game stats <<](http://{})", g_gameStatsUrl) };

        sendMessage(g_chatChannelId, hlinkMsg, ChatMode::Public);
        g_gameStatsUrl = "";
    }
}


/// <summary>
/// Process the arena message queue for SpecialFunctions mode 'continuum', which is dedicated to
/// the Subspace Continuum Discord server.
/// </summary>
/// <param name="msg">Text message.</param>
void processArenaMessagesSFContinuum(std::string_view msg)
{
    std::string spamMsg;

    if (msg.find("needed for a 4v4 practice") != std::string::npos) {
        // a reply to someone's !spam command
        std::string helpArena{ msg.substr(msg.find("?go ") + 4) };
        std::string arenaName{ helpArena.substr(0, helpArena.find(" ")) };
        
        spamMsg = msg.substr(0, msg.find("--") - 1);
    }
    else if (std::chrono::system_clock::now() - g_announceSentTimeStamp > g_announceSendInterval) {
        // start of a practice or match
        if (msg.find("First team to score") != std::string::npos) {
            spamMsg = std::format("4v4 practice starting in ?go {}", g_arena);
            g_announceSentTimeStamp = std::chrono::system_clock::now();
        }
    }

    if (!spamMsg.empty()) {
        // propagate the message to Discord, mention discord users with the Pro League role
        std::string eventMsg{ std::format("<@&1313803018231087144> {}", spamMsg) };

        sendMessage(g_chatChannelId, eventMsg, ChatMode::Public);
    }
}


/// <summary>
/// Process and send a public, team or arena message received from the host to the configured 
/// discord channel.
/// </summary>
/// <param name="msg">Text message.</param>
/// <param name="chatMode">Chat mode.</param>
/// <param name="player">Player name.</param>
export void sendMessage(std::string_view msg, ChatMode chatMode, std::string_view player = "")
{
    if (chatMode == ChatMode::Public || chatMode == ChatMode::Team) {
        if (!g_isContinuumEnabled) {
            // not an arena message, so send all buffered arena messages to discord
            flushArenaMessages();
            // send the current message to discord
            sendMessage(g_chatChannelId, filterMessage(msg), chatMode, player);
            g_lastActivityTimeStamp = std::chrono::system_clock::now();
        }
    }
    else if (chatMode == ChatMode::Arena) {
        // store the arena message to the buffer until we receive a non-arena message
        std::string filteredMsg{ filterMessage(msg) };

        if (!g_lastArenaMessage.empty() && filteredMsg == g_lastArenaMessage) {
            // this arena message has already been sent, add it to the blacklisted arena messages
            storeBlacklistMessage(filteredMsg);
            filteredMsg = "";
        }

        if (!filteredMsg.empty()) {
            if (!g_isContinuumEnabled) {
                processArenaMessages(filteredMsg);
            }
            else {
                processArenaMessagesSFContinuum(filteredMsg);
            }
        }
    }
}


/// <summary>
/// If no table messages are currently being transmitted, send all buffered arena messages to 
/// Discord.
/// </summary>
void tryFlushArenaMessages()
{
    // flush only if our link to discord is up and we are not transmitting a table right now
    if (!g_isTableStarted) {
        flushArenaMessages();
    }
}


/// <summary>
/// Send a direct message to a Discord user.
/// </summary>
/// <param name="msg">Text message.</param>
/// <param name="player">Name of the issueing player.</param>
/// <param name="user">Name of the receiving user.</param>
void sendDirectMessage(std::string_view msg, std::string_view player, std::string_view user)
{
    if (isLinkedUser(user)) {
        dpp::snowflake userId{ getLinkedUserId(user) };
        std::string directMsg{ formatDiscordMessage(msg, ChatMode::Private, player) };

        g_discordCluster->direct_message_create(userId, dpp::message(directMsg));
    }
}


/// <summary>
/// Update a discord message.
/// </summary>
/// <param name="discordMsg">Discord message object to be updated.</param>
/// <param name="msg">Text message.</param>
void updateMessage(dpp::message& discordMsg, const std::string& msg)
{
    try {
        discordMsg.set_content(msg);
        g_discordCluster->message_edit(discordMsg);
    }
    catch (...) {
        // just for the case dpp causes a problem
    }
}


/// <summary>
/// Handle a private message from Subspace to the bot in case of an established private 
/// messaging connection to a Discord user.
/// </summary>
/// <param name="logMsg">Log message.</param>
/// <returns>True, if a reply has been sent, otherwise false.</returns>
export bool processPrivateDiscordMessage(std::string_view logMsg)
{
    std::string msg{ logMsg };
    std::string player;
    CommandScope scope;
    bool retVal{};

    // check if it's a private player message to the bot
    if (parsePrivateMessage(msg, player, scope)) {
        if (g_privateDMChannels.contains(player)) {
            // there already exists an active channel for direct messages to this discord
            // user, so just propagate the message
            sendDirectMessage(msg, player, g_privateDMChannels[player].first);
            return true;
        }
    }

    return false;
}


//////// Reporting ////////

/// <summary>
/// Create the players table message
/// </summary>
/// <param name="isForDiscobotChannel">True, if it's a table for the discobot channel, otherwise
/// for the player-table channel.</param>
/// <returns></returns>
MessageList getPlayersTable(bool isForDiscobotChannel)
{
    uint32_t tableWidth{ 31 };  // adjust when Discord changes the width of pinned messages

    if (!isForDiscobotChannel) {
        // introduce a slight difference in the table width for the player-table channel to be 
        // able to identify this message in messageCreateCallback
        tableWidth++;
    }

    uint32_t squadWidth{ 15 };
    uint32_t playerWidth{ tableWidth - squadWidth - 2 };
    MessageList retMessages;

    if (!g_isNexusEnabled)
        retMessages.push_back(format("\x1b[0mArena: \x1b[34m{}", g_arena));
    else
        retMessages.push_back(format("\x1b[0mArena: \x1b[2;37m{}", g_arena));

    // print the stats of team 100
    if (g_team100Stats.size() > 0) {
        std::string freqCountStr{ std::to_string(g_team100Stats.size()) };
        std::string teamName{ g_team100Name.substr(0, 15) };

        retMessages.push_back(format("\x1b[31m[{}]{}K--D---Re-Ro{} {}",
            teamName, std::string(playerWidth + 1 - teamName.length(), '-'),
            std::string(2 - freqCountStr.length(), '-'), freqCountStr));

        for (auto& [player, stats] : g_team100Stats) {
            if (!stats.isLaggedOut) {
                if (!stats.isSpectator) {
                    retMessages.push_back(format("\x1b[0m  {} {} {} {} {}",
                        padRight(player, playerWidth).substr(0, playerWidth),
                        padRight(std::to_string(stats.kills), 2),
                        padRight(std::to_string(stats.deaths), 3),
                        padRight(std::to_string(stats.repeller), 2),
                        padRight(std::to_string(stats.rockets), 2)));
                }
                else {
                    retMessages.push_back(format("\x1b[32ms \x1b[33m{} {} {}",
                        padRight(player, playerWidth).substr(0, playerWidth),
                        padRight(std::to_string(stats.kills), 2),
                        padRight(std::to_string(stats.deaths), 2)));
                }
            }
        }
    }

    // print the stats of team 200
    if (g_team200Stats.size() > 0) {
        std::string freqCountStr{ std::to_string(g_team200Stats.size()) };
        std::string teamName{ g_team200Name.substr(0, 15) };

        retMessages.push_back(format("\x1b[31m[{}]{}K--D---Re-Ro{} {}",
            teamName, std::string(playerWidth + 1 - teamName.length(), '-'),
            std::string(2 - freqCountStr.length(), '-'), freqCountStr));

        for (auto& [player, stats] : g_team200Stats) {
            if (!stats.isLaggedOut) {
                if (!stats.isSpectator) {
                    retMessages.push_back(format("\x1b[0m  {} {} {} {} {}",
                        padRight(player, playerWidth).substr(0, playerWidth),
                        padRight(std::to_string(stats.kills), 2),
                        padRight(std::to_string(stats.deaths), 3),
                        padRight(std::to_string(stats.repeller), 2),
                        padRight(std::to_string(stats.rockets), 2)));
                }
                else {
                    retMessages.push_back(format("\x1b[32ms \x1b[33m{} {} {}",
                        padRight(player, playerWidth).substr(0, playerWidth),
                        padRight(std::to_string(stats.kills), 2),
                        padRight(std::to_string(stats.deaths), 2)));
                }
            }
        }
    }

    // print the 8025 spectator frequency
    std::list<std::string> spectators;

    for (auto& [player, squadInfo] : g_playerInfos) {
        if (!player.starts_with(g_dzBotName) && !player.starts_with(c_discordBotName)
            && (!g_team100Stats.contains(player) || g_team100Stats[player].isLaggedOut)
            && (!g_team200Stats.contains(player) || g_team200Stats[player].isLaggedOut)) {
            spectators.push_back(player);
        }
    }

    if (spectators.size() > 0) {
        std::string specCountStr{ std::to_string(spectators.size()) };

        retMessages.push_back(std::format("\x1b[31m{} {}",
            std::string(tableWidth - specCountStr.length(), '-'), spectators.size()));

        // add spectators that are in the prac queue
        for (const std::string& player : g_playerPracQueue) {
            if (std::find(spectators.begin(), spectators.end(), player) ==
                spectators.end()) {
                continue;
            }

            std::string squadName{ trim(g_playerInfos[player]) };

            if (!squadName.empty()) {
                squadName = "\x1b[2;37m" + padRight(g_playerInfos[player],
                    squadWidth).substr(0, squadWidth);
            }

            retMessages.push_back(
                std::format("\x1b[34m> \x1b[34m{}\x1b[0m {}",
                    padRight(player, playerWidth).substr(0, playerWidth), squadName));
        }

        // add spectators that are not in the prac queue
        for (const std::string& player : spectators) {
            if (std::find(g_playerPracQueue.begin(), g_playerPracQueue.end(), player) != 
                g_playerPracQueue.end()) {
                continue;
            }

            std::string squadName{ trim(g_playerInfos[player]) };

            if (!squadName.empty()) {
                squadName = "\x1b[2;37m" + padRight(g_playerInfos[player], 
                    squadWidth).substr(0, squadWidth);
            }

            retMessages.push_back(
                std::format("\x1b[32ms \x1b[33m{}\x1b[0m {}",
                    padRight(player, playerWidth).substr(0, playerWidth), squadName));
        }
    }

    return retMessages;
}


export void updatePlayersMessage()
{
    // create or update the players message in the discobot channel
    if (g_chatChannelId) {
        MessageList replyMessages{ getPlayersTable(true) };

        if (!g_chatChPlayersMessage.id) {
            // send the playerlist as arena message, the message Id will be stored in the create-
            // message command handler on receiving a notification for this post
            g_lastUpdatedPlayersMessageChat = sendMessage(g_chatChannelId, join(replyMessages),
                ChatMode::Arena);
        }
        else {
            // update the players message in the chat channel
            std::string formatMsg{ formatDiscordMessage(join(replyMessages), ChatMode::Arena) };

            if (formatMsg != g_lastUpdatedPlayersMessageChat) {
                // if there is a new playerlist message, so update it
                updateMessage(g_chatChPlayersMessage, formatMsg);
                g_lastUpdatedPlayersMessageChat = formatMsg;
            }
        }
    }

    // create or update the players message in the player-table channel
    if (c_tableChannelId) {
        MessageList replyMessages{ getPlayersTable(false) };

        if (!g_tableChPlayersMessage.id) {
            // send the playerlist as arena message, the message Id will be stored in the create-
            // message command handler on receiving a notification for this post
            g_lastUpdatedPlayersMessageTable = sendMessage(c_tableChannelId, join(replyMessages),
                ChatMode::Arena);
        }
        else {
            // update the players message in the chat channel
            std::string formatMsg{ formatDiscordMessage(join(replyMessages), ChatMode::Arena) };

            if (formatMsg != g_lastUpdatedPlayersMessageTable) {
                // if there is a new playerlist message, so update it
                updateMessage(g_tableChPlayersMessage, formatMsg);
                g_lastUpdatedPlayersMessageTable = formatMsg;
            }
        }
    }
}


//////// Command handlers ////////

/// <summary>
/// Handle the player command 'pm'. Sends a private message from Subspace to a Discord user.
/// </summary>
/// <param name="player">Player name of the issuer.</param>
/// <param name="cmd">Command.</param>
/// <param name="scope">Command scope.</param>
/// <param name="userId">Discord user Id in case of commands sent from Discord.</param>
/// <returns>List of reply messages to the issuer of the command.</returns>
MessageList handleCommandPmSubspace2Discord(std::string_view player, const Command& cmd,
    CommandScope scope, uint64_t userId)
{
    MessageList retMessages;
    std::string msg{ cmd.getFinal() };

    if (msg.starts_with(":")) {
        // just in case someone types /:name:message instead of /name:message
        msg = msg.substr(1);
    }

    if (msg.find(':') != std::string::npos) {
        // the sending player is either linked or a linked user already initiated a chat
        std::vector<std::string> splitMsg = split(msg, ':');
        std::string playerName{ player };
        std::string user{ resolvePlayerName(splitMsg[0]) };
        std::string message{ trim(splitMsg[1]) };

        if (isLinkedUser(user)) {
            if (!message.empty()) {
                sendDirectMessage(message, playerName, user);
                // remember we are direct messaging with this discord user from now on
                g_privateDMChannels[playerName] = DMChannelInfo(user, 0);
            }
            else {
                // no empty message allowed
                retMessages.push_back("The message text must not be empty.");
            }
        }
        else {
            // the string before the : was not recognized as a linked user
            retMessages.push_back("This Discord user is not allowed to receive private messages."
                " Please contact a mod for permission.");
        }
    }
    else {
        // invalid parameter format, print help for this command
        return executeCommand(std::string(player), Command{ "help pm" }, scope, userId);
    }

    return retMessages;
}


/// <summary>
/// Handle the player command 'pm'. Sends a private message from Discord to a Subspace user.
/// </summary>
/// <param name="user">User name of the issuer.</param>
/// <param name="cmd">Command.</param>
/// <param name="scope">Command scope.</param>
/// <param name="userId">Discord user Id in case of commands sent from Discord.</param>
/// <returns>List of reply messages to the issuer of the command.</returns>
MessageList handleCommandPmDiscord2Subspace(std::string_view user, const Command& cmd,
    CommandScope scope, uint64_t userId)
{
    MessageList retMessages;

    if (isLinkedUser(user)) {
        std::string msg{ cmd.getFinal() };

        if (msg.find(':') != std::string::npos) {
            std::string player{ resolvePlayerName(msg.substr(0, msg.find(':'))) };
            std::string message{ trim(msg.substr(msg.find(':') + 1)) };

            sendPrivate(player, std::format("{}> {}", user, message));
    
            // remember we are direct messaging with this discord user from now on, the id
            // of the DM channel will be stored by messagecreatecallback when the player
            // replies with a private message to the bot
            g_privateDMChannels[player] = DMChannelInfo(user, 0);

            retMessages.push_back(std::format("Private message sent to {}. After a reply, use "
                "the DM channel to Discobot for subsequent messages.", player));
        }
        else {
            // invalid parameter format, print help for this command
            return executeCommand(std::string(user), Command{ "help pm" }, scope, userId);
        }
    }
    else {
        retMessages.push_back("You are not allowed to send private messages to a Subspace "
            "player. Please contact a mod for permission.");
    }

    return retMessages;
}


/// <summary>
/// Handle the player command 'spam'. Sends a role mention to the Discord channel. The role name 
/// is an optional parameter.
/// </summary>
/// <param name="player">Player name of the issuer.</param>
/// <param name="cmd">Command.</param>
/// <param name="scope">Command scope.</param>
/// <param name="userId">Discord user Id in case of commands sent from Discord.</param>
/// <returns>List of reply messages to the issuer of the command.</returns>
MessageList handleCommandSpam(std::string_view player, const Command& cmd, CommandScope scope,
    uint64_t userId)
{
    MessageList retMessages;

    if (!g_isNexusEnabled) {
        if (g_arena.find("sbt") != -1) {
            sendPrivate(g_dzBotName, std::format("!sbtline"));
            g_spamPlayerName = player;
            g_isSpamInfoIssued = true;
        }
        else {
            sendPrivate(g_dzBotName, std::format("!spam"));
        }
    }

    return retMessages;
}


/// <summary>
/// Handle the player command 'stats'. Shows the full practice or match stats.
/// </summary>
/// <param name="player">Player name of the issuer.</param>
/// <param name="cmd">Command.</param>
/// <param name="scope">Command scope.</param>
/// <param name="userId">Discord user Id in case of commands sent from Discord.</param>
/// <returns>List of reply messages to the issuer of the command.</returns>
MessageList handleCommandStats(std::string_view player, const Command& cmd, CommandScope scope,
    uint64_t userId)
{
    MessageList retMessages;

    if (!g_isNexusEnabled) {
        sendPrivate(g_dzBotName, "!stats");
    }

    return retMessages;
}


/// <summary>
/// Handle the player command 'del'. Deletes a discord message that has been sent by the bot.
/// </summary>
/// <param name="user">User name of the issuer.</param>
/// <param name="cmd">Command.</param>
/// <param name="scope">Command scope.</param>
/// <param name="userId">Discord user Id in case of commands sent from Discord.</param>
/// <returns>List of reply messages to the issuer of the command.</returns>
MessageList handleCommandDeleteDiscordMessage(std::string_view user, const Command& cmd,
    CommandScope scope, uint64_t userId)
{
    MessageList retMessages;

    try {
        std::string msgId{ cmd.getFinal() };

        g_discordCluster->message_delete(std::stoull(msgId), g_chatChannelId);
        sendPrivate(user, std::format("Discord message with Id {} deleted.", msgId));
    }
    catch (...) {
    }

    return retMessages;
}


//////// Event handlers ////////

/// <summary>
/// Handle the Tick event. Flush the arena message buffer.
/// </summary>
/// <param name="timeStamp">Current system time.</param>
export void handleEventTick(TimeStamp& timeStamp)
{
    if (timeStamp - g_messageSentTimestamp >= g_messageSendInterval) {
        // flush all buffered arena messages at least once a second
        tryFlushArenaMessages();
        g_messageSentTimestamp = timeStamp;
    }
}


/// <summary>
/// Handle the Subspace chat event. Propagate the public, team or arena message to Discord.
/// </summary>
export void handleEventChat(ChatMode chatMode, std::string_view player, std::string_view msg)
{
    // We do not want the bot's messages to be transfered to Discord, as this would cause a 
    // reposting of user messages entered in a Discord channel. Also exclude public messages 
    // from the Chaos-Bot.
    if (!player.starts_with(c_discordBotName) && !player.starts_with(g_chaosBotName)
        // for Nexus there is not Bot we message to, but always the server itself
        && !msg.starts_with("lsqueue")
        && g_parseLsqCount == 0
        && !player.starts_with(std::format(":{}:", g_dzBotName))
        && !msg.starts_with("Freq ")) {
        sendMessage(msg, chatMode, player);
    }
}


//////// Setup & Shutdown ////////
dpp::commandhandler* m_commandHandler;

/// <summary>
/// Setup the discord client for message transfer.
/// </summary>
export void setupDiscord()
{
    if (!g_isDiscordEnabled) {
        return;
    }

    // read configuration parameters and message blacklist
    readConfigParams(c_configFileName);
    readMessageBlacklist();

    // setup  command handling for discord commands
    // level 0 (player):
    registerCommandHandler("pm", &handleCommandPmSubspace2Discord, {
        OperatorLevel::Player, CommandScope::Local,
        "send a private message to a Discord user",
        { { CommandParamType::String, "name:message", "<name>:<message>",
        "name and message", true } } });
    registerCommandHandler("pm", &handleCommandPmDiscord2Subspace, {
        OperatorLevel::Player, CommandScope::External,
        "send a private message to a Subspace player",
        { { CommandParamType::String, "name", "<name>", "player name", true },
            { CommandParamType::String, "message", "<message>", "message", true } },
        {}, "", true });
    registerCommandHandler("spam", &handleCommandSpam, {
        OperatorLevel::Player, CommandScope::External,
        "post a practice spam message to the Discord channel 4v4prac" });
    registerCommandHandler("stats", &handleCommandStats, {
        OperatorLevel::Player, CommandScope::External,
        "Shot the full practice or match stats." });
    registerCommandHandler("del", &handleCommandDeleteDiscordMessage, {
        OperatorLevel::SysOp, CommandScope::External,
        "delete a discord message of the bot",
        { { CommandParamType::String, "message Id", "<message Id>",
        "message Id", true } } });

    // create a discord client with the gateway intent to scan message content, make sure to 
    // grant all privileged gateway intents in the discord development portal!
    g_discordCluster = new dpp::cluster(c_botToken, 
        dpp::i_default_intents | dpp::i_message_content);

    g_discordCluster->on_slashcommand(&handleDiscordCommand);

    g_discordCluster->on_ready([](const dpp::ready_t& evt) {
        if (c_specialFunctions.contains("initsc") 
            && dpp::run_once<struct register_bot_commands>()) {
            // register all bot commands as discord slash-commands
            std::vector<dpp::slashcommand> slashCommands;

            sendPrivate(g_chaosBotName, "Initializing Discord slash-commands...");

            for (const std::string& command : getCommandHandlers() | std::views::keys) {
                for (auto& [cmdHandler, cmdInfo] : getCommandInfos(command)) {
                    // only consider commands that can be used both in continuum and in an 
                    // external chat client
                    if (cmdInfo.maxScope == CommandScope::External) {
                        dpp::slashcommand slashCmd(command, cmdInfo.shortHelp,
                            g_discordCluster->me.id);

                        // define the command parameters
                        for (const ParamInfo& parInfo : cmdInfo.paramInfos) {
                            dpp::command_option_type t = (dpp::command_option_type)parInfo.type;

                            slashCmd.add_option(dpp::command_option(dpp::co_string, parInfo.name,
                                parInfo.help, parInfo.isRequired));
                        }

                        slashCommands.push_back(slashCmd);
                        logDiscordBot("Discord slash command '" + command + "' added.");
                        break;
                    }
                }
            }

            g_discordCluster->global_bulk_command_create(slashCommands);
        }

        if (g_chatChPlayersMessageId) {
            // get the message of the players table in the discobot channel
            g_discordCluster->message_get(g_chatChPlayersMessageId, g_chatChannelId,
                [](const dpp::confirmation_callback_t& evt) {
                    g_chatChPlayersMessage = std::get<dpp::message>(evt.value);
                });
        }

        // get the message of the players table in the player-table channel
        if (g_tableChPlayersMessageId) {
            g_discordCluster->message_get(g_tableChPlayersMessageId, c_tableChannelId,
                [](const dpp::confirmation_callback_t& evt) {
                    g_tableChPlayersMessage = std::get<dpp::message>(evt.value);
                });
        }

        // get the avaiable roles for replacing the related ids in messages
        g_discordCluster->roles_get(c_serverId, &rolesGetCallback);

        if (!g_isContinuumEnabled) {
            // register a handler for created messages
            g_discordCluster->on_message_create(&messageCreateCallback);
        }
    });
//    g_discordCluster->message_delete(1347925754188533791, g_chatChannelId);

    // start the Discord client
    if (g_isContinuumEnabled || g_arena.find("4v4") != -1)
        sendPrivate(g_dzBotName, "Starting Discord Client...");
    else if (g_isNexusEnabled)
        sendPrivate(g_nexusBotName, "Starting Discord Client...");
    else
        sendPrivate(g_chaosBotName, "Starting Discord Client...");
    g_discordCluster->start();
    logDiscordBot("Transmission started.");

    // set the chat name length
    sendPublic(std::format("?namelen={}", c_chatNameLength), 250);

    // initialize timestamps
    g_messageSentTimestamp = std::chrono::system_clock::now();
    g_announceSentTimeStamp = std::chrono::system_clock::now();
    g_lastItemsRequestTimeStamp = std::chrono::system_clock::now();
}


/// <summary>
/// Free all Discord ressources.
/// <param name="isConnectionLost">True, if the shutdown occurs due to a connection loss.</param>
/// </summary>
export void shutdownDiscord(bool isConnectionLost = false)
{
    if (!g_isDiscordEnabled) {
        return;
    }

    // reset the players table message
    g_team100Stats.clear();
    g_team200Stats.clear();
    g_playerInfos.clear();
    updatePlayersMessage();

    // send remaining buffered arena messages to discord
    if (g_isActive) {
        tryFlushArenaMessages();
        logDiscordBot("Transmission stopped.");
    }

    if (!isConnectionLost) {
        if (g_discordCluster) {
            delete g_discordCluster;
        }
        if (g_commandHandler) {
            delete g_commandHandler;
        }
    }
}

