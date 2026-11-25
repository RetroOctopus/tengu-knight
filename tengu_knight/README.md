# TENGU KNIGHT

A side-view arena game for the TenguCon 3.0 badge (ESP32-S3). A knight in a
tengu mask must slay 50 of the undead, at least 5 of each kind, to draw out
The Lich. Runs as an `.elf` from the SD card; no reflashing.

## Install

Copy `tengu_knight.elf` to the SD card root, then Files -> `tengu_knight.elf` -> A.

First launch asks once for Up, Down, Left, Right, Attack (A), Item (B) and
Menu (ESC or any key). The choice is saved to the SD card.

## Controls

| Input | Action |
|---|---|
| Left / Right | Walk |
| Stand still | Shield up: blocks attacks from the front |
| A | Sword swing. Mash for a 4-hit combo: flat slash, rising slash, falling slash, overhead |
| B | Use the selected item. With the shield selected: shield bash |
| Down | Scroll: shield, then each item you hold |
| Menu key | Back to the menu |

Each swing takes about a quarter of a second, and you can walk and turn while
swinging. A swing also cuts a spider's web out of the air. Webbed by a spider? Tap A repeatedly, or
use a cross to burn the web off at once.

**Shield bash:** half the sword's reach, no damage, stuns for 1.5 seconds.
It does nothing to the black knight or the Lich.

## Goal

Beat 50 enemies in total and at least 5 of each kind (the top bar keeps
count). Enemies arrive at random, easier kinds first; a kind you have beaten
many of turns up less. When both goals are met and the arena is clear, The
Lich appears.

## Items (dropped by enemies)

| Item | Per drop | Max held | Effect |
|---|---|---|---|
| Throwing knives | 2 | no limit | Fly straight ahead; two knives equal one sword hit |
| Cross | 1 | 6 | Blast around you: kills small enemies, hurts the black knight, burns webs, not the Lich |
| Fire flask | 1 | 2 | Thrown in an arc, leaves fire about 3 knights wide for 4 seconds; burns for one sword hit every half second |
| Heal potion (rare) | 1 | 2 | Restores 2 hearts |

Enemies also have a 40% chance to drop half a heart, which heals as soon as
you walk over it.

## Enemies

| Enemy | Sword hits | Notes |
|---|---|---|
| Skeleton | 3 | Fast and aggressive; sometimes hops back to dodge |
| Red-eyed skeleton | 3 | Brown bones, red eyes; releases a flying skull when it dies |
| Flying skull | 2 | Keeps its distance and spits fireballs |
| Spider | 2 | Spits web to pin you, then bites; sometimes lunges instead. Only one at a time |
| Snake man | 5 | Sword slash, or a long-necked bite from range. Up to 3 at once |
| Black knight | 7 | Halberd combo: overhead, sweep, thrust. Up to 2 at once |

**The Lich:** green fireballs; green circles under you (2.5 seconds to step
off); summons skeletons and skulls (long wind-up, free hits); red eyes then a
burst of fire around him; a staff swipe followed by an overhead bash that the
shield cannot stop. Standing right next to him for long sets off the fire.

## Saving

Buttons and the top 5 scores are stored in `TKNIGHT.SAV` on the SD card. The
firmware exports no file functions to apps, so the game calls the firmware's
own open/read/write at their addresses in firmware build 69d9791, after
checking the firmware code there matches that build. On other firmware the
game still runs but does not save. Nothing is written to flash.

## Build

`build.sh` (Linux/macOS) or `build_windows.ps1` with Espressif's Xtensa
toolchain esp-15.2.0_20251204. Keep `source/text_pad.S` last on the command
line: the badge's ELF loader crashes with LoadStoreError if `.text` is not a
multiple of 4 bytes, and the scripts refuse to output such a file.

Status: tested in a host simulator; not yet tested on badge hardware.
