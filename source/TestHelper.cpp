module TestHelper;

import Spawn;

import <format>;


/// <summary>
/// Set up the module. Register event and command handlers.
/// </summary>
void TestHelper::setup()
{
    // register command handlers
    // level 3:
    registerCommandHandler("test", &TestHelper::handleCommandTest, this,
        { { OperatorLevel::SuperModerator, CommandScope::External,
            "Usage: !test <name>  (execute an action for a testcase)\n"
            "<name>=statstable  (print a practice stats table)",
            getAliasesDescription("test") }
        });
}


/// <summary>
/// Close the module. Free all ressources.
/// </summary>
void TestHelper::close()
{
}


//////////////////////////////////////////////////////////////////////////////////////////////////

MessageList TestHelper::handleCommandTest(const Player& player, const Command& cmd)
{
    MessageList retMessages;

    if (cmd.getFinal() == "stattable") {
        // print a stat table to test the formatting of stat tables for transmission to Discord
        sendArena("Final Score : 12 - 10 Freq 100 wins -- Game Time : 11 : 24");
        sendArena("+-------------------------------------------------------+--------------------+---------+----------+----------+----------------+");
        sendArena("| F 100: Freq 100  Ki/De TK LO SK AS FR WR WRk Mi PTime | DDealt/DTaken DmgE | AcB AcG |    W-L   |   Ki/De  |  Rat TRat ERat |");
        sendArena("+-------------------------------------------------------+--------------------+---------+----------+----------+----------------+");
        sendArena("| Minor Threat      3/ 2  0  0  0  4  4  1   3  1  8:56 |  18866/ 21562  46% |  62  11 |   13-18  |   50/66  |  +11  527  884 |");
        sendArena("| Tom Petty         3/ 1  0  0  1  2  1  2   2  0  8:56 |  12515/ 18755  40% |  27  12 |   21-19  |   95/85  |  +11  671 1236 |");
        sendArena("| Pic               3/ 1  0  0  0  0  6  2   3  0  8:56 |  18806/ 13533  58% |  45  12 |   13-14  |   52/60  |  +10  602 1399 |");
        sendArena("| Benji             0/ 0  0  0  0  2  0  0   0  0  8:56 |  14971/ 17520  46% |  48  10 |    7-7   |   32/29  |   +2  508 1486 |");
        sendArena("| TOTAL:            9/ 4  0  0  1  8 11  5   8  1       |  65158/ 71370      |  44  11 |   54-58  |  229/240 |   +8  577 1251 |");
        sendArena("+-------------------------------------------------------+--------------------+---------+----------+----------+----------------+");
        sendArena("| F 200: Freq 200  Ki/De TK LO SK AS FR WR WRk Mi PTime | DDealt/DTaken DmgE | AcB AcG |    W-L   |   Ki/De  |  Rat TRat ERat |");
        sendArena("+-------------------------------------------------------+--------------------+---------+----------+----------+----------------+");
        sendArena("| Sabrewolf         1/ 3  0  0  1  1  0  5   6  1  7:06 |  17089/ 12715  57% |  51  18 |    6-9   |   29/28  |  -33  566 1274 |");
        sendArena("| the seven year    2/ 3  0  0  0  0  2  2   4  0  8:29 |  17211/ 21064  44% |  43  17 |   20-26  |   87/105 |   +1  633 1251 |");
        sendArena("| el caca           1/ 1  0  1  0  1  2  0   1  0  8:42 |  16703/ 13174  55% |  40  19 |    6-9   |   14/40  |   +8  343  759 |");
        sendArena("| MizzKitty         0/ 2  0  1  0  1  2  1   1  0  8:31 |  16469/ 21370  43% |  39  28 |    7-15  |   21/50  |   -1  372  745 |");
        sendArena("| TOTAL:            4/ 9  0  2  1  3  6  8  12  1       |  67472/ 68323      |  42  20 |   39-59  |  151/223 |   -6  478 1006 |");
        sendArena("+-------------------------------------------------------+--------------------+---------+----------+----------+----------------+");
        sendArena("Stats for this game can be viewed at: http://svssubspace.com/?page=Game&id=1359580");
        sendArena("MVP: psycho (+16)      -- Runner Up: YELLOW HAMMER (+16)");
        sendArena("LVP: Mongoose (-8)      -- Runner Up: Tool (-7)");
    }
    else {
        retMessages.push_back(std::format("Unknown test helper '{}'", cmd.getFinal()));
    }

    return retMessages;
}
