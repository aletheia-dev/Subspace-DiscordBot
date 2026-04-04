module;
#define WINDOWS_LEAN_AND_MEAN

#include <windows.h>

export module Subspace;

import <chrono>;
import <filesystem>;
import <format>;
import <fstream>;
import <functional>;
import <iostream>;
import <list>;
import <map>;
import <queue>;
import <ranges>;
import <set>;
import <string>;

import Algorithms;
import Commands;
import Config;


// AHK callback type for executing any command with a single parameter
export typedef int(__stdcall* AHKf1Callback)(const char* cmd, const char* param);
// AHK callback type for sending a keystroke to OBS
export typedef int(__stdcall* AHKf2Callback)(const char* param);


// Dynamic objects
//
// Callback function to execute an AHK command
export AHKf1Callback execCmd{};
// Callback function to send a keystroke to OBS
export AHKf2Callback sendObs{};
// Generic name of the 4v4 bot
export std::string g_dzBotName{ "4v4" };
// Chaos bot name
export std::string g_chaosBotName{ "Chaos-Bot" };
// Nexus duel bot name, used only to supposedly have a valid bot name for startup messages
export std::string g_nexusBotName{ "Nex" };
// Flag to indicate if a reconnection is in progress
export bool g_isReconnecting{};
// Last log file stream position
std::streampos g_lastLogStreamPos{};
// Queue with log messages to be processed
std::queue<std::string> g_logMessages;
// Flag to indicate if log observation has been started
bool g_isObserving{};

// Flag to indicate if a ?find command was issued by this bot to determine the bot's arena
export bool g_isFindIssued{};
// Timestamp of the last chat message or arena entering, used for switching arenas
export TimeStamp g_lastActivityTimeStamp;
// Flag to indicate if player names are currently being fetched after having entered an arena
export bool g_isFetchingPlayerNames{};
// Last ticked name in the process of fetching player or squad names in current arena
export std::string g_lastTickName;
// Index of the current arena in c_switchArenas
export int32_t g_arenaIndex{ -1 };
// Number of players in an arena at the initial count after entering
export size_t g_initialPlayerCount{};

// Name of the player who's squad name is currently being fetched
export std::string g_squadFetchPlayerName;
// Timestamp of the last squad name query
export TimeStamp g_lastSquadQueryTimeStamp;
// Interval in seconds for fetching player squad names
export CounterDuration c_squadQueryInterval{ 60 };


/// <summary>
/// Sleep a number of milliseconds.
/// </summary>
/// <param name="ms">Milliseconds.</param>
export void sleep(uint32_t ms)
{
    Sleep(ms);
}


/// <summary>
/// Replace special AutoHotKey characters.
/// </summary>
/// <param name="msg">Text message.</param>
/// <returns>Message with replaced special AHK characters.</returns>
void replaceSpecialAHKChars(std::string& msg)
{
    std::set<std::string> search{ "!", "<", ">" , "^" , "+" , "#", "’" };
    
    for (const std::string& sub : search) {
        size_t pos{};

        while ((pos = msg.find(sub, pos)) != std::string::npos) {
            msg.replace(pos, sub.length(), std::format("{{{}}}", sub));
            pos += sub.length() + 2;  // add 2 more character positions for the {}
        }
    }
}


/// <summary>
/// 
/// Send a private chat message to a Subspace player.
/// </summary>
/// <param name="player">Player name.</param>
/// <param name="msg">Message to be sent.</param>
/// <param name="sleepInterval">Optional time interval in ms to sleep after sending.</param>
export void sendPrivate(std::string_view player, std::string msg, uint32_t sleepInterval = 10)
{
    replaceSpecialAHKChars(msg);

    for (std::string msgLine : split(msg, '\n')) {
        execCmd("Send", std::format(":{}:{}{{Enter}}", player, msgLine).c_str());
        sleep(sleepInterval);
    }
}


/// <summary>
/// Send a public message to the Subspace chat.
/// </summary>
/// <param name="msg">Message to be sent.</param>
/// <param name="sleepInterval">Optional time interval in ms to sleep after sending.</param>
export void sendPublic(std::string msg, uint32_t sleepInterval = 10)
{
    replaceSpecialAHKChars(msg);
    execCmd("Send", std::format("{}{{Enter}}", msg).c_str());
    sleep(sleepInterval);
}


/// <summary>
/// Send a team message to the Subspace chat.
/// </summary>
/// <param name="msg">Message to be sent.</param>
/// <param name="sleepInterval">Optional time interval in ms to sleep after sending.</param>
export void sendTeam(std::string msg, uint32_t sleepInterval = 10)
{
    replaceSpecialAHKChars(msg);
    execCmd("Send", std::format("//{}{{Enter}}", msg).c_str());
    sleep(sleepInterval);
}


/// <summary>
/// Send a team message to the Subspace chat.
/// </summary>
/// <param name="msg">Message to be sent.</param>
/// <param name="sleepInterval">Optional time interval in ms to sleep after sending.</param>
export void sendChannel(std::string msg, uint32_t sleepInterval = 10)
{
    replaceSpecialAHKChars(msg);
    execCmd("Send", std::format(";{}{{Enter}}", msg).c_str());
    sleep(sleepInterval);
}


/// <summary>
/// Send a keystroke to Subspace.
/// </summary>
/// <param name="msg">AHK key code to be sent.</param>
export void sendKey(const std::string& msg, uint32_t sleepInterval = 50)
{
    Sleep(sleepInterval);  // before Send calls we need to wait a bit, or it could be skipped!
    execCmd("Send", msg.c_str());
}


/// <summary>
/// Open a Subspace log file and start observation of the log.
/// </summary>
export void startObserving()
{
    if (g_isObserving) {
        execCmd("MsgBox", "Log observation has already been started!");
        return;
    }

    // remove the Continuum log file
    if (std::filesystem::exists(g_logFilePath)) {
        try {
            // try to remove an existing log file
            std::filesystem::remove(g_logFilePath);
        }
        catch (...) {
            // failed to remove the log file, it's blocked by an active log in Subspace
            try {
                // try to turn off logging in subspace and remove the log file then
                sendPublic("?log");
                Sleep(250);
                std::filesystem::remove(g_logFilePath);
            }
            catch (...) {
                // the log file is still blocked after sending ?log
                throw std::exception(std::format("Unable to remove the log file '{}'!",
                    g_logFilePath.string()).c_str());
            }
        }
    }

    // remove the DiscordBot log file
    //if (std::filesystem::exists(g_botLogFilePath)) {
    //    // try to remove an existing log file
    //    std::filesystem::remove(g_botLogFilePath);
    //}

    // start logging
    sendPublic(std::format("?log {}", g_logFileName), 1000);

    if (std::filesystem::exists(g_logFilePath)) {
        // initially we start with an empty log file
        g_lastLogStreamPos = 0;
    }
    else {
        throw std::exception("Failed to activate logging! Make sure the bot is started in "
            "Administrator mode.");
    }

    g_isObserving = true;
}


/// <summary>
/// Stop observation of the log and close the Subspace log file.
/// </summary>
export void stopObserving()
{
    if (g_isObserving) {
        // turn off logging in subspace
        sendPublic("?log", 500);
        // empty the log message queue
        for (; !g_logMessages.empty(); g_logMessages.pop())
            ;
        logDiscordBot(std::format("DiscordBot v{} stopped.", c_version));
        g_isObserving = false;
    }
}


/// <summary>
/// Get the next log message.
/// </summary>
/// <returns>Next log message.</returns>
export std::string getNextLogMessage()
{
    std::string logMsg;

    if (g_logMessages.empty()) {
        // open the log file and get the stream position at the end of the file
        std::ifstream logStream{ g_logFilePath, std::ios::ate };
        std::streampos curLogStreamPos = logStream.tellg();

        // check if the log file has grown in comparision to the backup log list
        if (curLogStreamPos > g_lastLogStreamPos) {
            std::string line;

            // move to the last read position in the log file
            logStream.seekg(g_lastLogStreamPos);

            // read all new lines and push them to the log message queue
            while (std::getline(logStream, line)) {
                g_logMessages.push(line);
            }
        }

        g_lastLogStreamPos = curLogStreamPos;
        logStream.close();
    }

    // if there is another log message in the queue, return it
    if (!g_logMessages.empty()) {
        logMsg = g_logMessages.front();
        g_logMessages.pop();
    }

    if (g_isActive)
        return logMsg;
    else
        return "";
}


/// <summary>
/// Spectate two players for arena spec.
/// </summary>
export void arenaSpec()
{
    // select the last player in the list and
    for (uint32_t i = 0; i < 100; i++) {
        sendKey("{PgDn}", 1);
    }
    // try arena spec on the last 3 players in the player list, assuming that one of them is not
    // in spec
    sendKey("{F4}");
    sendKey("{PgUp}", 250);
    sendKey("{F4}", 250);
    sendKey("{PgUp}", 250);
    sendKey("{F4}", 250);
    // wait for the player list to disappear, so it does not appear in the recording
    sleep(3000);
}


/// <summary>
/// Login with the bot player and switch to the subspace chat window.
/// </summary>
export void login()
{
    if (g_isContinuumEnabled) {
        // the Subspace Continuum Discord bot does not need to log into Subspace, as it is
        // always started after DiscordBot
        return;
    }

    // simulate press <return> to reenter the game
    sendPublic("");
    sleep(7000);
    /*
    // switch to chat window
    if (!c_specialFunctions.contains("recording")) {
        sendKey("{ALT DOWN}{TAB}");
        // press tab while holding alt for a defined number of times
        for (uint32_t i = 1; i < c_altTabCount; i++) {
            sleep(2000);
            sendKey("{TAB}");
        }
        sendKey("{ALT UP}");
        sleep(5000);
    }
    */
}


/// <summary>
/// Tick all players in the arena to determine their names by issuing successive messages with
/// %tickname.
/// </summary>
export void fetchNextPlayerName()
{
    if (!g_isFetchingPlayerNames) {
        g_playerInfos.clear();
        g_isFetchingPlayerNames = true;
        //sleep(5000);
        //sendPrivate(g_dzBotName, "1");
        //sendPrivate(g_dzBotName, "12");
        //sendPrivate(g_dzBotName, "13");
        //sendPrivate(g_dzBotName, "14");
        //sendPrivate(g_dzBotName, "15");
        //sendPrivate(g_dzBotName, "16");
        //sendPrivate(g_dzBotName, "17");
        //sendPrivate(g_dzBotName, "18");
        //sendPrivate(g_dzBotName, "19");
        //sendPrivate(g_dzBotName, "11");
        //sendPrivate(g_dzBotName, "21");
        //sendPrivate(g_dzBotName, "31");
    }
    
    // send a private command to the bot with %tickname to obtain the ticked player's name
    sendPrivate(g_dzBotName, "%tickname in arena", 1000);
    // tick the next player in the list by hitting page-down key
    sendKey("{PgDn}");
}



/// <summary>
/// Fetch the squad names of all players currently present in the arena.
/// </summary>
export void fetchNextSquadName()
{
    std::string squadFetchPlayerName;
    uint32_t minUpdateCount;
    bool isFirst{ true };

    for (const std::string& curPlayerName : g_playerInfos | std::views::keys) {
        // determine the update count for the current player name
        uint32_t curUpdateCount;
        bool found{ false };

        for (auto& [playerName, squadInfo] : g_squadInfos) {
            if (playerName == curPlayerName) {
                if (isFirst) {
                    squadFetchPlayerName = curPlayerName;
                    minUpdateCount = squadInfo.second;
                }
                else {
                    curUpdateCount = squadInfo.second;
                }
                found = true;
                break;
            }
        }

        if (found && !isFirst) {
            if (curUpdateCount == 0) {
                squadFetchPlayerName = curPlayerName;
                break;
            }

            if (curUpdateCount < minUpdateCount) {
                squadFetchPlayerName = curPlayerName;
                minUpdateCount = curUpdateCount;
            }
        }
        isFirst = false;
    }

    // send a public command obtain the currently indexed player's squad name
    if (squadFetchPlayerName != "") {
        g_squadFetchPlayerName = squadFetchPlayerName;
        sendPublic(std::format("?squad {}", squadFetchPlayerName), 1000);
    }
}


/// <summary>
/// Switch the arena.
/// </summary>
/// <param name="isCyclincArena">True, if it's a switch to the next configured switch-arena, or
/// false in case of the initial arena from the persitent data file or an arena explicitely set 
/// with the !go command.</param>
export void switchArena(bool isCyclincArena = false)
{
    if (g_isContinuumEnabled) {
        // only issue a find command to obtain the current arena name
        sendPublic(std::format("?find {}", c_discordBotName));
        g_isFindIssued = true;
        return;
    }

    g_isFindIssued = false;
    g_isFetchingPlayerNames = false;

    if (isCyclincArena && c_switchArenas.size() > 0) {
        // determine the next arena index
        g_arenaIndex = (g_arenaIndex + 1) % c_switchArenas.size();

        // determine the name of the next arena
        int32_t i{};

        for (std::string_view arena : c_switchArenas) {
            if (i++ == g_arenaIndex) {
                sendPublic(std::format("?go {}", arena));
                sleep(2000);
                break;
            }
        }
    }
    else if (g_arena != "") {
        // go to the arena that has been set with the !go command
        sendPublic(std::format("?go {}", g_arena));
        sleep(2000);
    }
    else if (c_initialArena != "") {
        // go to the initial arena from the persitent data file
        sendPublic(std::format("?go {}", c_initialArena));
        sleep(2000);
    }

    g_lastActivityTimeStamp = std::chrono::system_clock::now();
}


//////// Command handler ////////

/// <summary>
/// Handle the player command 'help'.
/// </summary>
/// <param name="player">Player name of the issuer.</param>
/// <param name="cmd">Command.</param>
/// <param name="scope">Command scope.</param>
/// <param name="userId">Discord user Id in case of commands sent from Discord.</param>
/// <returns>List of reply messages to the issuer of the command.</returns>
MessageList handleCommandHelp(std::string_view player, const Command& cmd, CommandScope scope,
    uint64_t userId)
{
    MessageList retMessages;

    std::string param{ toLower(cmd.getFinal()) };
    //OperatorLevel level{ getOperatorLevel(param) };

    if (param.empty() || param == "all") {
        // !help: show all commands that are available for this player
        auto& levelDesc{ getLevelDescriptions() };

        for (auto iter = levelDesc.rbegin(); iter != levelDesc.rend(); ++iter) {
            OperatorLevel accessLevel{ iter->first };

            // consider only levels that are available for the player who issued !help
            if (checkOperatorLevel(player, accessLevel)) {
                std::string cmdDesc{ getCommandsDescription(accessLevel, scope) };

                if (!cmdDesc.empty()) {
                    retMessages.push_back(cmdDesc);
                }
            }
        }
    }
    //else if (level != OperatorLevel::Unknown) {
    //    // !help <operator level>: show all commands that are available for the specified 
    //    // operator level
    //    if (player.access >= level) {
    //        std::string cmdDesc{ getCommandsDescription(level) };

    //        if (!cmdDesc.empty()) {
    //            retMessages.push_back(std::format("{}:", m_pluginName));
    //            retMessages.push_back(cmdDesc);
    //        }
    //    }
    //}
    else {
        // !help <command>: show detailed help for a specified command
        std::string command{ getAliasList().aliasToCommand(toLower(param)) };
        std::string errMsg;

        if (isCommand(command)) {
            for (auto& [cmdHandler, cmdInfo] : getCommandInfos(command)) {
                if (checkCommandAccess(player, cmdInfo, scope, errMsg)) {
                    retMessages.push_back(getCommandHelp(command, cmdInfo, scope));
                    break;
                }
            }

            // in case command access is denied for this player, send a specific message to the 
            // player
            if (!errMsg.empty()) {
                retMessages.push_back(errMsg);
            }
        }
        else {
            // print all commands available to this player
            return handleCommandHelp(player, Command{ "help" }, scope, userId);
        }
    }

    return retMessages;
}


/// <summary>
/// Handle the player command 'version'.
/// </summary>
/// <param name="player">Player name of the issuer.</param>
/// <param name="cmd">Command.</param>
/// <param name="scope">Command scope.</param>
/// <param name="userId">Discord user Id in case of commands sent from Discord.</param>
/// <returns>List of reply messages to the issuer of the command.</returns>
MessageList handleCommandVersion(std::string_view player, const Command& cmd, CommandScope scope,
    uint64_t userId)
{
    MessageList retMessages;

    retMessages.push_back(std::format("DiscordBot v{} - by Aletheia", c_version));

    return retMessages;
}


/// <summary>
/// Handle the player command 'about'.
/// </summary>
/// <param name="player">Player name of the issuer.</param>
/// <param name="cmd">Command.</param>
/// <param name="scope">Command scope.</param>
/// <param name="userId">Discord user Id in case of commands sent from Discord.</param>
/// <returns>List of reply messages to the issuer of the command.</returns>
MessageList handleCommandAbout(std::string_view player, const Command& cmd, CommandScope scope,
    uint64_t userId)
{
    MessageList retMessages;

    retMessages.push_back("I am a bot for automated recording of 4v4 matches and provide "
        "various features for the interconnection of Subspace and Discord. Send me bug reports "
        "or suggestions with ?message");

    return retMessages;
}


/// <summary>
/// Handle the player command 'restart'.
/// </summary>
/// <param name="player">Player name of the issuer.</param>
/// <param name="cmd">Command.</param>
/// <param name="scope">Command scope.</param>
/// <param name="userId">Discord user Id in case of commands sent from Discord.</param>
/// <returns>List of reply messages to the issuer of the command.</returns>
MessageList handleCommandRestart(std::string_view player, const Command& cmd, CommandScope scope,
    uint64_t userId)
{
    MessageList retMessages;

    retMessages.push_back("The bot has been restarted. It should be back in a minute.");
    logDiscordBot("Bot manually restarted.");

    g_isRestarted = true;

    return retMessages;
}


/// <summary>
/// Handle the player command 'go'.
/// </summary>
/// <param name="player">Player name of the issuer.</param>
/// <param name="cmd">Command.</param>
/// <param name="scope">Command scope.</param>
/// <param name="userId">Discord user Id in case of commands sent from Discord.</param>
/// <returns>List of reply messages to the issuer of the command.</returns>
MessageList handleCommandGo(std::string_view player, const Command& cmd, CommandScope scope,
    uint64_t userId)
{
    MessageList retMessages;

    g_arena = cmd.getFinal();
    switchArena();

    return retMessages;
}


//////// User links ////////

/// <summary>
/// Load the Discord to Subspace user links from a dedicated text file. The file contains  
/// Discord user Ids and Subspace player names separated by a tab. The verified Discord user 
/// names are equal to the Subspace names names except casing.
/// </summary>
export void readSubspaceUserLinks()
{
    std::ifstream inFile{ c_userLinksFilename };

    if (inFile.is_open()) {
        std::string line;

        while (getline(inFile, line)) {
            std::vector<std::string> linkInfo = split(line, '\t');

            if (linkInfo.size() == 2) {
                uint64_t userId{ stoull(linkInfo[0]) };
                const std::string userName{ linkInfo[1] };

                g_userLinks[userName] = userId;
            }
        }
    }
}


/// <summary>
/// Get the full player name of a name that might be abbreviated to 10 characters.
/// </summary>
/// <param name="">Player name, must not be abbreviated under a length of 10 characters.</param>
/// <returns>Player name.</returns>
export std::string resolvePlayerName(std::string_view player)
{
    // remove brackets that might appear around a remote sender
    std::string searchName{ toLower(trim(player)) };

    // if applicable, retrieve the full name
    if (!searchName.empty()) {
        for (auto& [name, id] : g_userLinks) {
            std::string lcName{ toLower(name) };

            if (name.length() > c_chatNameLength && lcName.starts_with(searchName)
                || lcName == searchName) {
                return name;
            }
        }
    }

    return trim(player);
}


/// <summary>
/// Get a linked Discord user name for a Discord user Id.
/// </summary>
/// <param name="userId">Discord user Id.</param>
/// <returns>Discord user name for the given Discord user Id.</returns>
export std::string getLinkedUserName(uint64_t userId)
{
    for (auto& [name, id] : g_userLinks) {
        if (id == userId) {
            return name;
        }
    }

    return "";
}


/// <summary>
/// Get a linked Discord user Id for a Discord user name.
/// </summary>
/// <param name="userId">Discord user Id.</param>
/// <returns>Discord user name for the given Discord user Id, or 0 in case the name does not 
/// exist.</returns>
export uint64_t getLinkedUserId(std::string_view player)
{
    for (auto& [name, id] : g_userLinks) {
        if (toLower(name) == toLower(player)) {
            return id;
        }
    }

    return 0;
}


/// <summary>
/// Check if a player is a linked Discord user.
/// </summary>
/// <param name="player">Player.</param>
/// <returns>True, if the specified player is a linked Discord user.</returns>
export bool isLinkedUser(std::string_view player)
{
    for (auto& [name, id] : g_userLinks) {
        if (toLower(name) == toLower(player)) {
            return true;
        }
    }

    return false;
}


//////// Command processing ////////

/// <summary>
/// Check if a log message is a private message form a bot with a specified name prefix.
/// </summary>
/// <param name="logMsg">Log message.</param>
/// <param name="botName">Bot name, which might also be a prefix.</param>
/// <returns>True, if the log message is a private message form the DZ bot.</returns>
export bool isPrivateBotMessage(std::string_view logMsg, std::string_view botName)
{
    if (logMsg.starts_with("P ") && trim(logMsg.substr(2)).starts_with(botName))
        return true;
    else
        return false;
}


/// <summary>
/// Parse the player name from a public, private (local or remote) or team log message.
/// </summary>
/// <param name="logMsg">Log message.</param>
/// <returns>Player name.</returns>
export std::string parsePlayerName(std::string_view logMsg)
{
    std::string player;

    if (logMsg.find(">") != -1) {
        size_t pos{ logMsg.find(">") };

        if (pos != std::string::npos) {
            if (!logMsg.starts_with("P (") && pos == c_chatNameLength + 2) {
                // it's a public, team, or local private message
                player = resolvePlayerName(logMsg.substr(2, c_chatNameLength));
            }
            else if (logMsg.starts_with("P (") && logMsg.find(")>") == pos - 1) {
                // it's a remote private message
                player = logMsg.substr(3, pos - 4);
            }
        }
    }
    else {
        if (logMsg.find(std::format(":{}:", g_dzBotName)) == 2) {
            player = std::format(":{}:", g_dzBotName);
        }
    }

    return player;
}


/// <summary>
/// Parse the player name, command scope and message from a private message to this bot.
/// </summary>
/// <param name="logMsg">Log message in and message after name out.</param>
/// <param name="player">Player name out.</param>
/// <param name="scope">Command scope out.</param>
/// <returns>True, if the message is a private message to this bot, that has not been sent from 
/// this bot or the DZ bot.</returns>
export bool parsePrivateMessage(std::string& logMsg, std::string& player, CommandScope& scope)
{
    // check if it's a private player message to the bot
    if (!logMsg.starts_with("P ")) {
        return false;
    }

    player = parsePlayerName(logMsg);

    // check if it's not a private message of this bot or the DZ bot
    if (player.starts_with(c_discordBotName) || player.starts_with(g_dzBotName)) {
        return false;
    }

    // determine the scope, either local or remote
    if (logMsg.starts_with("P ("))
        scope = CommandScope::Remote;
    else
        scope = CommandScope::Local;

    // get the message text after the player name and >
    logMsg = trim(logMsg.substr(logMsg.find(">") + 1));

    return true;
}


/// <summary>
/// Process commands typed in the subspace chat.
/// </summary>
/// <param name="logMsg">Log message.</param>
/// <returns>True, if a command has been processed, otherwise false.</returns>
export bool processCommands(std::string_view logMsg)
{
    if (g_isContinuumEnabled) {
        return false;
    }
        
    std::string msg{ logMsg };
    std::string player;
    CommandScope scope;

    // check if it's a private player message to the bot
    if (!parsePrivateMessage(msg, player, scope)) {
        return false;
    }

    // a private message could either be a command or a review comment
    if (msg[0] == '!' || msg[0] == '.') {
        // user command received
        std::string cmdLine{ msg.substr(1) };
        std::string command{ cmdLine.substr(0, cmdLine.find(" ")) };

        Command cmd(cmdLine, isFinalOnlyCommand(command));
        MessageList replyMessages = executeCommand(player, cmd, scope);

        for (std::string replyMsg : replyMessages) {
            sendPrivate(player, replyMsg, 200);
        }
        return true;
    }

    return false;
}


//////// Setup ////////

/// <summary>
/// Setup Subspace commands.
/// </summary>
export void setupSubspace()
{
    g_lastActivityTimeStamp = std::chrono::system_clock::now();
    g_lastSquadQueryTimeStamp = std::chrono::system_clock::now();

    // setup  command handling for Subspace commands
    // level 0 (player):
    registerCommandHandler("help", &handleCommandHelp, {
        OperatorLevel::Player, CommandScope::External,
        "provide help for a command or show all available commands",
        { { CommandParamType::String, "command", "[<command>]", "command name" } }, {},
        "conventions:\n"
        "<> argument  | alternative  [] optional  () description  -s[=<value>] switch" });
    registerCommandHandler("version", &handleCommandVersion, {
        OperatorLevel::Player, CommandScope::External,
        "display bot version" });
    registerCommandHandler("about", &handleCommandAbout, {
        OperatorLevel::Player, CommandScope::External,
        "query me about my function" });
    registerCommandHandler("restart", &handleCommandRestart, {
        OperatorLevel::Moderator, CommandScope::External,
        "restart the bot" });
    registerCommandHandler("go", &handleCommandGo, {
        OperatorLevel::Moderator, CommandScope::External,
        "move the bot to a specified arena",
        { { CommandParamType::String, "arena", "[<arena>]", "arena name" } } });

    // print the bot version
    std::string versionMsg{ std::format("DiscordBot v{}", c_version) };

    if (g_isNexusEnabled)
        sendPrivate(g_nexusBotName, versionMsg);
    else
        sendPrivate(g_chaosBotName, versionMsg);
    logDiscordBot(versionMsg + " started.");
}

