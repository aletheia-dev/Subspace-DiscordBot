//#pragma warning(disable: 4251)
#define WINDOWS_LEAN_AND_MEAN
#include <windows.h>
//#include <dpp/dpp.h>
#include <memory>

import <string>;
import <format>;
import <fstream>;

import Commands;
import Config;
import Discord;
import Events;
import Subspace;
import Recording;


/// <summary>
/// Initialize observation of the Subspace log file.
/// </summary>
/// <param name="f1">Callback for executing any function with a single parameter.</param>
/// <param name="f2">Callback for sending a keystroke to OBS.</param>
/// <returns>True, if the start of the bot was successful, otherwise false.</returns>
extern "C" _declspec(dllexport) uint32_t start(AHKf1Callback f1, AHKf2Callback f2)
{
    if (g_isStarted) {
        return true;
    }

    // store the AHK callback functions for later use
    execCmd = f1;
    sendObs = f2;

    try {
        // read the bot configuration, persistent data and bot operators files
        readConfigFile();
        readPersistentDataFile();
        readPlayerSquads();
        readOperators();
        readSubspaceUserLinks();

        // login with the bot player and switch to Subspace chat window
        login();
        // setup Subspace commands
        setupSubspace();
        // setup the Discord client for message transfer
        setupDiscord();
        // setup recording of practices and matches
        setupRecording();
        // start Subspace log observation
        startObserving();
        // initially go to the last arena where the bot has been in
        switchArena();

        // take care that key strokes reach the OBS window for recording
        execCmd("DetectHiddenWindows", "true");
        g_isStarted = true;
        g_isActive = true;
        return true;
    }
    catch (std::exception& ex) {
        execCmd("MsgBox", ex.what());
        return false;
    }
}


/// <summary>
/// Force match recording. Used in cases where the bot is late to a match.
/// </summary>
extern "C" _declspec(dllexport) void force()
{
    forceRecording();
}


/// <summary>
/// Process log file observation and task handling until a time-consuming task has been finished
/// <returns>True, if the bot is still active, or false, in case the bot shut down die to a 
/// connection loss.</returns>
/// </summary>
extern "C" _declspec(dllexport) uint32_t process()
{
    bool taskFinished = false;
    std::string logMsg;

    while (g_isStarted && !taskFinished) {
        try {
            if (!g_isReconnecting) {
                logMsg = getNextLogMessage();

                // process commands
                taskFinished = processCommands(logMsg);
                // process private messages that are applicable for recording
                if (!taskFinished) {
                    taskFinished = processPrivateRecordingMessage(logMsg);
                }
                // process private messages that are applicable for Discord
                if (!taskFinished) {
                    taskFinished = processPrivateDiscordMessage(logMsg);
                }

                // process events that occur due to log content
                if (!taskFinished) {
                    taskFinished = processEvents(logMsg);
                }
            }

            if (logMsg.empty()) {
                // no log messages to process, so return
                taskFinished = true;
            }
        }
        catch (std::exception& ex) {
            execCmd("MsgBox", ex.what());
        }
    }

    return g_isActive;
}


/// <summary>
/// Stop observation of the Subspace log file.
/// </summary>
extern "C" _declspec(dllexport) void stop()
{
    if (g_isStarted && !g_isContinuumEnabled) {
        shutdown();
    }
}

bool has{};

/// <summary>
/// Test helper.
/// </summary>
extern "C" _declspec(dllexport) void test()
{
    //std::ofstream perstDataFile;

    //// Open the file in append mode
    //perstDataFile.open(g_logFilePath, std::ios::app);

    //if (perstDataFile.is_open()) {
    //    if (!has)
    //        perstDataFile << "  GO!" << std::endl;
    //    else 
    //        perstDataFile << "  player 1 kb player 2" << std::endl;
    //    has = true;
    //    perstDataFile.close();
    //}
}

