# AIssac
A program that plays the binding of Isaac.

## Goal
To write a program (AI?) that can play the binding of isaac. I would like it to be a "non cheating"
AI which I'll define as having to interact with the game the same way a player would. It's input
will only be watching the screen and it's output will be keypresses, there will be no direct
observation of the game's memory.

## Plan of Attack
1) Get screen capture and input working (just on my laptop, not worried about multiplatform).
2) Scrape screen for relevant information (items, bad guys, obsticles, stuff like that).
3) Write a bot that can play the game!

## More detailed plan of Attack.
* I think I have already worked out roughly how screen scraping and input triggering are going to
work. I think the best thing to do is use the existing cocoa apis for this.
* I reeeeeally would love a way to be able to snapshot the running game, tweak some code and then
restart the game from the snapshot, similar to the way casey does live code reloading in handmade
hero. It's obvously a lot more complicated in this case though because I'd need to implement live
reloading in an existing game that doesn't have it built in. I'm hoping there's some way I can use
ptrace to snapshot the game's memory at a point in time and reload it. Sorta feel like that's
definately not gonna work. Memory is one thing but gpu/file system state is another.

## Requirements

* premake (premake4 for building nanovg @TODO: write own makefile for nanovg)
* SDL2
* The Binding of Isaac Rebirth

## LOG
* ####2016-01-08: Project Setup
    Just set up the SDL main loop, some typedefs and started on interfaces. I have some screen
    capture code I wrote previously that I need to modify to work for this.

    **Next Step**: Set up screen capture loop and queue mechanism to get them to the bot code.