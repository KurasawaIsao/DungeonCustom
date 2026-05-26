#pragma once

class GameUIObject
{
public:
    virtual ~GameUIObject() = default;
    virtual void DrawGameUI() = 0;
};
