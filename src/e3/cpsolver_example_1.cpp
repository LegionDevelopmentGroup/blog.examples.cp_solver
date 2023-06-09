
#include <vector>
#include <string>
#include <cstdint>
#include <iostream>
#include <algorithm>
#include <fstream>

#include "ortools/sat/cp_model.h"
#include "ortools/sat/model.h"
#include "ortools/sat/sat_parameters.pb.h"
#include "ortools/util/time_limit.h"
#include "ortools/sat/cp_model_solver.h"
#include "ortools/port/proto_utils.h"


#include "json.hpp"

using Amount = std::uint64_t;

using std::string;
using std::vector;
using std::cout;
using std::endl;
using std::pair;

using operations_research::Domain;
using operations_research::TimeLimit;
using operations_research::sat::CpModelBuilder;
using operations_research::sat::CpSolverResponse;
using operations_research::sat::CpSolverStatus;
using operations_research::sat::IntVar;
using operations_research::sat::LinearExpr;
using operations_research::sat::Model;
using operations_research::sat::SatParameters;
using operations_research::sat::SolutionIntegerValue;
using operations_research::sat::NewFeasibleSolutionObserver;
using operations_research::ProtoEnumToString;

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

Group find_grouping(const vector<Item> & items, const Parameters & params) {

    CpModelBuilder model_builder;
    
    Group selected;

    const int64 scaling_factor = 1000;

    // Define variables
    vector<IntVar> within_pool(items.size());
    vector<int64> value_scaled(items.size());
    vector<int64> weight_scaled(items.size());
    vector<int64> volume_scaled(items.size());

    const Domain domain_from_zero(0, 1);
    // const Domain domain_from_one(1, 1);
    for(std::size_t i = 0; i < items.size(); ++i)
    {
        within_pool[i] = model_builder.NewIntVar(domain_from_zero);
        value_scaled[i] = static_cast<int64>(items[i].value) * scaling_factor;
        weight_scaled[i] = static_cast<int64>(items[i].weight) * scaling_factor;
        volume_scaled[i] = static_cast<int64>(items[i].volume) * scaling_factor;
    }

    // Define constraints

    // 1. SUM(v_i x u_i.weight) <= w_max
    model_builder.AddLessOrEqual(LinearExpr::WeightedSum(within_pool, weight_scaled), static_cast<int64>(params.max_weight) * scaling_factor);

    // 2. SUM(v_i x u_i.volume) <= v_max
    model_builder.AddLessOrEqual(LinearExpr::WeightedSum(within_pool, volume_scaled), static_cast<int64>(params.max_volume) * scaling_factor);

    // MAX(SUM(v_i x u_i.value))
    model_builder.Maximize(LinearExpr::WeightedSum(within_pool, value_scaled));

    // TODO add remaining constraints.

    // Run model
    Model model;

    SatParameters parameters;
    parameters.set_num_search_workers(4);

    int max_time = 3 * 60;
    parameters.set_max_time_in_seconds(max_time);
    model.Add(NewSatParameters(parameters));

    const CpSolverResponse response = SolveCpModel(model_builder.Build(), &model);

    cout << "Resp Status: " << ProtoEnumToString<CpSolverStatus>(response.status()) << endl;

    if (
        response.status() != CpSolverStatus::OPTIMAL &&
        response.status() != CpSolverStatus::FEASIBLE)
    {
        // no solution
        return {};
    }

    for (size_t i = 0; i < items.size(); ++i)
    {
        if (SolutionIntegerValue(response, within_pool[i]) >= 1)
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

constexpr pair<Parameters, bool> check_valid(const Group & selected, const Parameters & params)
{
    auto total_weight = sum_weight(selected);
    auto total_volume = sum_volume(selected);
    return {{total_weight, total_volume}, total_weight > params.max_weight || total_volume > params.max_volume};
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
    vector<Item> items = { {10, 10, 10, "a", "p1"}, {3, 4, 9, "b", "p2"}, {3, 5, 2, "c", "p1"}, {2, 4, 4, "c", "p1"} };

    Parameters params = {20, 20, 10, 0.8, 0.025, 0.025};

    parse_args(argc, argv, items, params);

    auto chosen = find_grouping(items, params);

    print_results(chosen, params);

    return 0;
}