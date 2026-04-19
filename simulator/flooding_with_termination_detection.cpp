#include "simulator.hpp"

enum class MessageType {
    FLOOD,
    ACK
};

struct FloodState {
    bool visited = false;
    ssize_t parent = -1;
    size_t expected_acks = 0;
    size_t level = 0;
    size_t visit_time = 0;
};

struct FloodMessage {
    MessageType type;
    size_t level = 0;
};


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

            const auto& neighbors = sim.get_graph().neighbors(node_id);
            state.expected_acks = neighbors.size();

            for (size_t neighbor : sim.get_graph().neighbors(node_id)) {
                sim.send_message(node_id, neighbor, FloodMessage{MessageType::FLOOD, 0});
            }
        },
        [](Simulator<FloodState, FloodMessage>& sim, size_t node_id, FloodState& state, size_t sender_id, const FloodMessage& message) {
            if (message.type == MessageType::FLOOD) {
                if (!state.visited) {
                    state.visited = true;
                    state.parent = sender_id;
                    state.level = message.level + 1;
                    state.visit_time = sim.get_current_time();

                    const auto& neighbors = sim.get_graph().neighbors(node_id);
                    state.expected_acks = neighbors.size() - 1;

                    if (state.expected_acks == 0) {
                        sim.send_message(node_id, state.parent, {MessageType::ACK});
                    } else {
                        for (size_t neighbor : neighbors) {
                            if (neighbor != state.parent) {
                                sim.send_message(node_id, neighbor, {MessageType::FLOOD, state.level});
                            }
                        }
                    }
                } 
                else {
                    sim.send_message(node_id, sender_id, {MessageType::ACK});
                }
            } 
            else if (message.type == MessageType::ACK) {
                state.expected_acks--;

                if (state.expected_acks == 0) {
                    
                    if (state.parent == -1) {
                        std::cout << "TERMINATION DETECTED at node " << node_id << " at time " << sim.get_current_time() << std::endl;
                    } 
                    else {
                        sim.send_message(node_id, state.parent, {MessageType::ACK});
                    }
                }
            }
        }
    );

    simulator.run(0);

    std::cout << "Total time: " << simulator.get_current_time() << std::endl;
    std::cout << "Total messages: " << simulator.get_message_counter() << std::endl;

    for(size_t node_id : graph.nodes()) {
        const FloodState& state = simulator.get_node_state(node_id);
        std::cout << "Node " << node_id << ": parent=" << state.parent << ", level=" << state.level << ", visit_time=" << state.visit_time << std::endl;
    }

    return 0;
}
