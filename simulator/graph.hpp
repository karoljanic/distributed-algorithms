#ifndef GRAPH_HPP
#define GRAPH_HPP

#include <algorithm>
#include <map>
#include <vector>
#include <fstream>

class Graph {
public:
    Graph() = default;
    Graph(const std::string& graph_file) {
        std::ifstream input_file(graph_file);

        size_t node1, node2;
        while (input_file >> node1 >> node2) {
            add_link(node1, node2);
        }
        
        input_file.close();
    }

    Graph(const Graph&) = default;
    Graph(Graph&&) = default;

    Graph& operator=(const Graph&) = default;
    Graph& operator=(Graph&&) = default;
    
    ~Graph() = default;

    void add_node(size_t node) {
        if(adjacency_list_.find(node) == adjacency_list_.end()) {
            adjacency_list_[node] = std::vector<size_t>();
        }
    }

    void add_link(size_t node1, size_t node2) {
        add_node(node1);
        add_node(node2);

        adjacency_list_[node1].push_back(node2);
        adjacency_list_[node2].push_back(node1);
    }

    const size_t nodes_count() const {
        return adjacency_list_.size();
    }

    const size_t links_count() const {
        size_t count = 0;
        for(const auto& [node, neighbors] : adjacency_list_) {
            count += neighbors.size();
        }
        return count / 2;
    }

    const std::vector<size_t> nodes() const {
        std::vector<size_t> result;
        for(const auto& [node, _] : adjacency_list_) {
            result.push_back(node);
        }
        return result;
    }

    const std::vector<size_t> links() const {
        std::vector<size_t> result;
        for(const auto& [node, neighbors] : adjacency_list_) {
            for(size_t neighbor : neighbors) {
                if(node < neighbor) {
                    result.push_back(node);
                    result.push_back(neighbor);
                }
            }
        }
        return result;
    }

    const std::vector<size_t>& neighbors(size_t node) const {
        if(adjacency_list_.find(node) == adjacency_list_.end()) {
            static const std::vector<size_t> empty;
            return empty;
        }
        return adjacency_list_.at(node);
    }

    bool has_node(size_t node) const {
        return adjacency_list_.find(node) != adjacency_list_.end();
    }

    bool has_link(size_t node1, size_t node2) const {
        if(adjacency_list_.find(node1) == adjacency_list_.end()) {
            return false;
        }

        const auto& neighbors = adjacency_list_.at(node1);
        return std::find(neighbors.begin(), neighbors.end(), node2) != neighbors.end();
    }

private:
    std::map<size_t, std::vector<size_t>> adjacency_list_;
};

#endif // GRAPH_HPP
