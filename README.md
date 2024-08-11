# FlatOut: Ultimate Carnage FO2 Handling

Plugin to port FlatOut 2 handling into FlatOut: Ultimate Carnage, recreating the game's handling as accurately as I possibly could.

Best paired with [my mod to disable wallriding](https://github.com/gaycoderprincess/FlatOutUCNoWallriding/releases).

https://github.com/user-attachments/assets/4511cec5-a5f2-4912-b35f-546bc4e11307

This mod takes a more unique approach, as instead of tweaking the existing handling values, I copied the FlatOut 2 values one to one, and then modified the game's code to use the values the same way as FlatOut 2 does.

This includes the removal of extra parameters such as SlideControlMultiplier and AntiSpinMultiplier, as well as a conversion of FlatOut 2's smooth steering algorithm, tire grip math and slide control code.

## Installation

- Make sure you have the Steam GFWL version of the game, as this is the only version this plugin is compatible with. (exe size of 4242504 bytes)
- Plop the files into your game folder.
- Enjoy, nya~ :3

## Building

Building is done on an Arch Linux system with CLion being used for the build process. 

Before you begin, clone [nya-common](https://github.com/gaycoderprincess/nya-common) to a folder next to this one, so it can be found.

You should be able to build the project now in CLion.
