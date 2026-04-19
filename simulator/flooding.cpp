#include "simulator.hpp"

struct FloodState {
    bool visited = false;
};

struct FloodMessage {};


int main(int argc, char** argv) {
    if (argc < 3) {
        std::cerr << "Usage: " << argv[0] << " <mode> <graph_file> [min_delay] [max_delay]" << std::endl;
        return 1;
    }

    bool is_sync = argv[1] == std::string("sync");
    if(!is_sync && argc < 5) {
        std::cerr << "Asynchronous mode requires min_delay and max_delay parameters" << std::endl;
        return 1;
    }

    const std::string graph_file = argv[2];
    const std::string min_delay_str = is_sync ? "1" : argv[3];
    const std::string max_delay_str = is_sync ? "1" : argv[4];

    Graph graph(graph_file);
    Simulator<FloodState, FloodMessage> simulator(graph, is_sync, std::stoul(min_delay_str), std::stoul(max_delay_str));

    simulator.set_handlers(
        [](Simulator<FloodState, FloodMessage>& sim, size_t node_id, FloodState& state) {
            state.visited = true;
            for (size_t neighbor : sim.get_graph().neighbors(node_id)) {
                sim.send_message(node_id, neighbor, FloodMessage{});
            }
        },
        [](Simulator<FloodState, FloodMessage>& sim, size_t node_id, FloodState& state, size_t sender_id, const FloodMessage& message) {
            if (!state.visited) {
                state.visited = true;
                for (size_t neighbor : sim.get_graph().neighbors(node_id)) {
                    sim.send_message(node_id, neighbor, FloodMessage{});
                }
            }
        }
    );

    simulator.run(0);

    std::cout << "Total time: " << simulator.get_current_time() << std::endl;
    std::cout << "Total messages: " << simulator.get_message_counter() << std::endl;

    return 0;
}
