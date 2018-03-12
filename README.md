![screenshot](img/logo.PNG)

Littlewolf aims to be a very minimalistic software graphics
engine reminiscent of the early wolfenstein and doom days.

*A Windows 10 release is now available.*

Development packages you will require:

    SDL2-devel

Then build and run with

    make; ./littlewolf

Controls:

    move: W,A,S,D

    turn: H,L

    exit: END, ESCAPE

![screenshot](img/2017-12-12-012113_500x500_scrot.png)

Littlewolf is portable and lightweight, compilable by clang, gcc, g++, mingw32,
and supported on any platform that supports SDL2.

Some ideas for your game:

    - Implement texture mapping.

    - Implement pixel shading where the pixels go darker the further they are from the player.

    - Add sprites using the walls as a zbuffer.

    - Make a game like Faster Than Light (FTL) or 0x10c where you control a spaceship with a programming language.
