#include "RoomGenerator.h"
#include "GameRandom.h"
#include "MapData.h"
#include "MapLoader.h"
#include "Room.h"
#include "scene.h"
#include <algorithm>
#include <cstdlib>
#include <vector>

namespace
{
    constexpr int kRoomMargin = 2;
    constexpr int kMaxRoomPlaceAttempts = 50;
    constexpr int kLayoutGap = 4;
    constexpr int kSmallLayoutGap = 3;

    struct PatternRoomSpec
    {
        int x;
        int y;
        bool treasure = false;
    };

    struct PatternRoomRect
    {
        int x;
        int y;
        int w;
        int h;
        bool treasure = false;
    };

    int RandomRange(int minValue, int maxValue)
    {
        if (maxValue <= minValue) return minValue;
        return GameRandom::Range(minValue, maxValue);
    }

    int ClampInt(int value, int minValue, int maxValue)
    {
        return (std::max)(minValue, (std::min)(maxValue, value));
    }

    bool OverlapsAnyRoom(MapData* map, const Room& newRoom)
    {
        for (const Room& room : map->GetRooms())
        {
            if (room.Overlaps(newRoom))
                return true;
        }
        return false;
    }

    bool IsFixedRoomFloor(TileType type)
    {
        return type == TileType::Floor || type == TileType::Stair || type == TileType::Corridor;
    }

    bool BuildFixedRoomFromTiles(const MapData& fixedData, const Vector2Int& offset, Room& outRoom)
    {
        int minX = fixedData.GetWidth();
        int minY = fixedData.GetHeight();
        int maxX = -1;
        int maxY = -1;
        std::vector<Vector2Int> floorTiles;

        for (int y = 0; y < fixedData.GetHeight(); ++y)
        {
            for (int x = 0; x < fixedData.GetWidth(); ++x)
            {
                if (!IsFixedRoomFloor(fixedData.GetTile(x, y))) continue;

                minX = (std::min)(minX, x);
                minY = (std::min)(minY, y);
                maxX = (std::max)(maxX, x);
                maxY = (std::max)(maxY, y);
                floorTiles.push_back(offset + Vector2Int(x, y));
            }
        }

        if (floorTiles.empty()) return false;

        // 固定部屋は外周に壁を含むことがあるため、歩けるタイルだけを部屋判定に使う。
        outRoom = Room(offset + Vector2Int(minX, minY), Vector2Int(maxX - minX + 1, maxY - minY + 1));
        outRoom.isFixed = true;
        for (const Vector2Int& tile : floorTiles)
            outRoom.AddSubRect(tile, Vector2Int(1, 1));

        return true;
    }
    void RebuildRoomSubRects(MapData* map)
    {
        if (!map) return;

        for (Room& room : map->GetRooms())
        {
            room.ClearSubRects();

            const Vector2Int pos = room.GetPosition();
            const Vector2Int size = room.GetSize();

            for (int y = pos.y; y < pos.y + size.y; ++y)
            {
                for (int x = pos.x; x < pos.x + size.x; ++x)
                {
                    if (map->GetTile(x, y) == TileType::Floor)
                        room.AddSubRect({ x, y }, { 1, 1 });
                }
            }
        }
    }

    MapLayoutType PickLayoutType(const FloorData& floor)
    {
        int totalWeight = 0;
        for (const LayoutWeight& layout : floor.layoutWeights)
        {
            totalWeight += (std::max)(0, layout.weight);
        }

        if (totalWeight <= 0)
            return MapLayoutType::Random;

        int roll = GameRandom::Range(0, totalWeight - 1);
        for (const LayoutWeight& layout : floor.layoutWeights)
        {
            const int weight = (std::max)(0, layout.weight);
            if (weight <= 0) continue;
            if (roll < weight)
                return layout.type;
            roll -= weight;
        }

        return MapLayoutType::Random;
    }

    void NormalizePattern(std::vector<PatternRoomSpec>& specs)
    {
        if (specs.empty()) return;

        int minX = specs[0].x;
        int minY = specs[0].y;
        for (const PatternRoomSpec& spec : specs)
        {
            minX = (std::min)(minX, spec.x);
            minY = (std::min)(minY, spec.y);
        }

        for (PatternRoomSpec& spec : specs)
        {
            spec.x -= minX;
            spec.y -= minY;
        }
    }

    void MirrorPatternX(std::vector<PatternRoomSpec>& specs)
    {
        int maxX = 0;
        for (const PatternRoomSpec& spec : specs)
            maxX = (std::max)(maxX, spec.x);
        for (PatternRoomSpec& spec : specs)
            spec.x = maxX - spec.x;
    }

    void MirrorPatternY(std::vector<PatternRoomSpec>& specs)
    {
        int maxY = 0;
        for (const PatternRoomSpec& spec : specs)
            maxY = (std::max)(maxY, spec.y);
        for (PatternRoomSpec& spec : specs)
            spec.y = maxY - spec.y;
    }

    void RotatePatternRight(std::vector<PatternRoomSpec>& specs)
    {
        for (PatternRoomSpec& spec : specs)
        {
            const int oldX = spec.x;
            spec.x = spec.y;
            spec.y = -oldX;
        }
        NormalizePattern(specs);
    }

    void RotateRectsRight(std::vector<PatternRoomRect>& rects)
    {
        for (PatternRoomRect& rect : rects)
        {
            const int oldX = rect.x;
            const int oldW = rect.w;
            rect.x = rect.y;
            rect.y = -oldX - oldW;
            rect.w = rect.h;
            rect.h = oldW;
        }

        if (rects.empty()) return;

        int minX = rects[0].x;
        int minY = rects[0].y;
        for (const PatternRoomRect& rect : rects)
        {
            minX = (std::min)(minX, rect.x);
            minY = (std::min)(minY, rect.y);
        }

        for (PatternRoomRect& rect : rects)
        {
            rect.x -= minX;
            rect.y -= minY;
        }
    }

    bool AddPatternRooms(MapData* map, const std::vector<PatternRoomRect>& rects)
    {
        if (!map || rects.empty()) return false;

        int layoutW = 0;
        int layoutH = 0;
        for (const PatternRoomRect& rect : rects)
        {
            layoutW = (std::max)(layoutW, rect.x + rect.w);
            layoutH = (std::max)(layoutH, rect.y + rect.h);
        }

        if (layoutW + kRoomMargin * 2 > map->GetWidth() || layoutH + kRoomMargin * 2 > map->GetHeight())
            return false;

        const int maxStartX = map->GetWidth() - layoutW - kRoomMargin;
        const int maxStartY = map->GetHeight() - layoutH - kRoomMargin;
        const int startX = (maxStartX >= kRoomMargin) ? RandomRange(kRoomMargin, maxStartX) : (map->GetWidth() - layoutW) / 2;
        const int startY = (maxStartY >= kRoomMargin) ? RandomRange(kRoomMargin, maxStartY) : (map->GetHeight() - layoutH) / 2;

        map->GetRooms().clear();
        map->SetAllTile(TileType::Wall);
        map->SetAllActive(false);

        for (const PatternRoomRect& rect : rects)
        {
            const int x = startX + rect.x;
            const int y = startY + rect.y;
            map->SetActiveRect(x - 2, y - 2, x + rect.w + 1, y + rect.h + 1, true);

            Room room({ x, y }, { rect.w, rect.h });
            room.id = (int)map->GetRooms().size();
            if (rect.treasure)
                room.specialType = RoomSpecialType::Treasure;

            map->AddRoom(room);
            map->ApplyRoom(map->GetRooms().back());
        }

        return true;
    }

    Vector2Int GetRectLayoutSize(const std::vector<PatternRoomRect>& rects)
    {
        Vector2Int size(0, 0);
        for (const PatternRoomRect& rect : rects)
        {
            size.x = (std::max)(size.x, rect.x + rect.w);
            size.y = (std::max)(size.y, rect.y + rect.h);
        }
        return size;
    }

    void ResizeMapForLayoutIfNeeded(MapData* map, const Vector2Int& layoutSize)
    {
        if (!map) return;

        const int needW = layoutSize.x + kRoomMargin * 2 + RandomRange(4, 12);
        const int needH = layoutSize.y + kRoomMargin * 2 + RandomRange(4, 12);
        const int newW = (std::max)(map->GetWidth(), needW);
        const int newH = (std::max)(map->GetHeight(), needH);

        if (newW != map->GetWidth() || newH != map->GetHeight())
            map->Reset(newW, newH);
    }

    bool TryGenerateSmallRoomChainRooms(MapData* map)
    {
        if (!map) return false;

        for (int attempt = 0; attempt < 50; ++attempt)
        {
            const bool vertical = GameRandom::Range(0, 1) == 0;
            const int columns = RandomRange(4, 7);
            const bool useBridgeRoom = GameRandom::Percent(18);
            const int bridgeColumn = useBridgeRoom ? RandomRange(1, columns - 2) : -1;
            const int laneGap = RandomRange(5, 9);
            const int lane0Y = 0;

            std::vector<int> topW(columns), topH(columns), bottomW(columns), bottomH(columns);
            for (int i = 0; i < columns; ++i)
            {
                topW[i] = RandomRange(5, 7);
                topH[i] = RandomRange(5, 7);
                bottomW[i] = RandomRange(5, 7);
                bottomH[i] = RandomRange(5, 7);
            }

            const int maxTopH = *std::max_element(topH.begin(), topH.end());
            const int lane1Y = maxTopH + laneGap;

            std::vector<PatternRoomRect> rects;
            int x = 0;
            for (int i = 0; i < columns; ++i)
            {
                if (i == bridgeColumn)
                {
                    const int bridgeW = RandomRange(8, 14);
                    const int bridgeH = lane1Y + bottomH[i];
                    rects.push_back({ x, lane0Y, bridgeW, bridgeH, false });
                    x += bridgeW + RandomRange(4, 8);
                    continue;
                }

                const int columnW = (std::max)(topW[i], bottomW[i]);
                const int topOffset = RandomRange(0, (std::max)(0, columnW - topW[i]));
                const int bottomOffset = RandomRange(0, (std::max)(0, columnW - bottomW[i]));
                rects.push_back({ x + topOffset, lane0Y, topW[i], topH[i], false });
                rects.push_back({ x + bottomOffset, lane1Y, bottomW[i], bottomH[i], false });
                x += columnW + RandomRange(4, 8);
            }

            if (vertical)
                RotateRectsRight(rects);

            ResizeMapForLayoutIfNeeded(map, GetRectLayoutSize(rects));

            if (AddPatternRooms(map, rects))
                return true;
        }

        return false;
    }

    std::vector<PatternRoomSpec> MakePattern(MapLayoutType type)
    {
        std::vector<PatternRoomSpec> specs;

        switch (type)
        {
        case MapLayoutType::CrossFive:
            specs = { {1, 0}, {0, 1}, {1, 1}, {2, 1}, {1, 2} };
            for (int i = 0; i < GameRandom::Range(0, 3); ++i)
                RotatePatternRight(specs);
            break;

        case MapLayoutType::SmallRoomChain:
            specs = { {0, 0}, {1, 0}, {2, 0}, {3, 0}, {4, 0} };
            if (GameRandom::Range(0, 1) == 0)
                RotatePatternRight(specs);
            break;

        case MapLayoutType::TreasureRoomChain:
        {
            const int variant = GameRandom::Range(0, 2);
            if (variant == 0)
                specs = { {0, 0}, {1, 0}, {2, 0}, {3, 0}, {4, 0, true} };
            else if (variant == 1)
                specs = { {0, 0}, {1, 0}, {2, 0}, {2, 1}, {2, 2, true} };
            else
                specs = { {0, 0}, {1, 0}, {1, 1}, {2, 1}, {3, 1, true} };
            for (int i = 0; i < GameRandom::Range(0, 3); ++i)
                RotatePatternRight(specs);
            if (GameRandom::Range(0, 1) == 0)
                MirrorPatternX(specs);
            break;
        }

        case MapLayoutType::TShapeFour:
            specs = { {0, 0}, {0, 1}, {1, 1}, {0, 2} };
            for (int i = 0; i < GameRandom::Range(0, 3); ++i)
                RotatePatternRight(specs);
            if (GameRandom::Range(0, 1) == 0)
                MirrorPatternX(specs);
            break;

        case MapLayoutType::SoilEight:
            specs = {
                {0, 0}, {1, 0}, {2, 0},
                {1, 1},
                {0, 2}, {1, 2}, {2, 2},
                {1, 3}
            };
            for (int i = 0; i < GameRandom::Range(0, 3); ++i)
                RotatePatternRight(specs);
            if (GameRandom::Range(0, 1) == 0)
                MirrorPatternX(specs);
            if (GameRandom::Range(0, 1) == 0)
                MirrorPatternY(specs);
            break;

        case MapLayoutType::LShape:
            specs = { {0, 0}, {0, 1}, {0, 2}, {1, 2}, {2, 2} };
            for (int i = 0; i < GameRandom::Range(0, 3); ++i)
                RotatePatternRight(specs);
            if (GameRandom::Range(0, 1) == 0)
                MirrorPatternX(specs);
            break;

        default:
            break;
        }

        NormalizePattern(specs);
        return specs;
    }

    bool TryGenerateLargeRoom(MapData* map)
    {
        if (!map || map->GetWidth() <= kRoomMargin * 2 + 5 || map->GetHeight() <= kRoomMargin * 2 + 5)
            return false;

        map->GetRooms().clear();
        map->SetAllTile(TileType::Wall);
        map->SetAllActive(false);

        // 大部屋はフロア全体を1つの部屋として扱うため、外周の壁だけ残して床を大きく広げる。
        const int maxRoomW = map->GetWidth() - kRoomMargin * 2;
        const int maxRoomH = map->GetHeight() - kRoomMargin * 2;
        const int roomW = ClampInt(maxRoomW - RandomRange(0, 4), 5, maxRoomW);
        const int roomH = ClampInt(maxRoomH - RandomRange(0, 4), 5, maxRoomH);
        const int maxStartX = map->GetWidth() - roomW - kRoomMargin;
        const int maxStartY = map->GetHeight() - roomH - kRoomMargin;
        const int x = (maxStartX >= kRoomMargin) ? RandomRange(kRoomMargin, maxStartX) : (map->GetWidth() - roomW) / 2;
        const int y = (maxStartY >= kRoomMargin) ? RandomRange(kRoomMargin, maxStartY) : (map->GetHeight() - roomH) / 2;

        map->SetActiveRect(x - 2, y - 2, x + roomW + 1, y + roomH + 1, true);

        Room room({ x, y }, { roomW, roomH });
        room.id = 0;
        map->AddRoom(room);
        map->ApplyRoom(map->GetRooms().back());
        return true;
    }
    bool TryGeneratePatternRooms(MapData* map, MapLayoutType type)
    {
        if (!map || type == MapLayoutType::Random) return false;
        if (type == MapLayoutType::LargeRoom)
            return TryGenerateLargeRoom(map);
        if (type == MapLayoutType::SmallRoomChain)
            return TryGenerateSmallRoomChainRooms(map);

        std::vector<PatternRoomSpec> specs = MakePattern(type);
        if (specs.empty()) return false;

        int maxX = 0;
        int maxY = 0;
        for (const PatternRoomSpec& spec : specs)
        {
            maxX = (std::max)(maxX, spec.x);
            maxY = (std::max)(maxY, spec.y);
        }

        const int cols = maxX + 1;
        const int rows = maxY + 1;
        const bool keepSameRoomSize = (type == MapLayoutType::TreasureRoomChain);

        std::vector<int> colGaps((std::max)(0, cols - 1), keepSameRoomSize ? kSmallLayoutGap : kLayoutGap);
        std::vector<int> rowGaps((std::max)(0, rows - 1), keepSameRoomSize ? kSmallLayoutGap : kLayoutGap);
        int totalGapW = 0;
        int totalGapH = 0;
        for (int& gap : colGaps)
        {
            gap = keepSameRoomSize ? kSmallLayoutGap : RandomRange(3, 8);
            totalGapW += gap;
        }
        for (int& gap : rowGaps)
        {
            gap = keepSameRoomSize ? kSmallLayoutGap : RandomRange(3, 8);
            totalGapH += gap;
        }

        const int availableW = map->GetWidth() - kRoomMargin * 2 - totalGapW;
        const int availableH = map->GetHeight() - kRoomMargin * 2 - totalGapH;
        if (availableW <= 0 || availableH <= 0) return false;

        std::vector<int> colWidths(cols, 5);
        std::vector<int> rowHeights(rows, 5);

        if (keepSameRoomSize)
        {
            const int side = ClampInt((std::min)(availableW / cols, availableH / rows), 5, 9);
            std::fill(colWidths.begin(), colWidths.end(), side);
            std::fill(rowHeights.begin(), rowHeights.end(), side);
        }
        else
        {
            const int maxColWidth = (std::max)(5, availableW / cols);
            const int maxRowHeight = (std::max)(5, availableH / rows);
            for (int& width : colWidths)
                width = RandomRange(5, (std::min)(12, maxColWidth));
            for (int& height : rowHeights)
                height = RandomRange(5, (std::min)(12, maxRowHeight));
        }

        std::vector<int> colStarts(cols, 0);
        std::vector<int> rowStarts(rows, 0);
        for (int i = 1; i < cols; ++i)
            colStarts[i] = colStarts[i - 1] + colWidths[i - 1] + colGaps[i - 1];
        for (int i = 1; i < rows; ++i)
            rowStarts[i] = rowStarts[i - 1] + rowHeights[i - 1] + rowGaps[i - 1];

        const int layoutW = colStarts.back() + colWidths.back();
        const int layoutH = rowStarts.back() + rowHeights.back();
        if (layoutW + kRoomMargin * 2 > map->GetWidth() || layoutH + kRoomMargin * 2 > map->GetHeight())
        {
            if (type != MapLayoutType::TreasureRoomChain)
                return false;

            const int newW = layoutW + kRoomMargin * 2 + RandomRange(4, 12);
            const int newH = layoutH + kRoomMargin * 2 + RandomRange(4, 12);
            map->Reset((std::max)(map->GetWidth(), newW), (std::max)(map->GetHeight(), newH));
        }

        const int maxStartX = map->GetWidth() - layoutW - kRoomMargin;
        const int maxStartY = map->GetHeight() - layoutH - kRoomMargin;
        const int startX = (maxStartX >= kRoomMargin) ? RandomRange(kRoomMargin, maxStartX) : (map->GetWidth() - layoutW) / 2;
        const int startY = (maxStartY >= kRoomMargin) ? RandomRange(kRoomMargin, maxStartY) : (map->GetHeight() - layoutH) / 2;

        map->GetRooms().clear();
        map->SetAllTile(TileType::Wall);
        map->SetAllActive(false);

        for (const PatternRoomSpec& spec : specs)
        {
            const int cellW = colWidths[spec.x];
            const int cellH = rowHeights[spec.y];
            const int roomW = keepSameRoomSize ? cellW : RandomRange(5, cellW);
            const int roomH = keepSameRoomSize ? cellH : RandomRange(5, cellH);
            const int x = startX + colStarts[spec.x] + (keepSameRoomSize ? 0 : RandomRange(0, cellW - roomW));
            const int y = startY + rowStarts[spec.y] + (keepSameRoomSize ? 0 : RandomRange(0, cellH - roomH));
            map->SetActiveRect(x - 2, y - 2, x + roomW + 1, y + roomH + 1, true);

            Room room({ x, y }, { roomW, roomH });
            room.id = (int)map->GetRooms().size();
            if (spec.treasure)
                room.specialType = RoomSpecialType::Treasure;

            map->AddRoom(room);
            map->ApplyRoom(map->GetRooms().back());
        }

        return true;
    }

    void GenerateFixedRooms(MapData* map, const FloorData& floor, Scene* scene)
    {
        if (!map || !floor.useFixedRoom) return;

        std::vector<const FixedRoomSetting*> candidates;
        for (const FixedRoomSetting& setting : floor.fixedRoomPaths)
        {
            if (GameRandom::Percent(setting.appearanceRate))
                candidates.push_back(&setting);
        }
        if (candidates.empty()) return;

        std::vector<const FixedRoomSetting*> targets;
        if (floor.spawnAllFixedRooms)
        {
            targets = candidates;
        }
        else
        {
            targets.push_back(candidates[GameRandom::Index(candidates.size())]);
        }

        for (const FixedRoomSetting* setting : targets)
        {
            if ((int)map->GetRooms().size() >= floor.maxRoomCount) break;

            auto fixedData = MapLoader::LoadDataOnly(setting->path);
            if (!fixedData) continue;

            const int fw = fixedData->GetWidth();
            const int fh = fixedData->GetHeight();
            if (map->GetWidth() <= fw + kRoomMargin * 2 || map->GetHeight() <= fh + kRoomMargin * 2)
                continue;

            for (int attempts = 0; attempts < kMaxRoomPlaceAttempts; ++attempts)
            {
                const int rx = GameRandom::Range(kRoomMargin, map->GetWidth() - fw - kRoomMargin - 1);
                const int ry = GameRandom::Range(kRoomMargin, map->GetHeight() - fh - kRoomMargin - 1);
                Room newFixedRoom(Vector2Int(rx, ry), Vector2Int(fw, fh));

                if (OverlapsAnyRoom(map, newFixedRoom)) continue;

                MapLoader::InsertFixedRoom(map, setting->path, Vector2Int(rx, ry), scene);
                if (!BuildFixedRoomFromTiles(*fixedData, Vector2Int(rx, ry), newFixedRoom))
                    continue;

                newFixedRoom.id = (int)map->GetRooms().size();
                map->AddRoom(newFixedRoom);
                break;
            }
        }
    }

    bool TryGenerateRandomRoom(MapData* map)
    {
        if (!map || map->GetWidth() <= kRoomMargin * 2 + 5 || map->GetHeight() <= kRoomMargin * 2 + 5)
            return false;

        const int maxAllowedW = (std::max)(5, map->GetWidth() - kRoomMargin * 2 - 1);
        const int maxAllowedH = (std::max)(5, map->GetHeight() - kRoomMargin * 2 - 1);

        for (int attempts = 0; attempts < kMaxRoomPlaceAttempts; ++attempts)
        {
            int w = 6;
            int h = 6;
            const int shapeRoll = GameRandom::Range(0, 99);
            if (shapeRoll < 20)
            {
                w = RandomRange(5, 7);
                h = RandomRange(5, 7);
            }
            else if (shapeRoll < 45)
            {
                if (GameRandom::Range(0, 1) == 0)
                {
                    w = RandomRange(5, 8);
                    h = RandomRange(9, 16);
                }
                else
                {
                    w = RandomRange(9, 16);
                    h = RandomRange(5, 8);
                }
            }
            else if (shapeRoll < 65)
            {
                w = RandomRange(10, 16);
                h = RandomRange(10, 16);
            }
            else
            {
                w = RandomRange(6, 13);
                h = RandomRange(6, 13);
            }

            w = ClampInt(w, 5, maxAllowedW);
            h = ClampInt(h, 5, maxAllowedH);
            if (map->GetWidth() <= w + kRoomMargin * 2 || map->GetHeight() <= h + kRoomMargin * 2)
                continue;

            const int rx = GameRandom::Range(kRoomMargin, map->GetWidth() - w - kRoomMargin - 1);
            const int ry = GameRandom::Range(kRoomMargin, map->GetHeight() - h - kRoomMargin - 1);
            Room newRoom(Vector2Int(rx, ry), Vector2Int(w, h));

            if (OverlapsAnyRoom(map, newRoom)) continue;

            const int roomType = GameRandom::Range(0, 1);
            const int maxCornerShave = ((std::min)(w, h) <= 5) ? 1 : 3;
            const int shaveTL = RandomRange(0, maxCornerShave);
            const int shaveTR = RandomRange(0, maxCornerShave);
            const int shaveBL = RandomRange(0, maxCornerShave);
            const int shaveBR = RandomRange(0, maxCornerShave);

            for (int yy = ry; yy < ry + h; ++yy)
            {
                for (int xx = rx; xx < rx + w; ++xx)
                {
                    if (roomType == 1)
                    {
                        const int distL = xx - rx;
                        const int distR = (rx + w - 1) - xx;
                        const int distT = yy - ry;
                        const int distB = (ry + h - 1) - yy;

                        bool isShaved = false;
                        if (distL + distT < shaveTL) isShaved = true;
                        else if (distR + distT < shaveTR) isShaved = true;
                        else if (distL + distB < shaveBL) isShaved = true;
                        else if (distR + distB < shaveBR) isShaved = true;

                        if (isShaved) continue;
                    }
                    map->SetTile(xx, yy, TileType::Floor);
                }
            }

            newRoom.id = (int)map->GetRooms().size();
            map->AddRoom(newRoom);
            RebuildRoomSubRects(map);
            return true;
        }

        return false;
    }
}

MapLayoutType RoomGenerator::GenerateRooms(MapData* map, const FloorData& floor, Scene* scene)
{
    if (!map) return MapLayoutType::Random;

    const MapLayoutType layoutType = PickLayoutType(floor);
    if (layoutType != MapLayoutType::Random && TryGeneratePatternRooms(map, layoutType))
        return layoutType;

    GenerateFixedRooms(map, floor, scene);

    const int roomRange = (std::max)(0, floor.maxRoomCount - floor.minRoomCount);
    const int targetTotalRooms = floor.minRoomCount + (roomRange > 0 ? GameRandom::Range(0, roomRange) : 0);

    while ((int)map->GetRooms().size() < targetTotalRooms)
    {
        if (!TryGenerateRandomRoom(map))
            break;
    }

    return MapLayoutType::Random;
}

void RoomGenerator::ScanRoomEntrances(MapData* map)
{
    if (!map) return;

    for (Room& room : map->GetRooms())
    {
        const Vector2Int pos = room.GetPosition();
        const Vector2Int size = room.GetSize();

        room.entrances.clear();

        for (int y = pos.y - 1; y <= pos.y + size.y; ++y)
        {
            for (int x = pos.x - 1; x <= pos.x + size.x; ++x)
            {
                if (!map->IsInBounds(x, y)) continue;

                if (map->IsEntranceTile(x, y))
                    room.entrances.push_back({ x, y });
            }
        }
    }
}
