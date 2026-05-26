#pragma once
class ImGuiObject
{
public:
    virtual ~ImGuiObject() = default;
    // Dear ImGui windows/tools only. Use GameUIObject for in-game UI.
    virtual void DrawImGui() = 0;
};
