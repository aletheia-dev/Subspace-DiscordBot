export module Config;

import <any>;
import <filesystem>;
import <fstream>;
import <format>;
import <functional>;
import <iostream>;
import <map>;
import <set>;
import <string>;
import <vector>;

import Algorithms;


// Timestamp type
export typedef std::chrono::time_point<std::chrono::system_clock> TimeStamp;
// Duration type for counters
export typedef std::chrono::duration<int64_t> CounterDuration;
// Operator info
export typedef std::pair<uint32_t, std::string> OperatorInfo;


// Constants
//
export const std::string c_version{ "1.7.7" };  // app version
export const std::string c_configFileName{ "DiscordBot.INI" };  // bot configuration file name
export const std::string c_perstDataFileName{ "PersistentData.INI" };  // persistent data filename
export const size_t c_chatNameLength{ 10 };  // chat name length
export const std::string c_userLinksFilename = "DiscordUserLinks.txt";
// Duration in seconds until the bot switches from an inactive arena
export const CounterDuration c_switchTimeoutInterval{ 60 * 10 };

// Operator map, key: name, value: pair of level and password
export std::map<std::string, OperatorInfo> c_operators;
// mapping of Discord user names to Ids
export std::map<std::string, uint64_t> g_userLinks;

// General config objects
//
export std::string c_discordBotName;  // generic bot name without a number suffix
export std::string c_continuumDirPath;    // Continuum installation path
export std::set<std::string> c_specialFunctions;  // initsc, recording, nodiscord
export std::set<std::string> c_discordServerType;  // svsproleague, nexus, continuum
export std::list<std::string> c_switchArenas;  // arenas to check cyclicly for user activity
export uint32_t c_altTabCount;  // number of tabs to press when alt-tabing Subspace

// Persistent data
//
export std::string c_initialArena;  // name of the initial arena

// Dynamic objects and states
//
export std::string g_logFileName; // Continuum log file name
export std::filesystem::path g_logFilePath;   // Continuum log file pathname
export std::filesystem::path g_botLogFilePath;   // DiscordBot log file pathname
export bool g_isStarted{};   // true, if log observation has been started
export bool g_isActive{};  // true, if the bot is active (not shut down due to connection loss)
export bool g_isRestarted{};   // true, if the bot has been restarted with the !restart command
export TimeStamp g_updateOpsTimeStamp;  // timestamp of last operators update
export std::string g_arena;  // name of the current arena
export uint64_t g_chatChPlayersMessageId{};  // player message Id, discobot channel
export uint64_t g_tableChPlayersMessageId{};  // player message Id, player-table channel
// Mapping of player names to squad names
export std::map<std::string, std::string> g_playerInfos;
// Mapping of player names to pairs of squad name and update counter
export std::map<std::string, std::pair<std::string, uint32_t>> g_squadInfos;
// Counter for squad name updates, used to determine with player's sqaud needs to be updated next
export uint32_t g_squadUpdateCount{};
// True, if access to Discord is not disabled with "nodiscord"
export bool g_isDiscordEnabled{};
// True, if automated recording is enabaled with the "recording" config parameter value
export bool g_isRecordingEnabled{};
// True, if results reporting is not disabled with the "noreporting" config parameter value
export bool g_isReportingEnabled{};
// True, if message transfer to and from Discord is disabled with discord type "continuum"
export bool g_isContinuumEnabled{};
// True, if discord type is "nexus"
export bool g_isNexusEnabled{};


//////// File Access ////////

/// <summary>
/// Read a configuration parameter value from an INI file.
/// </summary>
/// <param name="section">Section name.</param>
/// <param name="key">Parameter name.</param>
/// <param name="filePath">Absolute path of the configuration file.</param>
/// <param name="defValue">Default parameter value.</param>
/// <returns>Parameter value as string.</returns>
export std::string getPrivateProfileString(std::string_view section, std::string_view key,
    std::string_view filePath, std::string defValue = "")
{
    std::ifstream file(filePath.data());

    if (!file.is_open()) {
        throw std::exception(std::format("Config file '{}' not found!", filePath).c_str());
    }

    try {
        bool sectionFound{};
        std::string line;
        uint32_t count{ 1 };

        if (section == "") {
            sectionFound = true;  // parameter outside of a section
        }

        while (getline(file, line)) {
            line = trim(line);

            if (line.starts_with("[")) {
                // [<section>]
                if (line.ends_with("]")) {
                    if (line.substr(1, line.length() - 2) == section) {
                        sectionFound = true;
                    }
                }
                else {
                    throw std::exception(std::format("Invalid configuration file '{}'. "
                        "Missing ']' in line {}!", filePath, count).c_str());
                }
            }
            else if (sectionFound && !line.starts_with("#") && !line.starts_with("//")
                && !line.starts_with(";")) {
                if (line.find("=") != std::string::npos) {
                    // <key>=<value>
                    std::string curKey = splitFirst(line, '=');

                    if (toLower(curKey) == toLower(key)) {
                        if (!line.empty())
                            return line;
                        else
                            return defValue;
                    }
                }
                else if (line.empty()) {
                    throw std::exception(std::format("Invalid configuration file '{}'. Missing "
                        "configuration parameter '{}' in section '{}'!", filePath, key, 
                        section).c_str());
                }
                else {
                    throw std::exception(std::format("Invalid configuration file '{}'. "
                        "Missing '=' in line {}!", filePath, count).c_str());
                }
            }
            ++count;
        }
        return defValue;
    }
    catch (...) {
        file.close();
        throw;
    }
}


/// <summary>
/// Write a configuration parameter value to an INI file.
/// </summary>
/// <param name="section">Section name.</param>
/// <param name="key">Parameter name.</param>
/// <param name="value">Parameter value.</param>
/// <param name="filePath">Absolute path of the configuration file.</param>
export void setPrivateProfileString(std::string_view section, std::string_view key,
    std::string value, std::string_view filePath)
{
    // read the config file and modify the specified parameter
    std::ifstream inFile(filePath.data());
    std::string dest;

    try {
        bool sectionFound{};
        std::string curLine, line;
        uint32_t count{ 1 };

        while (getline(inFile, curLine)) {
            line = trim(curLine);

            if (line.starts_with("[")) {
                // [<section>]
                if (line.ends_with("]")) {
                    if (line.substr(1, line.length() - 2) == section) {
                        sectionFound = true;
                    }
                }
                else {
                    throw std::exception(std::format("Invalid configuration file '{}'. "
                        "Missing ']' in line {}!", filePath, count).c_str());
                }
            }
            else if (!line.starts_with("#") && !line.starts_with("//")
                && !line.starts_with(";")) {
                // <key>=<value>
                if (line.find("=") != std::string::npos) {
                    if (sectionFound) {
                        std::string curKey = splitFirst(line, '=');

                        if (toLower(curKey) == toLower(key)) {
                            curLine = std::format("{}={}", curKey, value);
                        }
                    }
                }
                else if (line.empty()) {
                    throw std::exception(std::format("Invalid configuration file '{}'. Missing "
                        "configuration parameter '{}' in section '{}'!", filePath, key,
                        section).c_str());
                }
                else {
                    throw std::exception(std::format("Invalid configuration file '{}'. "
                        "Missing '=' in line {}!", filePath, count).c_str());
                }
            }
            dest += curLine + '\n';
            ++count;
        }
        inFile.close();
    }
    catch (...) {
        inFile.close();
        throw;
    }

    // write the modified configuration to the specified file
    std::ofstream outFile(filePath.data());

    try {
        outFile.write(dest.c_str(), dest.length());
        outFile.close();
    }
    catch (...) {
        outFile.close();
        throw;
    }
}


/// <summary>
/// Read a configuration parameter value from an INI file. The parameter is converted to the 
/// type of the destination variable. If no default value is given, the parameter is obligatory.
/// </summary>
/// <param name="section">Section name.</param>
/// <param name="key">Parameter name.</param>
/// <param name="retValue">Output value.</param>
/// <param name="filePath">Absolute path of the configuration file.</param>
/// <param name="defValue">Default parameter value. If undefined, the parameter has to be 
/// specified in the config file, otherwise an exception is thrown.</param>
export template<typename RetT>
void readConfigParam(std::string_view section, std::string_view key, RetT& retValue,
    std::string_view filePath, std::string defValue = "_noopt_")
{
    std::string configValue;

    if ((configValue = getPrivateProfileString(section, key, filePath, defValue)) != "_noopt_") {
        try {
            if (typeid(retValue) == typeid(std::string)) {
                retValue = (decltype(retValue))configValue;
            }
            else if (typeid(retValue) == typeid(bool)) {
                auto val{ stoll(configValue) };

                retValue = (decltype(retValue))val;
            }
            else if (typeid(retValue) == typeid(int8_t)
                || typeid(retValue) == typeid(int16_t)
                || typeid(retValue) == typeid(int32_t)
                || typeid(retValue) == typeid(int64_t)) {
                auto val{ stoll(configValue) };

                retValue = (decltype(retValue))val;
            }
            else if (typeid(retValue) == typeid(uint8_t)
                || typeid(retValue) == typeid(uint16_t)
                || typeid(retValue) == typeid(uint32_t)
                || typeid(retValue) == typeid(uint64_t)) {
                auto val{ stoull(configValue) };

                retValue = (decltype(retValue))val;
            }
            else if (typeid(retValue) == typeid(std::list<std::string>)) {
                // <string>[,<string>...]
                std::list<std::string> strList;

                if (trim(configValue).size()) {
                    for (std::string& str : split(configValue, ',')) {
                        strList.push_back(trim(str));
                    }
                }
                retValue = (decltype(retValue))strList;
            }
            else if (typeid(retValue) == typeid(std::set<std::string>)) {
                // <string>[,<string>...]
                std::set<std::string> strSet;

                if (trim(configValue).size()) {
                    for (std::string& str : split(configValue, ',')) {
                        strSet.insert(trim(str));
                    }
                }
                retValue = (decltype(retValue))strSet;
            }
            else if (std::is_enum_v<std::remove_reference_t<decltype(retValue)>>) {
                auto val{ stoi(configValue) };

                retValue = (decltype(retValue))val;
            }
            else if (typeid(retValue) == typeid(std::pair<std::string, uint16_t>)) {
                // <string>:<uint16_t>
                std::pair<std::string, uint16_t> val;

                val.first = splitFirst(configValue, ':');
                val.second = (uint16_t)stoul(configValue);
                retValue = (decltype(retValue))val;
            }
            else if (typeid(retValue) == typeid(std::map<std::string, uint64_t>)) {
                // <string>[:<string>][,<string>[:<string>]...]
                std::map<std::string, uint64_t> valStrMap;

                if (!configValue.empty()) {
                    for (std::string& splitConfigValue : split(configValue, ',')) {
                        std::vector<std::string> valStr = split(splitConfigValue, ':');
                        std::string arena = valStr.size() > 1 ? valStr[0] : "0";

                        valStrMap[arena] = stoull(valStr[1]);
                    }
                }
                retValue = (decltype(retValue))valStrMap;
            }
            else if (typeid(retValue) == typeid(std::filesystem::path)) {
                retValue = (decltype(retValue))std::filesystem::path(configValue);
            }
            else if (typeid(retValue) == typeid(CounterDuration)) {
                retValue = (decltype(retValue))CounterDuration(stoi(configValue));
            }
            else {
                throw std::exception(std::format("Unable to convert the value '{}' of config "
                    "parameter '{}' to '{}'. No conversion available for this destination type!",
                    configValue, key, typeid(retValue).name()).c_str());
            }
        }
        catch (...) {
            throw std::exception(std::format("Unable to convert the value '{}' of config "
                "parameter '{}' to '{}'!", configValue, key, typeid(retValue).name()).c_str());
        }
    }
    else {
        // neither was the parameter found nor is there a default value specified
        throw std::exception(std::format("The config parameter '{}' is not defined in '{}'!",
            key, filePath).c_str());
    }
}


/// <summary>
/// Extract data lines from a mixed-format database file.
/// </summary>
/// <param name="fileName">Parameter name.</param>
/// <param name="cback">Callback function to evaluate a line.</param>
/// <returns></returns>
export bool readDataLines(std::string_view fileName, std::function<void(std::string_view)> cback)
{
    std::ifstream file(fileName.data());

    if (!file)
        return false;

    char c, buffer[256]{};
    int i = 0;
    bool skip = false;

    while ((c = file.get()) != -1) {
        switch (c) {
        case '\n':
            buffer[i] = '\0';
            if ((i > 0) && !skip) {
                cback(buffer);
            }
            i = 0;
            skip = false;
            break;
        default:
            if (i >= 255)
                break;
            if (i == 0)
                skip = !isAlphaNumeric(c);
            buffer[i++] = c;
        case '\r':;
        }
    }
    buffer[i] = '\0';

    if (i > 0) {
        cback(buffer);
    }
    return true;
}


/// <summary>
/// Extract data lines from a mixed-format database file.
/// </summary>
/// <typeparam name="T">Calling class type.</typeparam>
/// <param name="fileName">Parameter name.</param>
/// <param name="callback">Callback function to evaluate a line.</param>
/// <param name="obj">Instance of the calling class.</param>
/// <returns></returns>
export template<typename T>
bool readDataLines(std::string_view fileName, void(T::* callback)(std::string_view line), T* obj)
{
    return readDataLines(fileName, [callback, obj](std::string_view line) {
        (obj->*callback)(line); });
}


/// <summary>
/// Read the Operators.txt file.
/// </summary>
export void readOperators()
{
    c_operators.clear();
    readDataLines("Operators.txt", [](std::string_view line) {
        std::string s{ trim(line) };

        if (s.size() && !s.starts_with(";") && s.find(":") != std::string::npos) {
            std::vector<std::string> nameLevel{ split(s, ':') };

            try {
                std::string op{ trim(toLower(nameLevel[0])) };

                c_operators[op] = std::make_pair(std::stoi(nameLevel[1]), "");
            }
            catch (...) {}
        }
    });

    // add some obligatory operators
    c_operators["aleksandra"] = std::make_pair(5, "");
    c_operators["avalon"] = std::make_pair(4, "");
}


/// <summary>
/// Read the PlayerSquads.txt file.
/// </summary>
export void readPlayerSquads()
{
    g_squadUpdateCount = 0;
    g_squadInfos.clear();
    readDataLines("PlayerSquads.txt", [](std::string_view line) {
        std::string s{ trim(line) };

        if (s.size() && !s.starts_with(";") && s.find(":") != std::string::npos) {
            std::vector<std::string> squadInfo{ split(s, ':') };

            try {
                std::string player{ trim(squadInfo[0]) };
                std::string squad{ trim(squadInfo[1]) };
                uint32_t updateCount{ std::stoul(squadInfo[2]) };

                g_squadInfos[player] = std::make_pair(squad, updateCount);

                if (updateCount > g_squadUpdateCount) {
                    g_squadUpdateCount = updateCount;
                }
            }
            catch (...) {}
        }
        });
}


/// <summary>
/// Write the PlayerSquads.txt file.
/// </summary>
export void writePlayerSquads()
{
    std::ofstream squadsFile;

    // Open the file in append mode
    squadsFile.open("PlayerSquads.txt", std::ios::out);

    if (squadsFile.is_open()) {
        for (auto& [player, squadInfo] : g_squadInfos) {
            squadsFile << player << ":" << squadInfo.first << ":" << squadInfo.second;
            squadsFile << std::endl;
        }
        squadsFile.close();
    }
}


/// <summary>
/// Log a line of text to the DiscordBot log file.
/// </summary>
/// <param name="logMsg">Log message.</param>
export void logDiscordBot(std::string_view logMsg)
{
    std::ofstream botLogFile;
    // Open the file in append mode
    botLogFile.open(g_botLogFilePath, std::ios::app);

    auto const time = std::chrono::current_zone()->to_local(std::chrono::system_clock::now());
    std::string timeStamp{ std::format("{:%Y-%m-%d %H:%M:%S}", time).substr(0, 19) };

    if (botLogFile.is_open()) {
        botLogFile << timeStamp << "  " << logMsg << std::endl;
        botLogFile.close();
    }
}


/// <summary>
/// Read the bot config file.
/// </summary>
export void readConfigFile()
{
    std::string filePath{ (std::filesystem::current_path() / c_configFileName).string() };

    readConfigParam("General", "BotName", c_discordBotName, filePath);
    readConfigParam("General", "ContinuumDirPath", c_continuumDirPath, filePath);
    readConfigParam("General", "SpecialFunctions", c_specialFunctions, filePath, "");
    readConfigParam("General", "DiscordServerType", c_discordServerType, filePath, "svsproleague");
    readConfigParam("General", "AltTabCount", c_altTabCount, filePath, "1");
    readConfigParam("General", "SwitchArenas", c_switchArenas, filePath, "");

    // store the pathname of the continuum log file
    g_logFileName = std::format("{}.log", c_discordBotName);
    g_logFilePath = std::filesystem::path(c_continuumDirPath) / "logs" / g_logFileName;
    // store the pathname of the DiscordBot log file
    g_botLogFilePath = std::filesystem::path(c_continuumDirPath) / "logs" / "DiscordBotD.log";
    // store the flags for special functions
    g_isRecordingEnabled = c_specialFunctions.contains("recording");
    g_isDiscordEnabled = !g_isRecordingEnabled && !c_specialFunctions.contains("nodiscord");
    g_isReportingEnabled = !g_isRecordingEnabled && !c_specialFunctions.contains("noreporting");
    g_isContinuumEnabled = c_discordServerType.contains("continuum");
    g_isNexusEnabled = c_discordServerType.contains("nexus");
}


/// <summary>
/// Read the persistent data file.
/// </summary>
export void readPersistentDataFile()
{
    std::string filePath{ "C:\\Users\\Public\\" + c_perstDataFileName };

    if (!std::filesystem::exists(filePath)) {
        // the persistent data file does not exist yet, just ignore
        return;
    }

    readConfigParam("", "Arena", c_initialArena, filePath, "");
    readConfigParam("", "PlayersMessageIdChat", g_chatChPlayersMessageId, filePath, "0");
    readConfigParam("", "PlayersMessageIdTable", g_tableChPlayersMessageId, filePath, "0");
}


/// <summary>
/// Write the persistent data file.
/// </summary>
export void writePersistentDataFile()
{
    std::ofstream perstDataFile;
    std::string filePath{ "C:\\Users\\Public\\" + c_perstDataFileName };

    // Open the file in append mode
    perstDataFile.open(filePath, std::ios::out);

    if (perstDataFile.is_open()) {
        perstDataFile << "Arena=" << g_arena << std::endl;
        perstDataFile << "PlayersMessageIdChat=" << g_chatChPlayersMessageId << std::endl;
        perstDataFile << "PlayersMessageIdTable=" << g_tableChPlayersMessageId << std::endl;
        perstDataFile.close();
    }
}


