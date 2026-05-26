// SpawnTableIO.h
#pragma once
#include <string>
#include <exception>
#include "external/json.hpp"
#include "JsonIO.h"

class SpawnTableIO {
public:
    // 構造が共通（tableId と entries {id, weight}）なテーブルを汎用的に保存
    template<typename T>
    static bool Save(const std::string& path, const T& table) {
        nlohmann::json root;
        nlohmann::json arr = nlohmann::json::array();
        for (const auto& e : table.entries) {
            arr.push_back({ {"id", e.id}, {"weight", e.weight} });
        }
        root["tableId"] = table.tableId;
        root["entries"] = arr;
        return JsonIO::SaveJson(path, root);
    }

    template<typename T>
    static bool Load(const std::string& path, T& outTable) {
        nlohmann::json root;
        if (!JsonIO::LoadJson(path, root)) return false;

        try {
            if (!root.is_object()) return false;
            if (!root.contains("tableId") || !root["tableId"].is_string()) return false;
            if (!root.contains("entries") || !root["entries"].is_array()) return false;

            outTable.tableId = root["tableId"].get<std::string>();
            if (outTable.tableId.empty()) return false;

            outTable.entries.clear();
            for (const auto& e : root["entries"]) {
                if (!e.is_object()) continue;
                if (!e.contains("id") || !e["id"].is_string()) continue;
                if (!e.contains("weight") || !e["weight"].is_number_integer()) continue;

                const std::string id = e["id"].get<std::string>();
                const int weight = e["weight"].get<int>();
                if (id.empty() || weight < 0) continue;

                outTable.entries.push_back({ id, weight });
            }
        }
        catch (const std::exception&) {
            return false;
        }
        return true;
    }
};