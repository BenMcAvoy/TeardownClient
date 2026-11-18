# Defensive analysis: in-process scripting

## Scope

TeardownClient explores the game's internal Lua runtime and includes a small editor and console prototype for evaluating Lua in the selected game context. This is an in-process scripting experiment, not a report of a vulnerability in Teardown and not a multiplayer anti-cheat evaluation.

## Trust boundary

Code evaluated inside a game process can access the same process state available to the host application. For a game that supports user-created scripts or mods, the defensive question is how the supported extension boundary differs from arbitrary code running with the game's privileges.

## Defensive considerations

- Prefer a documented extension API with explicit capabilities over unrestricted access to the host process.
- Treat third-party scripts as untrusted input. Limit filesystem, network, and gameplay-state access to what the extension needs, and make permissions visible to the user.
- If a game mode synchronizes state with other players, validate shared outcomes at the authoritative service instead of trusting a modified local client.
- Make the distinction between supported mods and internal debugging tools clear to users, including compatibility and trust expectations.

## Limits

This prototype does not implement a sandbox, permission system, or defensive control. The recommendations describe design choices for game and mod-platform developers; they are not claims about Teardown's current security architecture.
