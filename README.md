# Pawn.RakNet
[![GitHub Release](https://img.shields.io/github/release/katursis/Pawn.RakNet.svg)](https://github.com/katursis/Pawn.RakNet/releases/latest)

Plugin for **SA:MP 0.3.7** server that allows you to capture and analyze RakNet traffic

## About this fork (x64 open.mp)

This branch adds **64-bit open.mp** support on top of the upstream `omp` branch.

Changes vs upstream:
- **x64 Windows and x64 Linux builds** for the open.mp component. The upstream `omp` branch compiles Win32 / Linux x86 only.
- **Removed the `amx_Cleanup` runtime hook** (and the `urmem` dependency that implemented it). On Windows x64 `urmem` silently truncated pointers to 32 bits, corrupting the AMX export table. The plugin now uses `PawnEventHandler::onAmxUnload` from the open.mp SDK for the same bookkeeping.
- **Global BitStream handle registry** so the `BitStream:` cell passed into Pawn scripts is a stable 32-bit ID instead of a raw pointer — safe on 64-bit where pointers don't fit in a `cell`.
- **CI matrix** builds all four targets (Win32, Win64, Linux x86, Linux x64) and a tag-triggered GitHub release publishes the artifacts.
- Minor: single `plugin unloaded` log line, `CMakeLists.txt` strips `-m32` on 64-bit GCC builds without forking the cmake-modules submodule.

Use the `win64` / `linux-x64` artifact against a 64-bit `omp-server`. For 32-bit servers keep using the upstream releases or the `win32` / `linux` artifact from this repo — behavior there is unchanged.

## Main features
* Capture, modify, filter incoming/outgoing packets and RPCs
* Send your own packets and RPCs to a player
* Emulate incoming packets and RPCs from a player

## Documentation

[Pawn.RakNet wiki](https://github.com/katursis/Pawn.RakNet/wiki)

[Official RakNet manual](http://www.jenkinssoftware.com/raknet/manual/index.html)

## Simple example
```pawn
const PLAYER_SYNC = 207;

IPacket:PLAYER_SYNC(playerid, BitStream:bs)
{
  new onFootData[PR_OnFootSync];

  BS_IgnoreBits(bs, 8); // ignore packet id (uint8)
  BS_ReadOnFootSync(bs, onFootData);

  printf(
    "PLAYER_SYNC[%d]:\nlrKey %d \nudKey %d \nkeys %d \nposition %.2f %.2f %.2f \nquaternion %.2f %.2f %.2f %.2f \nhealth %d \narmour %d \nadditionalKey %d \nweaponId %d \nspecialAction %d \nvelocity %.2f %.2f %.2f \nsurfingOffsets %.2f %.2f %.2f \nsurfingVehicleId %d \nanimationId %d \nanimationFlags %d",
    playerid,
    onFootData[PR_lrKey],
    onFootData[PR_udKey],
    onFootData[PR_keys],
    onFootData[PR_position][0],
    onFootData[PR_position][1],
    onFootData[PR_position][2],
    onFootData[PR_quaternion][0],
    onFootData[PR_quaternion][1],
    onFootData[PR_quaternion][2],
    onFootData[PR_quaternion][3],
    onFootData[PR_health],
    onFootData[PR_armour],
    onFootData[PR_additionalKey],
    onFootData[PR_weaponId],
    onFootData[PR_specialAction],
    onFootData[PR_velocity][0],
    onFootData[PR_velocity][1],
    onFootData[PR_velocity][2],
    onFootData[PR_surfingOffsets][0],
    onFootData[PR_surfingOffsets][1],
    onFootData[PR_surfingOffsets][2],
    onFootData[PR_surfingVehicleId],
    onFootData[PR_animationId],
    onFootData[PR_animationFlags]
  );

  return 1;
}
```
