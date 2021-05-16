#include "joiner.h"

#include <cassert>
#include <iostream>
#include <string>
#include <unordered_map>
#include <utility>
#include <set>
#include <sstream>
#include <vector>
#include <random>       

#include "parser.h"

namespace {

enum QueryGraphProvides { Left, Right, Both, None };

// Analyzes inputs of join
QueryGraphProvides analyzeInputOfJoin(std::set<unsigned> &usedRelations,
                                      SelectInfo &leftInfo,
                                      SelectInfo &rightInfo) {
  bool used_left = usedRelations.count(leftInfo.binding);
  bool used_right = usedRelations.count(rightInfo.binding);

  if (used_left ^ used_right)
    return used_left ? QueryGraphProvides::Left : QueryGraphProvides::Right;
  if (used_left && used_right)
    return QueryGraphProvides::Both;
  return QueryGraphProvides::None;
}

}

// Loads a relation_ from disk
void Joiner::addRelation(const char *file_name) {
  relations_.emplace_back(file_name);
}

void Joiner::addRelation(Relation &&relation) {
  relations_.emplace_back(std::move(relation));
}

// Loads a relation from disk
const Relation &Joiner::getRelation(unsigned relation_id) {
  if (relation_id >= relations_.size()) {
    std::cerr << "Relation with id: " << relation_id << " does not exist"
              << std::endl;
    throw;
  }
  return relations_[relation_id];
}

// Add scan to query
std::unique_ptr<Operator> Joiner::addScan(std::set<unsigned> &used_relations,
                                          const SelectInfo &info,
                                          QueryInfo &query) {
  used_relations.emplace(info.binding);
  std::vector<FilterInfo> filters;
  for (auto &f : query.filters()) {
    if (f.filter_column.binding == info.binding) {
      filters.emplace_back(f);
    }
  }
  return !filters.empty() ?
         std::make_unique<FilterScan>(getRelation(info.rel_id), filters)
                          : std::make_unique<Scan>(getRelation(info.rel_id),
                                                   info.binding);
}

// Executes a join query
std::string Joiner::join(QueryInfo &query) {
  std::set<unsigned> used_relations;

  // We always start with the first join predicate and append the other joins
  // to it (--> left-deep join trees). You might want to choose a smarter
  // join ordering ...

  auto predicates_copy = query.predicates();
    for (unsigned j = 0; j< predicates_copy.size()-1; j++){
      for (unsigned i = 0; i< predicates_copy.size()-1; i++){
        auto first = predicates_copy[i];
        auto second = predicates_copy[i+1];
        auto firstL = getRelation(first.left.rel_id).size();
        auto firstR = getRelation(first.right.rel_id).size();
        auto secondL = getRelation(second.left.rel_id).size();
        auto secondR = getRelation(second.right.rel_id).size();
        auto firstSize = firstR*firstL;
        auto secondSize = secondL*secondR;
        if (firstSize > secondSize){
          // std::cerr << "in" << std::endl;
          predicates_copy[i] = second;
          predicates_copy[i+1] = first;
        }
      }
    }
  std::unique_ptr<Operator>
        root;

  // if (predicates_copy.size() == 2){
  //   std::cerr << "test" << std::endl;
  //   const auto &firstJoin = predicates_copy[0];
  //   std::unique_ptr<Operator> left, right;
  //   left = addScan(used_relations, firstJoin.left, query);
  //   right = addScan(used_relations, firstJoin.right, query);
  //   root = std::make_unique<Join>(move(left), move(right), firstJoin);
  //   std::vector<std::unique_ptr<Operator>> roots;
  //   // for (unsigned i = 0; i < predicates_copy.size(); i+=2) {
  //     const auto &firstJoin1 = predicates_copy[0];
  //     const auto &firstJoin2 = predicates_copy[1];
  //     std::unique_ptr<Operator> root1, left1, right1;
  //     std::unique_ptr<Operator> root2, left2, right2;
  //     left1 = addScan(used_relations, firstJoin1.left, query);
  //     right1 = addScan(used_relations, firstJoin1.right, query);
  //     root1 = std::make_unique<Join>(move(left1), move(right1), firstJoin1);
  //     left1 = addScan(used_relations, firstJoin2.left, query);
  //     right1 = addScan(used_relations, firstJoin2.right, query);
  //     root1 = std::make_unique<Join>(move(left2), move(right2), firstJoin2);
  //     std::unique_ptr<Operator> newroot = std::make_unique<Join>(move(left2), move(right2), firstJoin2);
  //     root = std::make_unique<Join>(move(root1), move(r), firstJoin2);
      // roots.push_back(newroot);
    // }

    // root = std::make_unique<Join>(move(roots[0]), move(roots[1]), firstJoin);

      // auto &p_info = predicates_copy[i];
      // auto &left_info = p_info.left;
      // auto &right_info = p_info.right;

      // switch (analyzeInputOfJoin(used_relations, left_info, right_info)) {
      //   case QueryGraphProvides::Left:
      //     left = move(root);
      //     right = addScan(used_relations, right_info, query);
      //     root = std::make_unique<Join>(move(left), move(right), p_info);
      //     break;
      //   case QueryGraphProvides::Right:
      //     left = addScan(used_relations,
      //                   left_info,
      //                   query);
      //     right = move(root);
      //     root = std::make_unique<Join>(move(left), move(right), p_info);
      //     break;
      //   case QueryGraphProvides::Both:
      //     // All relations of this join are already used somewhere else in the
      //     // query. Thus, we have either a cycle in our join graph or more than
      //     // one join predicate per join.
      //     root = std::make_unique<SelfJoin>(move(root), p_info);
      //     break;
      //   case QueryGraphProvides::None:
      //     // Process this predicate later when we can connect it to the other
      //     // joins. We never have cross products.
      //     predicates_copy.push_back(p_info);
      //     break;
      // };
  // }
  // else{
    const auto &firstJoin = predicates_copy[0];
    std::unique_ptr<Operator> left, right;
    left = addScan(used_relations, firstJoin.left, query);
    right = addScan(used_relations, firstJoin.right, query);
    root = std::make_unique<Join>(move(left), move(right), firstJoin);
    
    for (unsigned i = 1; i < predicates_copy.size(); ++i) {
      auto &p_info = predicates_copy[i];
      auto &left_info = p_info.left;
      auto &right_info = p_info.right;

      switch (analyzeInputOfJoin(used_relations, left_info, right_info)) {
        case QueryGraphProvides::Left:left = move(root);
          right = addScan(used_relations, right_info, query);
          root = std::make_unique<Join>(move(left), move(right), p_info);
          break;
        case QueryGraphProvides::Right:
          left = addScan(used_relations,
                        left_info,
                        query);
          right = move(root);
          root = std::make_unique<Join>(move(left), move(right), p_info);
          break;
        case QueryGraphProvides::Both:
          // All relations of this join are already used somewhere else in the
          // query. Thus, we have either a cycle in our join graph or more than
          // one join predicate per join.
          root = std::make_unique<SelfJoin>(move(root), p_info);
          break;
        case QueryGraphProvides::None:
          // Process this predicate later when we can connect it to the other
          // joins. We never have cross products.
          predicates_copy.push_back(p_info);
          break;
      };
    }
  // }

  Checksum checksum(move(root), query.selections());
  checksum.run();

  std::stringstream out;
  auto &results = checksum.check_sums();
  for (unsigned i = 0; i < results.size(); ++i) {
    out << (checksum.result_size() == 0 ? "NULL" : std::to_string(results[i]));
    if (i < results.size() - 1)
      out << " ";
  }
  out << "\n";
  return out.str();
}

