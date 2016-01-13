/*
  This is the OSX/SDL platform layer.
  
  * Debug output is displayed using SDL, OpenGL and nanovg.
  * Screen capture is done using osx CoreGraphics and IOSurface apis.
  * Game input is done using osx CoreFoundation apis.
  * @TODO: Process snapshotting/checkpoining (for livecoding) is done using TBD.

  Probably want to visualize more of what is going on here such as how big the image queue is,
  and how much latency between capture time and process time. Probably want to build a profiler.
 */
#include "aissac_platform.h"
#include "osx_aissac.h"

#include "imgui_impl_sdl_gl3.cpp"

// Library Reloading
bool
lib_reload(CurrentLib *current_lib)
{
    bool lib_reloaded = false;
    const char* library = "libaissac.dylib";
    
    struct stat attr;
    if ((stat(library, &attr) == 0) && (current_lib->id != attr.st_ino)) {
        INFO("New Library to load.");
        
        if (current_lib->handle) {
            dlclose(current_lib->handle);
        }
        
        void* handle = dlopen(library, RTLD_NOW);

        if (handle) {
            current_lib->handle = handle;
            current_lib->id = attr.st_ino;

            // game_update_and_render* game = (game_update_and_render*)dlsym(handle,
            //                                                               "update_and_render");
            AI* ai = (AI*)dlsym(handle, "aissac");
            
            if (ai != NULL) {
                current_lib->ai = *ai;
                INFO("Reloaded Game Library");
                lib_reloaded = true;
                
            } else {
                ERROR("Error loading api symbol.");
                dlclose(handle);
                current_lib->handle = NULL;
                current_lib->id = 0;
            }
            
        } else {
            ERROR("Error loading game library.");
            current_lib->handle = NULL;
            current_lib->id = 0;
        }
    }

    return lib_reloaded;
}

// Frame Queue
FrameQueue*
frame_queue_alloc(i32 display_width, i32 display_height, i32 compression_factor, i32 compression_offset)
{
    FrameQueue* frame_queue = (FrameQueue*)malloc(sizeof(FrameQueue));

    u32 num_allocated_frames = 5; // @TODO: Think about this.
    frame_queue->free_frames_list = NULL;
    frame_queue->frames_storage = (Frame*)calloc(1, sizeof(Frame) * num_allocated_frames);
    u32 bytes_per_frame = ((display_width * display_height) / compression_factor) * 4;
    // @TODO: Align to cache line boundry.
    frame_queue->frames_data_storage = (u8*)calloc(1, bytes_per_frame * num_allocated_frames);

    Frame *last_frame = NULL;
    for (u32 frame_index = 0; frame_index < num_allocated_frames; frame_index++) {
        Frame *frame = frame_queue->frames_storage + frame_index;
        frame->next_frame = last_frame;
        frame->width = display_width / compression_factor;
        frame->height = display_height / compression_factor;
        frame->pixel_components = 4;
        frame->data = frame_queue->frames_data_storage + (frame_index * bytes_per_frame);

        last_frame = frame;
    }

    frame_queue->free_frames_list = last_frame;
    frame_queue->head = NULL;

    OSMemoryBarrier();
    
    return frame_queue;
}

void
frame_queue_free(FrameQueue *frame_queue)
{
    free(frame_queue->frames_storage);
    free(frame_queue->frames_data_storage);
    free(frame_queue);
}

// Aquire a new frame to write to.
Frame*
frame_queue_aquire_frame(FrameQueue *frame_queue)
{
    Frame* frame = NULL;

    while (!frame) {
        Frame *try_frame = frame_queue->free_frames_list;
        if (!try_frame) {
            ERROR("No free frames to write to!");
            break;
        }

        if (OSAtomicCompareAndSwapPtrBarrier((void*)try_frame,
                                             (void*)try_frame->next_frame,
                                             (void*volatile*)&frame_queue->free_frames_list)) {
            frame = try_frame;
        }
    }

    return frame;
}

void
frame_queue_publish(FrameQueue *frame_queue, Frame* frame)
{
    frame->next_frame = NULL;
    
    OSMemoryBarrier();

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

Frame*
frame_queue_get_head(FrameQueue *frame_queue)
{
    Frame* frame = frame_queue->head;
    return frame;
}

void
frame_queue_free_frame(FrameQueue *frame_queue, Frame* frame)
{
    // @TODO: think about the ABA problem and how it might happen here.
    frame_queue->head = frame->next_frame;

    for (;;) {
        Frame* old_free_head = frame_queue->free_frames_list;
        frame->next_frame = old_free_head;
            
        frame->next_frame = old_free_head;
        if (OSAtomicCompareAndSwapPtrBarrier((void*)old_free_head,
                                             (void*)frame,
                                             (void*volatile*)&frame_queue->free_frames_list)) {
            // frame appended to freelist
            break;
        }
    }
}

// Screen Capture
void
get_capture_screen_size(i32 *display_width, i32 *display_height)
{
    CGDirectDisplayID display_id = 0;

    CGDisplayModeRef mode = CGDisplayCopyDisplayMode(display_id);

    *display_width = CGDisplayModeGetPixelWidth(mode);
    *display_height = CGDisplayModeGetPixelHeight(mode);
}

void
screen_capture_stop(CGDisplayStreamRef *stream)
{
    CGDisplayStreamStop(*stream);
}

void
screen_capture_begin(CGDisplayStreamRef *stream,
                     FrameQueue *frame_queue,
                     // i32 *capture_width, i32 *capture_height,
                     i32 compression_factor, i32 compression_offset)
{
    void(^handler)(CGDisplayStreamFrameStatus,
                   u64,
                   IOSurfaceRef,
                   CGDisplayStreamUpdateRef) = ^void(CGDisplayStreamFrameStatus status,
                                                     u64 time,
                                                     IOSurfaceRef frame,
                                                     CGDisplayStreamUpdateRef update)
    {
        if (status == kCGDisplayStreamFrameStatusFrameComplete &&
            frame != NULL) {
            IOSurfaceLock(frame, kIOSurfaceLockReadOnly, NULL);

            /* u32 size = IOSurfaceGetAllocSize(frame); */
            /* u32 pitch = IOSurfaceGetBytesPerRow(frame); */
            u32 width = IOSurfaceGetWidth(frame);
            u32 height = IOSurfaceGetHeight(frame);
            u8* data = (u8*)IOSurfaceGetBaseAddress(frame);

            u32 compressed_width = width / compression_factor;
            u32 compressed_height = height / compression_factor;

            if (data != NULL) {

                Frame *frame = frame_queue_aquire_frame(frame_queue);
                if (frame) {
                    // Write frame data
                    frame->next_frame = NULL;
                    frame->width = compressed_width;
                    frame->height = compressed_height;
                    frame->pixel_components = 4;
                    
                    for (u32 pixel_y = 0;
                         pixel_y < compressed_height;
                         pixel_y++) {
                        for (u32 pixel_x = 0;
                             pixel_x < compressed_width;
                             pixel_x++) {
                            i32 data_index = pixel_y * compression_factor *
                                (4 * width) +
                                pixel_x * compression_factor * 4;
                            i32 compressed_index = pixel_y *
                                (4 * compressed_width) +
                                pixel_x * 4;

                            // Swizzle r and b so we get rgba
                            frame->data[compressed_index] = data[data_index+2];
                            frame->data[compressed_index+1] = data[data_index+1];
                            frame->data[compressed_index+2] = data[data_index];
                            frame->data[compressed_index+3] = data[data_index+3];
                        }
                    }

                    frame_queue_publish(frame_queue, frame);
                }
            }
                
            IOSurfaceUnlock(frame, kIOSurfaceLockReadOnly, NULL);
        }
    };

    dispatch_queue_t q;
    q = dispatch_queue_create("aissac-q", NULL);

    CGDirectDisplayID display_id = 0;

    CGDisplayModeRef mode = CGDisplayCopyDisplayMode(display_id);

    u32 pixel_width = CGDisplayModeGetPixelWidth(mode);
    u32 pixel_height = CGDisplayModeGetPixelHeight(mode);

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

i32
main()
{
    // Get Library
    CurrentLib ai_lib = {.handle = NULL,
                         .id = 0,
                         .ai = {0}};
    lib_reload(&ai_lib);

    // Setup AI memory.
    Memory ai_memory;
    ai_memory.persistent_storage_size = ai_lib.ai.memory_needed;
    ai_memory.persistent_storage = (u8*)calloc(1, ai_lib.ai.memory_needed);
    ai_memory.executable_reloaded = true;
    // @TODO: Make this real when we have some platform stuff we need to call.
    ai_memory.platform_api = {.nah = 1};
    
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

    i32 capture_display_width, capture_display_height;
    get_capture_screen_size(&capture_display_width, &capture_display_height);

    // Setup screen capture.
    // I believe compression is going to be 7 for isaac.
    i32 compression_factor = 7; // 7;
    i32 compression_offset = 1;

    i32 last_compression_factor = -1;
    i32 last_compression_offset = -1;

    // Texture for displaying capture image.
    u32 capture_texture;
    glGenTextures(1, &capture_texture);
    glBindTexture(GL_TEXTURE_2D, capture_texture);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, capture_display_width/7, capture_display_height/7, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
    
    // @TODO: See if there's something better than this, this looks awful when scaled.
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

    FrameQueue* frame_queue = NULL;
    CGDisplayStreamRef screen_capture = NULL;

    Frame* frame = NULL;
    i32 frame_width = 1;
    i32 frame_height = 1;

    // Data for communicating with the AI code.
    bool screen_updated = false;
    i32 screen_width = 1;
    i32 screen_height = 1;
    u8* screen_data = NULL;

    bool running = true;
    while (running) {
        ai_memory.executable_reloaded = lib_reload(&ai_lib);
        
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

        // Set Up display capture.
        if (compression_factor != last_compression_factor ||
            compression_offset != last_compression_offset) {
            
            // Stop old screen capture.
            if (screen_capture) {
                screen_capture_stop(&screen_capture);
                screen_capture = NULL;
            }
            
            // Create new FrameQueue.
            if (frame_queue) {
                frame_queue_free(frame_queue);
                frame_queue = NULL;
            }

            frame_queue = frame_queue_alloc(capture_display_width, capture_display_height,
                                            compression_factor, compression_offset);
            screen_capture_begin(&screen_capture, frame_queue, compression_factor, compression_offset);
                
            last_compression_factor = compression_factor;
            last_compression_offset = compression_offset;
        }

        // Pull Frame
        bool new_frame = false;
        frame = frame_queue_get_head(frame_queue);
        if (frame) {
            new_frame = true;
            frame_width = frame->width;
            frame_height = frame->height;

            // Update texture.
            glBindTexture(GL_TEXTURE_2D, capture_texture);
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, frame_width, frame_height,
                            GL_RGBA, GL_UNSIGNED_BYTE, frame->data);

            // Copy out frame data for processing.
            if (frame_width != screen_width || frame_height != screen_height) {
                screen_width = frame_width;
                screen_height = frame_height;

                if (screen_data) {
                    free(screen_data);
                }

                screen_data = (u8*)malloc(4 * screen_width * screen_height);
            }
            screen_updated = true;
            memcpy(screen_data, frame->data, 4 * screen_width * screen_height);

            // free frame
            frame_queue_free_frame(frame_queue, frame);

            frame = NULL;
        }
        
        // Process screen.
        GameState state;
        state.screen_width = screen_width;
        state.screen_height = screen_height;
        state.screen_data = screen_data;

        GameInput input = ai_lib.ai.process_frame(&ai_memory, state);
        INFO("%i", input.a);

        // Platform UI.
                // Platform debug info.
        {
            ImGui::Begin("Platform");
            ImGui::Text("Performance %.3f ms/frame (%.1f FPS)",
                        1000.0f / ImGui::GetIO().Framerate,
                        ImGui::GetIO().Framerate);
            ImGui::Text("Frame Capture Size (%i x %i)", screen_width, screen_height);
            ImGui::End();
        }
        
        // Screen Management
        {
            ImGui::Begin("Screen");
            ImGui::Text("Compress Image to be 1 pixel per logical pixel.");
            ImGui::Text("Screen Resolution: (%i x %i)",
                        capture_display_width, capture_display_height);
            ImGui::Text("Captured Resolution: (%i x %i)", screen_width, screen_height);
            ImGui::End();
        }

        // Scene view.
        {
            ImGui::Begin("Screen Capture");

            if (screen_data) {
                ImVec2 window_size = ImGui::GetWindowContentRegionMax();
                u64 texture_loc = capture_texture;
                ImGui::Image((void*)texture_loc, window_size);
            }
            ImGui::End();
        }
        // end test UI

        // Render
        glViewport(0, 0, (i32)ImGui::GetIO().DisplaySize.x, (i32)ImGui::GetIO().DisplaySize.y);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui::Render();

        // If compression_factor != last_compression_factor
        // generate a new queue.

        SDL_GL_SwapWindow(window);
    }

    INFO("Shutting Down");

    screen_capture_stop(&screen_capture);
    frame_queue_free(frame_queue);

    free(screen_data);

    SDL_GL_DeleteContext(context);
    SDL_DestroyWindow(window);
    SDL_Quit();
}
