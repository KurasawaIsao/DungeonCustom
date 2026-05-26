#pragma once
#include <string>

class Enemy;
class Player;

class RecruitManager {
public:
    // Š©—U‚É¬Œ÷‚µ‚½Û‚ÌÀÛ‚Ì’‡ŠÔ¶¬E“o˜^ƒƒWƒbƒN
    static void ExecuteRecruit(Enemy* target, const std::string& customName);

    // Š©—U‚ğ’f‚Á‚½Û‚ÌŒãˆ—
    static void DeclineRecruit(Enemy* target);
};