#ifndef PAGMO_PARETO_HPP
#define PAGMO_PARETO_HPP

/** \file pareto.hpp
 * \brief Pareto.
 *
 * This header contains utilities used to compute non dominated fronts and other
 * quantities relevant for multi objective optimization
 */

#include <algorithm>
#include <stdexcept>
#include <limits>
#include <numeric>
#include <string>
#include <tuple>

#include "../types.hpp"
#include "../exceptions.hpp"
#include "../io.hpp"


namespace pagmo{

/// Pareto-dominance
/**
 * Return true if obj1 Pareto dominates obj2, false otherwise. Minimization
 * is assumed.
 *
 * Each pair of corresponding elements in obj1 and obj2 is compared: if all
 * elements in obj1 are less or equal to the corresponding element in obj2, 
 * but at least one is different, true will be returned. Otherwise, false will be returned.
 *
 * @param[in] obj1 first vector of objectives.
 * @param[in] obj2 second vector of objectives.
 *
 * @return true if obj1 is dominating obj2, false otherwise.
 *
 * @throws std::invalid_argument if the dimensions of the two objectives is different
 */
bool pareto_dominance(const vector_double &obj1, const vector_double &obj2) 
{
    if (obj1.size() != obj2.size()) {
        pagmo_throw(std::invalid_argument,
            "Fitness of different dimensions: " + std::to_string(obj1.size()) + 
            " and " + std::to_string(obj2.size()) +
         ": cannot define dominance");
    }
    vector_double::size_type count1 = 0u; 
    vector_double::size_type count2 = 0u;
    for (decltype(obj1.size()) i = 0u; i < obj1.size(); ++i) {
        if (obj1[i] < obj2[i]) {
            ++count1;
        }
        if (obj1[i] == obj2[i]) {
            ++count2;
        }
    }
    return ( ( (count1+count2) == obj1.size()) && (count1 > 0u) );
}

/// Return value for the fast_non_dominated_sorting algorithm
using fnds_return_value = std::tuple<std::vector<std::vector<vector_double::size_type>>,std::vector<std::vector<vector_double::size_type>>,std::vector<vector_double::size_type>,std::vector<vector_double::size_type>>;

/// Fast non dominated sorting
/**
 * An implementation of the fast non dominated sorting algorithm. Complexity is \f$ O(MN^2)\f$ where \f$M\f$ is the number of objectives
 * and \f$N\f$ is the number of individuals.
 *
 * @see Deb, Kalyanmoy, et al. "A fast elitist non-dominated sorting genetic algorithm
 * for multi-objective optimization: NSGA-II." Parallel problem solving from nature PPSN VI. Springer Berlin Heidelberg, 2000.
 *
 * @param[in] obj_list An std::vector containing the objectives of different individuals. Example {{1,2,3},{-2,3,7},{-1,-2,-3},{0,0,0}}
 *
 * @return an std::tuple containing:
 *  - the non dominated fronts, an <tt>std::vector<std::vector<size_type>></tt> containing the non dominated fronts. Example {{1,2},{3},{0}}
 *  - the domination list, an <tt>std::vector<std::vector<size_type>></tt> containing the domination list, i.e. the indexes of all individuals
 * dominated by the individual at position \f$i\f$. Example {{},{},{0,3},{0}}
 *  - the domination count, an <tt>std::vector<size_type></tt> containing the number of individuals
 * that dominate the individual at position \f$i\f$. Example {2, 0, 0, 1}
 *  - the non domination rank, an <tt>std::vector<size_type></tt> containing the index of the non dominated
 * front to which the individual at position \f$i\f$ belongs. Example {2,0,0,1}
 *
 * @throws std::invalid_argument If the size of \p obj_list is not at least 2
 * @throws unspecified all exceptions thrown by pagmo::pareto_dominance
 */
fnds_return_value fast_non_dominated_sorting (const std::vector<vector_double> &obj_list)
    {
        auto N = obj_list.size();
        // We make sure to have two points at least (one could also be allowed)
        if (N < 2) {
            pagmo_throw(std::invalid_argument, "At least two points are needed for fast_non_dominated_sorting: " + std::to_string(N) + " detected.");
        }
        // Initialize the return values
        std::vector<std::vector<vector_double::size_type>> non_dom_fronts(1u);
        std::vector<std::vector<vector_double::size_type>> dom_list(N);
        std::vector<vector_double::size_type> dom_count(N);
        std::vector<vector_double::size_type> non_dom_rank(N);

        // Start the fast non dominated sort algorithm
        for (decltype(N) i = 0u; i < N; ++i) {
            dom_list[i].clear();
            dom_count[i]=0u;
            for (decltype(N) j = 0u; j < N; ++j) {
                if (i==j) {
                    continue;
                }
                if (pareto_dominance(obj_list[i], obj_list[j])) {
                    dom_list[i].push_back(j);
                } else if (pareto_dominance(obj_list[j], obj_list[i])) {
                    ++dom_count[i];
                }
            }
            if (dom_count[i] == 0u) {
                non_dom_rank[i] = 0u;
                non_dom_fronts[0].push_back(i);
            }
        }
        // we copy dom_count as we want to output its value at this point
        auto dom_count_copy(dom_count);
        auto current_front = non_dom_fronts[0];
        std::vector<std::vector<vector_double::size_type>>::size_type front_counter(0u);
        while(current_front.size()!=0u) {
            std::vector<vector_double::size_type> next_front;
            for (decltype(current_front.size()) p = 0u; p < current_front.size(); ++p) {
                for (decltype(dom_list[current_front[p]].size()) q = 0u; q < dom_list[current_front[p]].size(); ++q) {
                    --dom_count_copy[dom_list[current_front[p]][q]];
                    if (dom_count_copy[dom_list[current_front[p]][q]] == 0u) {
                        non_dom_rank[dom_list[current_front[p]][q]] = front_counter + 1u;
                        next_front.push_back(dom_list[current_front[p]][q]);
                    }
                }
            }
            ++front_counter;
            current_front = next_front;
            if (current_front.size() != 0u) {
                non_dom_fronts.push_back(current_front);
            }
        }
        return std::make_tuple(std::move(non_dom_fronts), std::move(dom_list), std::move(dom_count), std::move(non_dom_rank));
    }

/// Crowding distance 
/**
 * An implementation of the crowding distance. Complexity is \f$ O(MNlog(N))\f$ where \f$M\f$ is the number of objectives
 * and \f$N\f$ is the number of individuals. The function assumes the input is a non-dominated front. Failiure to rhis condition
 * will result in an undefined behaviour  
 *
 * @see Deb, Kalyanmoy, et al. "A fast elitist non-dominated sorting genetic algorithm
 * for multi-objective optimization: NSGA-II." Parallel problem solving from nature PPSN VI. Springer Berlin Heidelberg, 2000.
 *
 * @param[in] non_dom_front An <tt>std::vector<vector_double></tt> containing some non dominated front. Example {{0,0},{-1,1},{2,-2}}
 *
 * @returns a vector_double containing the crowding distances. Example: {2, inf, inf}
 * 
 * @throws std::invalid_argument If \p non_dom_front does not contain at least two points
 * @throws std::invalid_argument If points in \p do not all have at least two objectives
 * @throws std::invalid_argument If points in \p non_dom_front do not all have the same dimensionality
*/
vector_double crowding_distance(const std::vector<vector_double> &non_dom_front)
{
    auto N = non_dom_front.size();
    // We make sure to have two points at least
    if (N < 2) {
        pagmo_throw(std::invalid_argument, "A non dominated front must contain at least two points: " + std::to_string(N) + " detected.");
    }
    auto M = non_dom_front[0].size();
    // We make sure the first point of the input non dominated front contains at least two objectives
    if (M < 2) {
        pagmo_throw(std::invalid_argument, "Points in the non dominated front must contain at least two objectives: " + std::to_string(M) + " detected.");
    }
    // We make sure all points contain the same number of objectives    
    if (!std::all_of(non_dom_front.begin(), non_dom_front.end(), [M](const vector_double &item){return item.size() == M;})) {
        pagmo_throw(std::invalid_argument, "A non dominated front must contain points of uniform dimensionality. Some different sizes were instead detected.");
    }
    std::vector<vector_double::size_type> indexes(N);
    std::iota(indexes.begin(), indexes.end(), 0);
    vector_double retval(N,0);
    for (decltype(M) i=0u; i < M; ++i) {
        std::sort(indexes.begin(), indexes.end(), [i, non_dom_front] (vector_double::size_type idx1, vector_double::size_type idx2) {return non_dom_front[idx1][i] < non_dom_front[idx2][i];});
        retval[indexes[0]] = std::numeric_limits<double>::infinity();
        retval[indexes[N-1]] =  std::numeric_limits<double>::infinity();
        double df = non_dom_front[indexes[N-1]][i] - non_dom_front[indexes[0]][i];
        for (decltype(N-2u) j=1u; j < N-1u; ++j) {
            retval[indexes[j]] += (non_dom_front[indexes[j+1]][i] - non_dom_front[indexes[j-1]][i]) / df;
        }
    }
    return retval;
}

std::vector<vector_double::size_type> fitness_comparison_multi_objective(const std::vector<vector_double> &input_f)
{
    std::vector<vector_double::size_type> retval(input_f.size());
    std::iota(retval.begin(), retval.end(), vector_double::size_type(0u));
    // Run fast-non-dominated sorting and crowding distance for the population
    auto tuple = fast_non_dominated_sorting(input_f);
    vector_double crowding(input_f.size());
    for (auto front: std::get<0>(tuple)) {
        std::vector<vector_double> non_dom_fits(front.size());
        for (auto i = 0u; i < front.size(); ++i) {
            non_dom_fits[i] = input_f[front[i]];
        }
        vector_double tmp(crowding_distance(non_dom_fits));
        for (auto i = 0u; i < front.size(); ++i) {
            crowding[front[i]] = tmp[i];
        }
    }
    // Sort the indexes
    std::sort(retval.begin(), retval.end(), [tuple, crowding] (auto idx1, auto idx2) 
    {
        if (std::get<3>(tuple)[idx1] == std::get<3>(tuple)[idx2]) { // same non domination rank
            return crowding[idx1] > crowding[idx2]; // crowding distance decides
        } else {
            return std::get<3>(tuple)[idx1] < std::get<3>(tuple)[idx2]; // non domination rank decides
        };
    });
    return retval;
}


} // namespace pagmo
#endif