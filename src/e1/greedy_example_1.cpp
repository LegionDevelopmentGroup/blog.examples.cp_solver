
#include <vector>
#include <string>
#include <cstdint>
#include <iostream>
#include <algorithm>
#include <compare>
#include <fstream>

#include <numeric>

#include "json.hpp"

using Amount = std::uint64_t;

using std::string;
using std::vector;
using std::cout;
using std::endl;
using std::sort;
using std::pair;
using std::accumulate;

struct Item {
  Amount value;
  Amount weight;
  Amount volume;
  string manufacturer;
  string type;
};

using Group = vector<Item>;

struct Parameters {
  Amount max_weight;
  Amount max_volume;
};

constexpr Amount sum_value(const Group & selected) {
  return accumulate(selected.cbegin(), selected.cend(), static_cast<Amount>(0),
    [](const Amount & a, const Item & b) {
      return a + b.value;
    }
  );
}

constexpr Amount sum_weight(const Group & selected) {
  return accumulate(selected.cbegin(), selected.cend(), static_cast<Amount>(0),
    [](const Amount & a, const Item & b) {
      return a + b.weight;
    }
  );
}

constexpr bool above_weight(const Item & item, const Group & selected, const Amount & max_weight)
{
  return sum_weight(selected) + item.weight > max_weight;
}

constexpr Amount sum_volume(const Group & selected) {
   return accumulate(selected.cbegin(), selected.cend(), static_cast<Amount>(0),
    [](const Amount & a, const Item & b) {
      return a + b.volume;
    }
  );
}

constexpr bool above_volume(const Item & item, const Group & selected, const Amount & max_volume)
{
  return sum_volume(selected) + item.volume > max_volume;
}

Group find_grouping(const vector<Item> & items, const Parameters & params) {
  
  Group selected;
  for(auto item : items)
  {
    if (above_weight(item, selected, params.max_weight) ||
        above_volume(item, selected, params.max_volume))
    {
      continue;
    }

    selected.push_back(item);
  }
  return selected;
}

template<typename V>
void to_json(nlohmann::json & j, const vector<V> & p)
{
  for (auto & item: p)
  {
  nlohmann::json inner;
  to_json(inner, item);
  j.push_back(inner);
  }
}

void to_json(nlohmann::json & j, const Item & i)
{
  j["value"] = i.value;
  j["weight"] = i.weight;
  j["volume"] = i.volume;
}

template<typename V>
void from_json(const nlohmann::json& j, vector<V> & p)
{
  
  for (auto & inner: j)
  {
    V item;
    from_json(inner, item);
    p.push_back(item);
  }
}

void from_json(const nlohmann::json& j, Item& p) {
    j.at("value").get_to(p.value);
    j.at("weight").get_to(p.weight);
    j.at("volume").get_to(p.volume);
    j.at("product_type").get_to(p.type);
    j.at("manufacturer").get_to(p.manufacturer);
}

void from_json(const nlohmann::json& j, Parameters& p) {
    j.at("max_weight").get_to(p.max_weight);
    j.at("max_volume").get_to(p.max_volume);
}

template<typename V>
V read_json(const std::string & file_path)
{
    std::ifstream i(file_path);
    nlohmann::json j;
    i >> j;

    V data;
    from_json(j, data);
    return data;
}

constexpr pair<Parameters, bool> check_valid(const Group & selected, const Parameters & params)
{
  auto total_weight = sum_weight(selected);
  auto total_volume = sum_volume(selected);
  return {{total_weight, total_volume}, total_weight > params.max_weight || total_volume > params.max_volume};
}

// Sort by value, weight, and volume
constexpr bool sort_by_filter(const Item & a, const Item & b) {
  auto res = a.value <=> b.value;
  if (res < 0) {
    return false;
  } else if (res > 0) {
    return true;
  }

  res = a.weight <=> b.weight;
  if (res < 0) {
    return false;
  } else if (res > 0) {
    return true;
  }

  res = a.volume <=> b.volume;
  if (res < 0) {
    return false;
  } else if (res > 0) {
    return true;
  }
  return false;
}

void print_results(const Group & chosen, const Parameters & params)
{
  auto [val_params, invalid] = check_valid(chosen, params);

  if (invalid) {
    cout << "Invalid";
  } else {
    cout << "Valid";
  }

  auto total_value = sum_value(chosen);

  cout << " Parameters" << endl;
  cout << "Value: " << total_value << endl;
  cout << "Weight: " << val_params.max_weight << endl;
  cout << "Volume: " << val_params.max_volume << endl;
  cout << endl;
  cout << "Chosen:" << endl;

  nlohmann::json data;
  to_json(data, chosen);

  cout << data.dump(4) << endl;
}

void parse_args(int argc, char * argv[], vector<Item> & items, Parameters & params) {
    if (argc == 3) {
        const char * cur_val = *(argv+1);
        string itemsPath(cur_val, std::char_traits<char>::length(cur_val));
        items = read_json<vector<Item>>(itemsPath);

        const char * cur_val2 = *(argv+2);
        string paramPath(cur_val2, std::char_traits<char>::length(cur_val2));
        params = read_json<Parameters>(paramPath);
    }
}

// Should output:
// Valid Parameters
// Weight: 19
// Volume: 0
//
// Chosen:
// [
//     {
//         "value": 10,
//         "volume": 0,
//         "weight": 10
//     },
//     {
//         "value": 10,
//         "volume": 0,
//         "weight": 9
//     }
// ]
int main(int argc, char * argv[]) {
  vector<Item> items = { {10, 10, 0, "a", "p1"}, {3, 2, 0, "b", "p2"}, {10, 9, 0, "c", "p1"} };

  Parameters params = {20, 20};

  parse_args(argc, argv, items, params);

  sort(items.begin(), items.end(), sort_by_filter);

  auto chosen = find_grouping(items, params);

  print_results(chosen, params);

  return 0;
}