#pragma once
#include <unordered_map>
#include <set>
#include <shared_mutex>

inline std::unordered_map<std::string, std::set<long>> room_registry;
inline std::unordered_map<long, int> client_registry;

inline std::shared_mutex room_lock;
inline std::shared_mutex client_lock;