#pragma once
#include "GameRandom.h"
#include "Vector2Int.h"
#include "external/json.hpp"
#include <vector>
#include <limits>
#include <string>

enum class RoomSpecialType
{
    Normal,
    MonsterHouse,
    Shop,
    Treasure,
};

class Room
{
private:
    Vector2Int m_position;
    Vector2Int m_size;
  
    struct SubRect { Vector2Int pos; Vector2Int size; };
    std::vector<SubRect> m_subRects;
public:
    // ============================
    // 部屋ID及び入口マス（巡回・管理用）
    // ============================
    int id = -1;
    std::vector<Vector2Int>  entrances;

    //自分で作ったした部屋か
    bool isFixed = false;

    RoomSpecialType specialType = RoomSpecialType::Normal;
    int specialTrapDensity = 0;
    bool specialMessageShown = false;
    std::string specialItemTableId;

    Room() = default;
    Room(const Vector2Int& pos, const Vector2Int& sz)
        : m_position(pos), m_size(sz) {}

    // ============================
    // 基本情報
    // ============================
    const Vector2Int& GetPosition() const { return m_position; }
    const Vector2Int& GetSize() const { return m_size; }

    void SetPosition(const Vector2Int& pos) { m_position = pos; }
    void SetSize(const Vector2Int& size) { m_size = size; }

    // ============================
    // 部屋中心
    // ============================
    Vector2Int GetCenter() const {
        if (m_subRects.empty()) {
            return Vector2Int(m_position.x + m_size.x / 2, m_position.y + m_size.y / 2);
        }
        // 厳密な中心に近い「実在するタイル」を返す（L字型などで中心が壁になるのを防ぐ）
        return m_subRects[m_subRects.size() / 2].pos;
    }

    // ============================
    // 判定系
    // ============================
    bool Contains(const Vector2Int& pos) const {
        // m_subRects が空（初期状態など）なら、矩形判定を行う
        if (m_subRects.empty()) {
            return (pos.x >= m_position.x && pos.x < m_position.x + m_size.x &&
                pos.y >= m_position.y && pos.y < m_position.y + m_size.y);
        }
        // タイル集合が登録されているなら、その中にあるかチェック（凹凸・角削りに対応）
        for (const auto& rect : m_subRects) {
            if (pos.x >= rect.pos.x && pos.x < rect.pos.x + rect.size.x &&
                pos.y >= rect.pos.y && pos.y < rect.pos.y + rect.size.y) {
                return true;
            }
        }
        return false;
    }


    bool Overlaps(const Room& other) const
    {
        int XMin = m_position.x;
        int XMax = m_position.x + m_size.x - 1;
        int YMin = m_position.y;
        int YMax = m_position.y + m_size.y - 1;

        int oXMin = other.m_position.x;
        int oXMax = other.m_position.x + other.m_size.x - 1;
        int oYMin = other.m_position.y;
        int oYMax = other.m_position.y + other.m_size.y - 1;

        return !(XMax + 2 < oXMin || XMin - 2 > oXMax ||
            YMax + 2 < oYMin || YMin - 2 > oYMax);
    }

    // ============================
    // ランダム床（既存）
    // ============================
    Vector2Int GetRoomInsideRandam() const {
        if (!m_subRects.empty()) {
            int index = GameRandom::Index(m_subRects.size());
            return m_subRects[index].pos;
        }

        if (m_size.x <= 2 || m_size.y <= 2)
            return Vector2Int(m_position.x, m_position.y);

        int rx = GameRandom::Range(m_position.x + 1, m_position.x + m_size.x - 2);
        int ry = GameRandom::Range(m_position.y + 1, m_position.y + m_size.y - 2);
        return Vector2Int(rx, ry);
    }
    void Deserialize(const nlohmann::json& j)
    {
        m_position.x = j["x"];
        m_position.y = j["y"];
        m_size.x = j["w"];
        m_size.y = j["h"];
    }

    void AddSubRect(Vector2Int p, Vector2Int s) {
        m_subRects.push_back({ p, s });
    }
    std::vector<SubRect> GetSubRects() { return m_subRects; }

    void ClearSubRects() { m_subRects.clear(); m_subRects.shrink_to_fit(); }

};


