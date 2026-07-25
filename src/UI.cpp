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
            spdlog::warn("SKSE Menu Framework was not found; respawn location cannot be set");
            return;
        }

        SKSEMenuFramework::SetSection("Shades Respawn Addon");
        SKSEMenuFramework::AddSectionItem("Respawn Location", Render);
        spdlog::info("Registered SKSE Menu Framework page");
    }
}
