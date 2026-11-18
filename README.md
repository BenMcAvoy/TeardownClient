# TeardownClient

TeardownClient is a Windows C++ reverse-engineering experiment around Teardown's internal Lua runtime. Its ImGui interface explores the game's script state and provides a small editor/console prototype for evaluating Lua in the selected client context.

## Build

Open `TeardowClient.sln` in Visual Studio and build the x64 configuration. The solution uses vcpkg manifest dependencies.

## Status

This is an internal, game-version-specific research prototype, not a supported public mod. It depends on private game structures and should only be used in environments where you have permission to modify the game.
