#include "Plan.h"
#include "PhysicalOperators.h"

namespace lightdb::optimization {
    const PhysicalOperatorReference& Plan::add(const PhysicalOperatorReference &physical) {
        physical_.insert(physical);
        assign(physical->logical(), physical);
        return physical;
    }

    void Plan::assign(const LightFieldReference &node, const PhysicalOperatorReference &physical) {
        if(assigned_.find(&*node) == assigned_.end())
            assigned_[&*node] = {};
        assigned_[&*node].insert(physical);
    }

    const set<PhysicalOperatorReference> Plan::assignments(const LightFieldReference &reference) const {
        auto element = assigned_.find(&*reference);
        return element != assigned_.end()
               ? element->second
               : set<PhysicalOperatorReference>{};
    }

    set<PhysicalOperatorReference> Plan::assignments(const LightFieldReference &reference) {
        auto element = assigned_.find(&*reference);
        return element != assigned_.end()
               ? element->second
               : set<PhysicalOperatorReference>{};
    }

    std::vector<PhysicalOperatorReference> Plan::unassigned(const LightFieldReference &reference, const bool global) const {
        auto assignments = this->assignments(reference);
        std::set<PhysicalOperator*> nonleafs;
        std::vector<PhysicalOperatorReference> leafs;

        for(const auto &assignment: assignments)
            for(auto& parent: assignment->parents())
                nonleafs.insert(&*parent);

        for(const auto &assignment: assignments)
            if(nonleafs.find(&*assignment) == nonleafs.end())
                leafs.push_back(assignment);

        //TODO this is like O(n^999) :-/
        if(global)
            for (const auto &child: children(reference))
                for (const auto &assignment: this->assignments(child))
                    for (const auto &parent: assignment->parents())
                        for (auto index = 0u; index < leafs.size(); index++)
                            if (parent == leafs[index])
                                leafs.erase(leafs.begin() + index--);
        return leafs;
    }

    set<PhysicalOperatorReference> Plan::children(const PhysicalOperatorReference &reference) const {
        set<PhysicalOperatorReference> children;

        for(const auto &node: physical_)
            for(const auto &parent: node->parents())
                if(parent == reference)
                    children.insert(node);

        return children;
    }

    void Plan::replace_assignments(const PhysicalOperatorReference &original, const PhysicalOperatorReference &replacement) {
        for(auto &[logical, assignments]: assigned_)
            if(assignments.find(original) != assignments.end()) {
                assignments.erase(original);
                assignments.insert(replacement);
            }

        for(auto &physical: physical_) {
            auto &parents = physical->parents();
            if(std::find(parents.begin(), parents.end(), original) != parents.end()) {
                parents.erase(std::remove(parents.begin(), parents.end(), replacement), parents.end());
                std::replace(parents.begin(), parents.end(), original, replacement);
            }
        }

        physical_.erase(original);
    }

    void Plan::remove_operator(const lightdb::PhysicalOperatorReference &original) {
        for (auto &[logical, assignments]: assigned_) {
            if (assignments.find(original) != assignments.end()) {
                assignments.erase(original);
            }
        }

        for (auto &physical: physical_) {
            auto &parents = physical->parents();
            if (std::find(parents.begin(), parents.end(), original) != parents.end())
                assert(false); // No other physical operator should be related to the one being removed.
        }

        physical_.erase(original);
    }

    void Plan::replace_assignments(const std::vector<PhysicalOperatorReference> &originals, const PhysicalOperatorReference &replacement) {
        // Remove a few operators to be replaced by one.
        for (auto &[logical, assignments]: assigned_) {
            for (auto &original : originals) {
                if (assignments.find(original) != assignments.end())
                    assignments.erase(original);
            }
            assignments.insert(replacement);
        }

        for (auto &physical : physical_) {
            auto &parents = physical->parents();
            // See if parents contains both of originals. Assume that it must contain both?
            for (auto &original : originals) {
                if (std::find(parents.begin(), parents.end(), original) != parents.end()) {
                    break;
                }
            }
            parents.erase(std::remove(parents.begin(), parents.end(), replacement), parents.end());

            // Replace the first original with the replacement.
            // Remove the rest of the originals.
            std::replace(parents.begin(), parents.end(), originals.front(), replacement);
            for (long unsigned int i = 1; i < originals.size(); ++i) {
                std::remove(parents.begin(), parents.end(), originals[i]);
            }
        }

        for (auto &original : originals) {
            physical_.erase(original);
        }
    }
} // namespace lightdb::optimization

