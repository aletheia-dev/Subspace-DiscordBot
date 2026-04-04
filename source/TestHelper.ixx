export module TestHelper;

import ModuleBase;

export class Spawn;


/// <summary>
/// Module provides functionality for manual tests of the Discord plugin.
/// </summary>
export class TestHelper : public ModuleBase<Spawn>
{
public:
    /// <summary>
    /// Constructor. Initialize the plugin module base with a reference to the spawn.
    /// </summary>
    /// <param name="spawn">Spawn</param>
    TestHelper(Spawn& spawn) : ModuleBase(spawn) {}

    /// <summary>
    /// Set up the module. Register event and command handlers.
    /// </summary>
    virtual void setup();

    /// <summary>
    /// Close the module. Free all ressources.
    /// </summary>
    virtual void close();

private:
    MessageList handleCommandTest(const Player& player, const Command& cmd);
};
