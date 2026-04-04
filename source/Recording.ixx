export module Recording;

import <array>;
import <chrono>;
import <cstdint>;
import <format>;
import <fstream>;
import <list>;
import <map>;
import <string>;

import Algorithms;
import Commands;
import Config;
import Subspace;


/// <summary>
/// Container for information about a review item.
/// </summary>
export struct ReviewInfo
{
    ReviewInfo() = default;
    ReviewInfo(std::string_view p, std::string_view c) : player{ p }, comment{ c } {};

    std::string player;  // player name
    std::string comment;  // review comment
};


// Recording config objects
//
// Duration in seconds to pause showing stats the next time
export CounterDuration c_statsPauseInterval;
// Duration in seconds to wait for showing stats after a kill
export CounterDuration c_statsWaitInterval;
// Duration of seconds to keep showing the stats with full-screen chat after a kill
export CounterDuration c_statsShowInterval;
// Duration in seconds to keep showing the stats with full-screen chat after match end
export CounterDuration c_endStatsShowInterval;
// Duration in seconds for showing items
export CounterDuration c_itemsWaitInterval;
// Negative offset for timeStamps of review marks
export CounterDuration c_timeStampOffset;
// Maximum number of review items per player
export uint32_t c_maxReviewItems;

// State variables
//
// True, if a recoding is being done, otherwise false
export bool g_isRecording{};
// True, if a match is being played, otherwise false
export bool g_isMatchGoing{};
// Start time of the recording
export TimeStamp g_startTime;
// Review infos of players who set review marks
export std::map<std::string, ReviewInfo> g_reviewInfos;
// Score filename prefix, for a pro league match it's "<team 1> vs <team 2>"
export std::string g_gameTitle{ "practice match" };
// Current number of chat lines
export uint32_t g_chatLinesCount{ 8 };

// Timestamp for pausing to show stats next time
export TimeStamp g_statsPauseTimeStamp;
// Timestamp for waiting to show stats after a kill
export TimeStamp g_statsWaitTimeStamp;
// State flag for waiting to show stats after a kill
export bool g_isWaitingForShowStats{};
// Flag to indicate whether in-game stats are shown
export bool g_isShowingStats{};
// Flag to indicate whether final stats are shown
export bool g_isShowingFinalStats{};
// Timestamp for keep showing the stats with full-screen chat after a kill
export TimeStamp g_statsShowTimeStamp;
// Flag to indicate whether the stats table is being fetched
export bool g_isFetchingStats{};
// Counter for fetched stats tables, [0..2]
export uint32_t g_fetchedTablesCount{};
// Flag to indicate whether the stats table of the first team has been fetched
export bool g_firstTableFetched{};
// Stats table buffers
export std::list<std::string> g_stats100Buffer;
export std::list<std::string> g_stats200Buffer;
// Stats ranking buffer
export std::string g_statsRanking;
// Timestamp for showing items next time
export TimeStamp g_itemsWaitTimeStamp;
// Buffer for preserving the last couple of log messages, index 0 holds the previous log message
export std::array<std::string, 2> g_logBuffer;


//////// Recording ////////

/// <summary>
/// Store a review message.
/// </summary>
/// <param name="player">Full player name.</param>
/// <param name="msg">Text message.</param>
/// <returns>List of reply messages to the issuer of the command.</returns>
export MessageList storeReviewMessage(std::string_view player, std::string msg)
{
    MessageList retMessages;
    uint32_t cnt{};
    
    // count the already stored review infos of this player
    if (!player.empty()) {
        for (const auto& [timeStamp, reviewInfo] : g_reviewInfos) {
            if (reviewInfo.player == player) {
                cnt++;
            }
        }
    }

    if (cnt < c_maxReviewItems) {
        // store a new review item
        auto now{ std::chrono::system_clock::now() };
        auto elapsedSeconds = floor<std::chrono::seconds>((now - g_startTime) + c_timeStampOffset);
        std::string timeStamp = std::format("{:%H:%M:%S}", elapsedSeconds);

        g_reviewInfos[timeStamp] = ReviewInfo{ player, msg };

        if (!player.empty()) {
            if (!msg.empty()) {
                retMessages.push_back("Review mark saved.");
            }
            else {
                retMessages.push_back("Review mark saved. Send me a private message to add a "
                    "comment.");
            }
        }
    }
    else if (!player.empty()) {
        retMessages.push_back(std::format(
            "You are not allowed to have more than {} review marks.", c_maxReviewItems));
    }
    else {
        sendPrivate(g_dzBotName, "ERRRRRRRRRRRRRRRRRRRR");
    }

    return retMessages;
}


/// <summary>
/// Save the review infos to a file.
/// </summary>
export void saveReviewInfos()
{
    sendPrivate(g_dzBotName, std::format("saving review"));
    if (g_reviewInfos.size() > 0) {
        std::string reviewsText{ "Subspace 4v4 Draft League\n\n" };

        for (const auto& [timeStamp, reviewInfo] : g_reviewInfos) {
            if (!reviewInfo.player.empty()) {
                if (!reviewInfo.comment.empty()) {
                    reviewsText = std::format("{}{} {}  ({})\n", reviewsText, timeStamp,
                        reviewInfo.comment, reviewInfo.player);
                }
                else {
                    reviewsText = std::format("{}{} ({})\n", reviewsText, timeStamp,
                        reviewInfo.player);
                }
            }
            else {
                reviewsText = std::format("{}{} {}\n", reviewsText, timeStamp,
                    reviewInfo.comment);
            }
        }

        TimeStamp now = std::chrono::system_clock::now();
        std::string fileName{ std::format("{} {:%Y-%m-%d %H-%M-%S}.txt", g_gameTitle,
            floor<std::chrono::seconds>(now)) };
        std::string filePath{ std::format("{}/videos/{}", c_continuumDirPath, fileName) };
        std::ofstream file(filePath);

        file << reviewsText;
        file.close();
        g_reviewInfos.clear();
    }
}


/// <summary>
/// Start recording with OBS.
/// </summary>
export void startRecording()
{
    // deactivate public chat and set the number of chat lines
    sendPublic("?nopubchat", 200);
    sendPublic(std::format("?lines={}", g_chatLinesCount), 200);

    // try to spec two players with f4
    arenaSpec();
    // start recording
    sendObs("{F9}");
    sleep(200);
    sendPrivate(g_dzBotName, "RECORDING STARTED", 200);
    // turn on radar
    sendKey("{End}");

    // initialize those timestamps that need to have the current system time from the start
    g_startTime = std::chrono::system_clock::now();
    g_statsPauseTimeStamp = g_startTime;
    g_itemsWaitTimeStamp = g_startTime;

    g_isRecording = true;
    g_isMatchGoing = true;
}


/// <summary>
/// Manually start recording a match. Used when the bot is late to a match and recording needs to 
/// be started manually by an administrator.
/// </summary>
export void forceRecording()
{
    if (g_isRecordingEnabled && !g_isRecording) {
        // start recording with OBS
        startRecording();
    }
}


/// <summary>
/// Stop recording with OBS.
/// </summary>
export void stopRecording()
{
    // proceed, if we are actually recording
    if (g_isRecording) {
        sendObs("{F10}");
        saveReviewInfos();
        g_isRecording = false;
        g_isMatchGoing = false;
        g_gameTitle = "";
        sendPrivate(g_dzBotName, "RECORDING STOPPED", 200);
        g_chatLinesCount = 8;
        sendPublic(std::format("?lines={}", g_chatLinesCount), 200);
    }
}


//////// Command handler ////////

/// <summary>
/// Handle the player command 'review'.
/// </summary>
/// <param name="player">Full player name of the issuer.</param>
/// <param name="cmd">Command.</param>
/// <param name="scope">Command scope.</param>
/// <param name="userId">Discord user Id in case of commands sent from Discord.</param>
/// <returns>List of reply messages to the issuer of the command.</returns>
MessageList handleCommandReview(std::string_view player, const Command& cmd,
    CommandScope scope, uint64_t userId)
{
    MessageList retMessages;

    if (g_isMatchGoing) {
        retMessages = storeReviewMessage(player, cmd.getFinal());
    }
    else {
        retMessages.push_back("This command is available in a matches only.");
    }

    return retMessages;
}


//////// Message processing ////////

/// <summary>
/// Process a private message to the bot. It is assumed that this message is a review comment.
/// </summary>
/// <param name="logMsg">Log message.</param>
/// <returns>True, if a reply has been sent, otherwise false.</returns>
export bool processPrivateRecordingMessage(std::string_view logMsg)
{
    std::string msg{ logMsg };
    std::string player;
    CommandScope scope;
    bool retVal{};

    // check if it's a private player message to the bot
    if (!parsePrivateMessage(msg, player, scope)) {
        return false;
    }

    // proceed if the message is not a command
    if (msg[0] != '!' && msg[0] != '.') {
        MessageList replyMessages;
        ReviewInfo* lastReviewInfo{};

        // find the last review info of this player
        for (auto& [timeStamp, reviewInfo] : g_reviewInfos) {
            if (reviewInfo.player == player) {
                lastReviewInfo = &reviewInfo;
            }
        }

        if (lastReviewInfo) {
            if (lastReviewInfo->comment.empty()) {
                // existing review mark without comment found, store the comment
                lastReviewInfo->comment = msg;
                sendPrivate(player, "Review comment saved.");
                retVal = true;
            }
            else {
                // the last review mark has an empty comment, store a new review mark
                replyMessages = storeReviewMessage(player, msg);
                retVal = true;
            }
        }
        //else {
        //    // no review mark found for this player, store a new review mark
        //    replyMessages = storeReviewMessage(player, msg);
        //    retVal = true;
        //}

        for (std::string replyMsg : replyMessages) {
            sendPrivate(player, replyMsg, 200);
        }
    }

    return retVal;
}


//////// Setup ////////

/// <summary>
/// Setup recording.
/// </summary>
export void setupRecording()
{
    if (!g_isRecordingEnabled) {
        return;
    }

    // obtain the folder path of the bot dll and read the config parameters
    std::string filePath{ (std::filesystem::current_path() / c_configFileName).string() };

    readConfigParam("Recording", "StatsPauseInterval", c_statsPauseInterval, filePath);
    readConfigParam("Recording", "StatsWaitInterval", c_statsWaitInterval, filePath);
    readConfigParam("Recording", "StatsShowInterval", c_statsShowInterval, filePath);
    readConfigParam("Recording", "EndStatsShowInterval", c_endStatsShowInterval, filePath);
    readConfigParam("Recording", "ItemsWaitInterval", c_itemsWaitInterval, filePath);
    readConfigParam("Recording", "TimeStampOffset", c_timeStampOffset, filePath);
    readConfigParam("Recording", "MaxReviewItems", c_maxReviewItems, filePath);

    // setup command handling for discord commands
    // level 0 (player):
    registerCommandHandler("review", &handleCommandReview, {
        OperatorLevel::Player, CommandScope::Local,
        "save a review mark with an optional comment",
        { { CommandParamType::String, "comment", "[<comment>]", "review comment" } }, {},
        "For a fast application, define a macro :RecBot:!review\n"
        "The comment can be set/changed afterwards by sending me a private message." });
}

