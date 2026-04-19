#include <iostream>
#include <fstream>
#include <cstring>


void generate_complete_graph(std::ofstream& output_file, size_t num_nodes) {
    for (size_t i = 0; i < num_nodes; ++i) {
        for (size_t j = i + 1; j < num_nodes; ++j) {
            output_file << i << " " << j << "\n";
        }
    }
}

void generate_ring_graph(std::ofstream& output_file, size_t num_nodes) {
    for (size_t i = 0; i < num_nodes; ++i) {
        output_file << i << " " << (i + 1) % num_nodes << "\n";
    }
}

void generate_star_graph(std::ofstream& output_file, size_t num_nodes) {
    for (size_t i = 1; i < num_nodes; ++i) {
        output_file << 0 << " " << i << "\n";
    }
}

void generate_tree_graph(std::ofstream& output_file, size_t num_nodes) {
    for (size_t i = 1; i < num_nodes; ++i) {
        output_file << (i - 1) / 2 << " " << i << "\n";
    }
}

void generate_path_graph(std::ofstream& output_file, size_t num_nodes) {
    for (size_t i = 0; i < num_nodes - 1; ++i) {
        output_file << i << " " << i + 1 << "\n";
    }
}

void generate_lollipop_graph(std::ofstream& output_file, size_t num_nodes) {
    const size_t half = num_nodes / 2;
    for (size_t i = 0; i < half - 1; ++i) {
        output_file << i << " " << i + 1 << "\n";
    }
    
    for (size_t i = half; i < num_nodes; ++i) {
        for (size_t j = i + 1; j < num_nodes; ++j) {
            output_file << i << " " << j << "\n";
        }
    }
}

/* Graph types:
    - "complete": every node is connected to every other node
    - "ring": nodes are arranged in a ring, each node connected to two neighbors
    - "star": one central node connected to all others, which are not connected to each other
    - "tree": a balanced binary tree structure
    - "path": nodes arranged in a line, each node connected to its immediate neighbors
    - "lollipop": a path with a clique at the end (first half of nodes in a line, second half connected to the last node of the line)
*/
int main(int argc, char** argv) {
    if (argc < 4) {
        std::cerr << "Usage: " << argv[0] << " <graph_type> <num_nodes> <graph_file>" << std::endl;
        return 1;
    }

    const size_t num_nodes = std::stoul(argv[2]);
    std::ofstream output_file(argv[3]);

    if (strcmp(argv[1], "complete") == 0) {
        generate_complete_graph(output_file, num_nodes);
    } else if (strcmp(argv[1], "ring") == 0) {
        generate_ring_graph(output_file, num_nodes);
    } else if (strcmp(argv[1], "star") == 0) {
        generate_star_graph(output_file, num_nodes);
    } else if (strcmp(argv[1], "tree") == 0) {
        generate_tree_graph(output_file, num_nodes);
    } else if (strcmp(argv[1], "path") == 0) {
        generate_path_graph(output_file, num_nodes);
    } else if (strcmp(argv[1], "lollipop") == 0) {
        generate_lollipop_graph(output_file, num_nodes);
    } else {
        std::cerr << "Unknown graph type: " << argv[1] << std::endl;
        return 1;
    }

    output_file.close();
    return 0;
}