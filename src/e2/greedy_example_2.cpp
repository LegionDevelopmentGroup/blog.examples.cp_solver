
#include <vector>
#include <string>
#include <cstdint>
#include <iostream>
#include <algorithm>
#include <compare>
#include <unordered_map>
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
using std::unordered_map;

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
  Amount min_value;
  double high_value_max;
  double high_man_max;
  double high_type_max;
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

  std::unordered_map<string, Amount> product_manufacturer_map;
  std::unordered_map<string, Amount> product_type_map;
  Amount total = 0;

  for(auto item : items)
  {
    if (above_weight(item, selected, params.max_weight) ||
      above_volume(item, selected, params.max_volume))
    {
      continue;
    }

    Amount man_total = 0;
    auto product_manufacturer_value_it = product_manufacturer_map.find(item.manufacturer);
    if (product_manufacturer_value_it != product_manufacturer_map.end()) {
      man_total = product_manufacturer_value_it->second;
    }

    Amount type_total = 0;
    auto product_type_value_it = product_type_map.find(item.type);
    if (product_type_value_it != product_type_map.end()) {
      type_total = product_type_value_it->second;
    }

    selected.push_back(item);
    total += item.value;
    product_manufacturer_map[item.manufacturer] = man_total;
    product_type_map[item.type] = type_total;
  }

  // auto total_value = sum_value(selected);
  if(total < params.min_value)
  {
    return {};
  }

  // This only helps exclude invalid solutions but we may be prevented from getting valid solutions
  // e.g. if excluding the max item could yield a valid solution
  auto max_value = std::max_element(selected.cbegin(), selected.cend(), [](auto a, auto b) {
    return a.value < b.value;
  });
  if(max_value->value > static_cast<Amount>(params.high_value_max * static_cast<double>(total))) {
    return {};
  }

  // This could miss valid solutions. As we only check afterwards. It doesn't influence our decision.
  for(auto [key, man_total] : product_manufacturer_map)
  {
    if (man_total > static_cast<Amount>(params.high_man_max * static_cast<double>(total))) {
      return {};
    }
  }

  // This could miss valid solutions. As we only check afterwards. It doesn't influence our decision.
  for(auto [key, type_total] : product_type_map)
  {
    if (type_total > static_cast<Amount>(params.high_type_max * static_cast<double>(total))) {
      return {};
    }
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
  j["type"] = i.type;
  j["manufacturer"] = i.manufacturer;
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
    j.at("min_value").get_to(p.min_value);
    j.at("high_value_max").get_to(p.high_value_max);
    j.at("high_man_max").get_to(p.high_man_max);
    j.at("high_type_max").get_to(p.high_type_max);
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

pair<Parameters, bool> check_valid(const Group & selected, const Parameters & params)
{
  if (selected.empty()) {
    return {{}, true};
  }
  auto total_weight = sum_weight(selected);
  auto total_volume = sum_volume(selected);

  auto total_value = sum_value(selected);

  auto max_value = std::max_element(selected.cbegin(), selected.cend(), [](auto a, auto b) {
    return a.value < b.value;
  });

  unordered_map<string, Amount> prod_types;
  unordered_map<string, Amount> man_types;

  for(auto item : selected) {
    auto prod_type = prod_types.find(item.type);
    if(prod_type != prod_types.end())
    {
      prod_type->second += item.value;
    }
    else
    {
      prod_types[item.type] = item.value;
    }

    auto man_type = man_types.find(item.manufacturer);
    if(man_type != man_types.end())
    {
      man_type->second += item.value;
    }
    else
    {
      man_types[item.manufacturer] = item.value;
    }        
  }

  double max_prod_type = 0.0;
  bool invalid_prods = false;
  for (auto [ptype, amount] : prod_types)
  {
    auto prod_type = static_cast<double>(amount) / static_cast<double>(total_value);
    if (prod_type > max_prod_type)
    {
      max_prod_type = prod_type;
    }
    invalid_prods = invalid_prods || prod_type > params.high_type_max;
  }

  double max_man_type = 0.0;
  bool invalid_mans = false;
  for (auto [mtype, amount] : man_types)
  {
    auto man_type = static_cast<double>(amount) / static_cast<double>(total_value);
    if (man_type > max_man_type)
    {
      max_man_type = man_type;
    }
    invalid_mans = invalid_mans || man_type > params.high_man_max;
  }

  auto max_value_percent = static_cast<double>(max_value->value) / static_cast<double>(total_value);

  return {{total_weight, total_volume, total_value, max_value_percent, max_man_type, max_prod_type},
    total_weight > params.max_weight || total_volume > params.max_volume ||
    total_value < params.min_value || max_value_percent > params.high_value_max ||
    invalid_prods || invalid_mans };
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
  
  cout << " Parameters" << endl;
  cout << "Value: " << val_params.min_value << endl;
  cout << "Weight: " << val_params.max_weight << endl;
  cout << "Volume: " << val_params.max_volume << endl;

  cout << "Man Types: " << val_params.high_man_max << endl;
  cout << "Prod Types: " << val_params.high_type_max << endl;

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
// Value: 15
// Weight: 19
// Volume: 16
// 
// Chosen:
// [
//     {
//         "value": 10,
//         "volume": 10,
//         "weight": 10
//     },
//     {
//         "value": 3,
//         "volume": 2,
//         "weight": 5
//     },
//     {
//         "value": 2,
//         "volume": 4,
//         "weight": 4
//     }
// ]
int main(int argc, char * argv[]) {
  vector<Item> items = { {10, 10, 10, "a", "p1"}, {3, 4, 9, "b", "p2"}, {3, 5, 2, "c", "p4"}, {2, 4, 4, "c", "p3"} };

  Parameters params = {20, 20, 10, 0.8, 0.7, 0.7};

  parse_args(argc, argv, items, params);

  sort(items.begin(), items.end(), sort_by_filter);

  auto chosen = find_grouping(items, params);

  print_results(chosen, params);

  return 0;
}