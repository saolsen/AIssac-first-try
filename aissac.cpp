#include "aissac.h"

GameInput process_frame(Memory *memory, GameState gamestate)
{
    AIState* state = (AIState*)memory->persistent_storage;
    if (!state->is_initialized) {

        state->is_initialized = true;
    }
    state->wudup_tho++;

    ImGui::Begin("AI");
    ImGui::Text("I can add UI from the library!");
    ImGui::End();
    
    GameInput frame_results;
    frame_results.a = state->wudup_tho;
    return frame_results;
}

extern "C" AI aissac = {.memory_needed = sizeof(AIState),
                        .process_frame = process_frame};
