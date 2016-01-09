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

* ####2016-01-09: Queue
    Started work on the image capture loop and the queue that passes image data from the OS managed
    background thread to my sdl managed processing thread. This is SO HARD. I really wanted to just
    use processor atomics and do the queue from scratch. First I went down a weird path that ended
    up being a dead end trying to have a circular buffer. I do need to revisit that and figure out
    how to do a proper circular buffer at some point but I went with a simpler design. Now I just
    have a linked list of frames to process and atomically put things onto both the end of the
    queue and a freelist that I use to manage the available frame memory. This works pretty well I
    think. I don't have it displaying on the screen yet so I'm not sure if it really works but I
    guess that is the next step. I need to do more testing of it and also probably make sure the
    memory for the pixels is 16 bit aligned so there's no cache sharing and so I can use simd
    for the compression / pixel swizzling.

    **Next Step**: Pass the real image data and display it. Then maybe work on scaling.