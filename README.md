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

* OSX
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

* ####2016-01-10: Capture
    Got "real time" image capture going. I'm capturing the data in the os thread, passing it through
    my queue and then displaying it on the screen. It is too slow/laggy to play in real time right
    now but it's an awesome start. I need to do color correction and compression of the screen so
    that what comes through are only the "logical" binding of isaac pixels instead of the whole
    (quite high rez) screen. This is pretty awesome so far though. I might also need to revisit
    my queue and see if that is the bottleneck, It could be cache line sharing or the linked list
    traversal slowing things down too, it's hard to say, I need to set up some way to profile it.

    **Next Step**: Color correction and compression.

* ####2016-01-11: Imgui
    Well I got a bit bogged down by nanovg and I realised it was going to be a lot easier to
    set up any debug UI I want if I just use ImGui. I started down a few paths to get it running.
    First I tried cimgui but couldn't figure it out, then I tried just having the gui code in cpp
    and the rest still in c but ultimately since I had to use a cpp linker anyway I just ported
    the whole project over to cpp. I'll probably still write this almost completely in a c style
    but it's now all cpp. I did some work to get ImGui up and running too and I'm very happy with
    it. Now I need to get the image drawing again and go back to working on cropping, color
    corections and compression.

    **Next Step**: Draw image, color correction and compression.

* ####2016-01-12: Compression
    ![Image of 2016-01-12 results.](https://dl.dropbox.com/s/udvj73mqloj8369/Screenshot%202016-01-12%2017.14.29.png)

    I got the image drawing again using an opengl texture. I then cleaned some stuff up and set it
    up to compress the image. The native resolution of my laptop screen, which I am capturing from
    is 2880x1800. Copying this image to memory (and again to a texture) is very expensive. Just
    getting it to draw on the screen was eating away at my ability to run at 60fps. I have done
    no image processing of it yet either so things would only get worse. Lucky for me binding of
    isaac is a game that uses pixel art. That means I can compress this image so that I have 1
    actual pixel for every "logical" pixel in the art. From some experimenting it turned out to be
    7 actual pixels per art pixel at this resolution so I can compress the screen image down to
    441 x 257 without any loss of information! This now runs fast enough that I can look at my
    debug screen instead of the actual screen and play the game just fine. This small image will
    be a lot better for image processing too.
    The compressed image is a bit worse in that animations aren't as smooth because you loose
    subpixel rendering since we're just sampling every 7 pixels. I don't think that's going to be
    a problem for image processing code though.
    I also did the r<->b color correction so the pixels are in rgba format.

    **Next Step**: Finish the platform AI interface.

* ####2016-01-12: Interface
    I did a little more work today to make sure the platform/AI interface is good. I set up live reloading of the AI layer
    so you can update and recompile AI code on the fly and it updates in the app. I also set up having the platform allocate
    a block of memory for the AI to use so things don't go away when you reload it. I then verified that the AI code can draw
    UI to the screen so I think I have just about all I need to start writing an AI. This is gonna be the hard part...

    **Next Step**: Start trying to make sense of the image.