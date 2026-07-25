#include "CheckpointManager.h"
#include "SKSEMenuFramework.h"
#include "UI.h"

namespace Menu
{
    void __stdcall Render()
    {
        if (ImGui::Button("Set Current Location")) {
            CheckpointManager::GetSingleton().SetRespawnLocation();
        }
    }

    void Register()
    {
        if (!SKSEMenuFramework::IsInstalled()) {
            return;
        }

        SKSEMenuFramework::SetSection("Shades Respawn Addon");
        SKSEMenuFramework::AddSectionItem("Respawn Location", Render);
    }
}
