
#include <vector>
#include <string>
#include <cstdint>
#include <iostream>
#include <algorithm>
#include <unordered_map>
#include <set>
#include <fstream>
#include <memory>

#include "ortools/base/logging.h"
#include "ortools/linear_solver/linear_solver.h"

#include "json.hpp"

using Amount = std::uint64_t;

using std::string;
using std::vector;
using std::cout;
using std::endl;
using std::set;
using std::pair;
using std::unordered_map;
using std::size_t;

using operations_research::MPSolver;
using operations_research::MPVariable;
using operations_research::LinearExpr;
using operations_research::MPObjective;

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

constexpr Amount sum_volume(const Group & selected) {
   return accumulate(selected.cbegin(), selected.cend(), static_cast<Amount>(0),
        [](const Amount & a, const Item & b) {
            return a + b.volume;
        }
    );
}

using int64 = int64_t;

enum class FourthConstraintMode {
    force_max = 0,
    max_all = 2 };

Group find_grouping(const vector<Item> & items, const Parameters & params) {

    std::unique_ptr<MPSolver> solver(MPSolver::CreateSolver("SCIP"));
    if (!solver) {
        cout << "SCIP solver unavailable" << endl;
        return {};
    }
    
    Group selected;

    // Define variables
    vector<const MPVariable*> within_pool(items.size());

    set<string> product_types;  
    set<string> manufacturer_types;

    auto max_value = std::max_element(items.cbegin(), items.cend(), [](auto a, auto b) {
        return a.value < b.value;
    });

    auto max_index = static_cast<size_t>(std::distance(items.begin(), max_value));

    FourthConstraintMode constraint_four_setting = FourthConstraintMode::force_max;

    LinearExpr in_pool_weight_sum;
    LinearExpr in_pool_volume_sum;
    LinearExpr in_pool_value_sum;

    for(size_t i = 0; i < items.size(); ++i)
    {
        if (constraint_four_setting == FourthConstraintMode::force_max && i == max_index) {
            within_pool[i] = solver->MakeIntVar(1, 1, "");
        } else {
            within_pool[i] = solver->MakeIntVar(0, 1, "");
        }

        in_pool_weight_sum += LinearExpr(within_pool[i]) * static_cast<double>(items[i].weight);
        in_pool_volume_sum += LinearExpr(within_pool[i]) * static_cast<double>(items[i].volume);
        in_pool_value_sum += LinearExpr(within_pool[i]) * static_cast<double>(items[i].value);

        product_types.insert(items[i].type);
        manufacturer_types.insert(items[i].manufacturer);
    }

    // Define constraints

    // 1. SUM(v_i x u_i.weight) <= w_max
    solver->MakeRowConstraint(in_pool_weight_sum <= static_cast<double>(params.max_weight)) ;

    // 2. SUM(v_i x u_i.volume) <= v_max
    solver->MakeRowConstraint(in_pool_volume_sum <= static_cast<double>(params.max_volume)) ;

    // MAX(SUM(v_i x u_i.value))
    MPObjective* const objective = solver->MutableObjective();
    objective->MaximizeLinearExpr(in_pool_value_sum);

    // 3. SUM(v_i x u_i.value) > v_min
    // LinearRange doesn't support '>'
    solver->MakeRowConstraint(in_pool_value_sum >= static_cast<double>(params.min_value));

    // 4. MAX(v_i x u_i.value) / SUM(p_i.value x c_i) < high_value_max
    // Rewritten as: MAX(v_i x u_i.value / high_value_max) < SUM(v_i x u_i.value)
    // MPSolver interface does not support MaxEquality of a set of variables therefore force max is used instead
    switch(constraint_four_setting) {
        case FourthConstraintMode::force_max:
        {
            solver->MakeRowConstraint((LinearExpr(within_pool[max_index]) * static_cast<double>(max_value->value) / params.high_value_max) <= in_pool_value_sum);
            break;
        }
        case FourthConstraintMode::max_all:
        {
            for(size_t i = 0; i < items.size(); ++i)
            {
                solver->MakeRowConstraint((LinearExpr(within_pool[i]) * static_cast<double>(items[i].value) / params.high_value_max) <= in_pool_value_sum);
            }
            break;
        }
    }

    // 5. SUM(v_i x u_i.value if p_i.product_type == type) / SUM(v_i x u_i.value) <= type_value_max
    // Rewritten as: SUM(v_i x u_i.value / type_value_max if p_i.product_type == type) <= SUM(v_i x u_i.value)
    for (auto prod_type : product_types)
    {
        LinearExpr prod_type_sum;

        for(size_t i = 0; i < items.size(); ++i)
        {
            if (items[i].type != prod_type)
            {
                continue;
            }

            prod_type_sum += LinearExpr(within_pool[i]) * (static_cast<double>(items[i].value) / params.high_type_max);
        }
        solver->MakeRowConstraint(prod_type_sum <= in_pool_value_sum);
    }

    // 6. SUM(p_i.value x c_i if p_i.manufacturer == manufacturer) / SUM(p_i.value x c_i) <= man_value_max
    // Rewritten as: SUM(p_i.value / man_value_max x c_i if p_i.manufacturer == manufacturer) <= SUM(p_i.value x c_i)
    for (auto manufacturer_type : manufacturer_types)
    {
        LinearExpr man_type_sum;

        for(size_t i = 0; i < items.size(); ++i)
        {
            if (items[i].manufacturer != manufacturer_type)
            {
                continue;
            }

            man_type_sum += LinearExpr(within_pool[i]) * (static_cast<double>(items[i].value) / params.high_man_max);
        }

        solver->MakeRowConstraint(man_type_sum <= in_pool_value_sum);
    }

    auto status = solver->SetNumThreads(4);
    if (status == absl::OkStatus())
    {
        std::cout << "Valid threads" << std::endl;
    } else {
        std::cout << "Invalid threads" << std::endl;
    }

    int max_time = 3 * 60;

    solver->set_time_limit(max_time * 1000);

    const MPSolver::ResultStatus result_status = solver->Solve();

    cout << "Resp Status: " << result_status << endl;

    if (result_status != MPSolver::OPTIMAL &&
        result_status != MPSolver::FEASIBLE) {
        // not solution
        return {};
    }

    for (size_t i = 0; i < items.size(); ++i)
    {
        if (within_pool[i]->solution_value() >= 1)
        {
            selected.push_back(items[i]);
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

    cout << "Max Percent of total: " << val_params.high_value_max << endl;
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
// Valid threads
// Resp Status: MPSOLVER_OPTIMAL
// Valid Parameters
// Value: 18
// Weight: 18
// Volume: 15
// Max Percent of total: 0.5
// Man Types: 0.5
// Prod Types: 0.666667
//
// Chosen:
// [
//     {
//         "manufacturer": "a",
//         "type": "p1",
//         "value": 9,
//         "volume": 10,
//         "weight": 10
//     },
//     {
//         "manufacturer": "c",
//         "type": "p1",
//         "value": 3,
//         "volume": 2,
//         "weight": 5
//     },
//     {
//         "manufacturer": "c",
//         "type": "p2",
//         "value": 3,
//         "volume": 2,
//         "weight": 2
//     },
//     {
//         "manufacturer": "c",
//         "type": "p2",
//         "value": 3,
//         "volume": 1,
//         "weight": 1
//     }
// ]
int main(int argc, char * argv[]) {
    vector<Item> items = { {9, 10, 10, "a", "p1"}, {3, 4, 9, "b", "p2"}, {3, 5, 2, "c", "p1"}, {6, 4, 4, "c", "p1"}, {3, 2, 2, "c", "p2"}, {3, 1, 1, "c", "p2"} };

    Parameters params = {20, 20, 10, 0.8, 0.7, 0.7};

    parse_args(argc, argv, items, params);

    auto chosen = find_grouping(items, params);

    print_results(chosen, params);

    return 0;
}