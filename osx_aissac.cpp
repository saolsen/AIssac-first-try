/*
  This is the OSX/SDL platform layer.
  
  * Debug output is displayed using SDL, OpenGL and nanovg.
  * Screen capture is done using osx CoreGraphics and IOSurface apis.
  * Game input is done using osx CoreFoundation apis.
  * @TODO: Process snapshotting/checkpointing (for livecoding) is done using TBD.
 */
#include "aissac_platform.h"
#include "osx_aissac.h"

#include "imgui_impl_sdl_gl3.cpp"

// Screen Capture
void
begin_screen_capture(CGDisplayStreamRef *stream,
                     FrameQueue *frame_queue,
                     i32 *capture_width, i32 *capture_height)
{
    void(^handler)(CGDisplayStreamFrameStatus,
                   u64,
                   IOSurfaceRef,
                   CGDisplayStreamUpdateRef) = ^void(CGDisplayStreamFrameStatus status,
                                                     u64 time,
                                                     IOSurfaceRef frame,
                                                     CGDisplayStreamUpdateRef update)
    {
        if (frame_queue->is_active) {
            if (status == kCGDisplayStreamFrameStatusFrameComplete &&
                frame != NULL) {
                IOSurfaceLock(frame, kIOSurfaceLockReadOnly, NULL);

                /* u32 size = IOSurfaceGetAllocSize(frame); */
                /* u32 pitch = IOSurfaceGetBytesPerRow(frame); */
                u32 width = IOSurfaceGetWidth(frame);
                u32 height = IOSurfaceGetHeight(frame);
                u8* data = (u8*)IOSurfaceGetBaseAddress(frame);

                if (data != NULL) {
                    // INFO("Captured Image that is %ix%i", width, height);

                    Frame* frame = NULL;
                    while (!frame) {
                        Frame *try_frame = frame_queue->free_frames_list;
                        if (!try_frame) {
                            ERROR("No free frames to write to!");
                            continue;
                        }

                        if (OSAtomicCompareAndSwapPtrBarrier((void*)try_frame,
                                                             (void*)try_frame->next_frame,
                                                             (void*volatile*)&frame_queue->free_frames_list)) {
                            INFO("Aquired Frame");
                            frame = try_frame;
                        }
                    }

                    INFO("writing to frame");
                    // Write frame data                    
                    frame->next_frame = NULL;
                    frame->frame_width = width;
                    frame->frame_height = height;
                    frame->pixel_components = 4;

                    memcpy(frame->data, data, width*height*4);

                    OSMemoryBarrier();

                    INFO("publishing frame");
                    // Publish frame
                    // @TODO: Use a tail so you don't walk the whole list each time.
                    bool published = false;
                    while (!published) {
                        Frame **tail = &(frame_queue->head);

                        while(*tail != NULL) {
                            tail = &((*tail)->next_frame);
                        }

                        // Try to add our frame to the end
                        if (OSAtomicCompareAndSwapPtrBarrier((void*)NULL,
                                                             (void*)frame,
                                                             (void*volatile*)tail)) {
                            published = true;
                        }
                    }
                }
                IOSurfaceUnlock(frame, kIOSurfaceLockReadOnly, NULL);
            }
        }
    };

    dispatch_queue_t q;
    q = dispatch_queue_create("aissac-q", NULL);

    CGDirectDisplayID display_id = 0;

    CGDisplayModeRef mode = CGDisplayCopyDisplayMode(display_id);

    u32 pixel_width = CGDisplayModeGetPixelWidth(mode);
    u32 pixel_height = CGDisplayModeGetPixelHeight(mode);

    INFO("Screen should be %ix%i", pixel_width, pixel_height);
    *capture_width = pixel_width;
    *capture_height = pixel_height;

    CGDisplayModeRelease(mode);

    *stream = CGDisplayStreamCreateWithDispatchQueue(display_id,
                                                     pixel_width,
                                                     pixel_height,
                                                     'BGRA',
                                                     NULL,
                                                     q,
                                                     handler);
    CGDisplayStreamStart(*stream);
}

void
stop_screen_capture(CGDisplayStreamRef *stream)
{
    CGDisplayStreamStop(*stream);
}

// @TODO: Can load this dynamically for livecoding AI. Fun stuff!
#include "aissac.cpp"

i32
main()
{   
    // Setup SDL
    if(SDL_Init(SDL_INIT_VIDEO)) {
        ERROR("Error initializing SDL: %s", SDL_GetError());
    }

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
    SDL_DisplayMode current;
    SDL_GetCurrentDisplayMode(0, &current);

    SDL_Window *window = SDL_CreateWindow("AIssac",
                                          SDL_WINDOWPOS_CENTERED,
                                          SDL_WINDOWPOS_CENTERED,
                                          1920,
                                          1080,
                                          SDL_WINDOW_OPENGL |
                                          SDL_WINDOW_RESIZABLE);//  |
                                          // SDL_WINDOW_ALLOW_HIGHDPI);

    if (!window) {
        ERROR("Error creating window: %s", SDL_GetError());
    }

    i32 width, height;
    SDL_GL_GetDrawableSize(window, &width, &height);
    INFO("Drawable Resolution: %d x %d", width, height);

    SDL_GLContext context = SDL_GL_CreateContext(window);

    // Use Vsync
    if (SDL_GL_SetSwapInterval(1) < 0) {
        ERROR("Unable to set VSync! SDL Error: %s", SDL_GetError());
    }

    // Setup imguigui
    ImGui_ImplSdlGL3_Init(window);
    // @TODO: Maybe load more fonts for imgui.



    // Setup screen capture.
    FrameQueue frame_queue;
    frame_queue.is_active = 0;
    
    CGDisplayStreamRef stream;
    i32 capture_width, capture_height;
    begin_screen_capture(&stream, &frame_queue, &capture_width, &capture_height);

    u32 num_allocated_frames = 5;
    frame_queue.free_frames_list = NULL;
    frame_queue.frames_storage = (Frame*)calloc(1, sizeof(Frame) * num_allocated_frames);
    u32 bytes_per_frame = capture_width * capture_height * 4;
    // @TODO: Align to cache line boundry.
    frame_queue.frames_data_storage = (u8*)calloc(1, bytes_per_frame * num_allocated_frames);

    Frame *last_frame = NULL;
    for (u32 frame_index = 0; frame_index < num_allocated_frames; frame_index++) {
        Frame *frame = frame_queue.frames_storage + frame_index;
        frame->next_frame = last_frame;
        frame->is_ready_for_processing = 0;
        frame->frame_width = capture_width;
        frame->frame_height = capture_height;
        frame->pixel_components = 4;
        frame->data = frame_queue.frames_data_storage + (frame_index * bytes_per_frame);

        last_frame = frame;
    }

    frame_queue.free_frames_list = last_frame;

    frame_queue.head = NULL;

    OSMemoryBarrier();

    frame_queue.is_active = 1;

    int screen_image = -1;

    // IMGUI test state
    bool show_test_window = true;
    bool show_another_window = false;
    ImVec4 clear_color = ImColor(114, 144, 154);

    bool running = true;
    while(running) {
        i32 w, h;
	SDL_GetWindowSize(window, &w, &h);

        i32 draw_w, draw_h;
        SDL_GL_GetDrawableSize(window, &draw_w, &draw_h);

        SDL_Event event;
        while(SDL_PollEvent(&event)) {
            ImGui_ImplSdlGL3_ProcessEvent(&event);
            switch(event.type) {
            case SDL_QUIT:
                running = false;
                break;
            };
        }

        if(!running) {
            break;
        }

        // New IMGUI Frame.
        ImGui_ImplSdlGL3_NewFrame();

        // Aquire Frame
        Frame* frame = frame_queue.head;
        if (frame) {
            // process frame

            // try displaying frame to our window.
            // @TODO: Should use update image each frame instead of this.
            if (screen_image == -1) {
                /* screen_image = nvgCreateImageRGBA(vg, */
                /*                                   frame->frame_width, */
                /*                                   frame->frame_height, */
                /*                                   0, */
                /*                                   frame->data); */
            } else {
                /* nvgUpdateImage(vg, screen_image, frame->data); */
            }

            // free frame
            // @TODO: think about the ABA problem and how it might happen here.
            frame_queue.head = frame->next_frame;

            bool freed = false;
            while(!freed) {
                Frame* old_free_head = frame_queue.free_frames_list;
                frame->next_frame = old_free_head;
            
                frame->next_frame = old_free_head;
                if (OSAtomicCompareAndSwapPtrBarrier((void*)old_free_head,
                                                     (void*)frame,
                                                     (void*volatile*)&frame_queue.free_frames_list)) {
                    freed = true;
                }
            }
        }

        // @TODO: Do everything here. For the AI.
        // begin test UI
        {
            static float f = 0.0f;
            ImGui::Text("Hello, world!");
            ImGui::SliderFloat("float", &f, 0.0f, 1.0f);
            ImGui::ColorEdit3("clear color", (float*)&clear_color);
            if (ImGui::Button("Test Window")) show_test_window ^= 1;
            if (ImGui::Button("Another Window")) show_another_window ^= 1;
            ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
        }
        // end test UI

        // Render
        glViewport(0, 0, (int)ImGui::GetIO().DisplaySize.x, (int)ImGui::GetIO().DisplaySize.y);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui::Render();

        SDL_GL_SwapWindow(window);

    }

    INFO("Shutting Down");
    SDL_GL_DeleteContext(context);
    SDL_DestroyWindow(window);
    SDL_Quit();
}
