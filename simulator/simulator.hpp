#ifndef SIMULATOR_HPP
#define SIMULATOR_HPP

#include <iostream>
#include <vector>
#include <queue>
#include <map>
#include <functional>
#include <random>
#include <algorithm>

#include "graph.hpp"

template<typename NodeState, typename Message>
class Simulator {
public:
    using InitHandler = std::function<void(Simulator& sim, size_t node_id, NodeState& state)>;
    using ReceiveHandler = std::function<void(Simulator& sim, size_t node_id, NodeState& state, size_t sender_id, const Message& message)>;

    Simulator() = delete;
    Simulator(const Graph& graph, bool is_synchronous, size_t min_delay=1, size_t max_delay=1) : 
        graph_{graph}, is_synchronous_{is_synchronous}, current_time_{0.0}, min_delay_{min_delay}, max_delay_{max_delay} {
        
        for(size_t node_id : graph_.nodes()) {
            states_[node_id] = NodeState{};
        }

        delay_dist_ = std::uniform_int_distribution<size_t>(min_delay_, max_delay_);
    }

    Simulator(const Simulator&) = default;
    Simulator(Simulator&&) = default;

    Simulator& operator=(const Simulator&) = default;
    Simulator& operator=(Simulator&&) = default;

    ~Simulator() = default;

    void set_handlers(InitHandler init_handler, ReceiveHandler receive_handler) {
        on_init_ = init_handler;
        on_receive_ = receive_handler;
    }

    const Graph& get_graph() const { 
        return graph_; 
    }
    
    double get_current_time() const { 
        return current_time_; 
    }

    size_t get_message_counter() const {
        return message_counter_;
    }

    const NodeState& get_node_state(size_t node_id) const {
        return states_.at(node_id);
    }

    void send_message(size_t sender, size_t receiver, const Message& data) {
        if (!graph_.has_node(receiver)) {
            return;
        }

        const size_t delay = is_synchronous_ ? 1 : delay_dist_(rng_);
        event_queue_.push({current_time_ + delay, sender, receiver, data});
    }

    void run(ssize_t start_node_id = -1) {
        std::uniform_int_distribution<size_t> node_dist(0, graph_.nodes_count() - 1);
        if (start_node_id == -1) {
            start_node_id = graph_.nodes().at(node_dist(rng_));
        }

        if (on_init_) {
            on_init_(*this, start_node_id, states_[start_node_id]);
        }

        while (!event_queue_.empty()) {
            Event ev = event_queue_.top();
            event_queue_.pop();
            
            current_time_ = ev.delivery_time;
            message_counter_++;
            
            if (on_receive_) {
                on_receive_(*this, ev.receiver_id, states_[ev.receiver_id], ev.sender_id, ev.message);
            }
        }
    }

private:
    struct Event {
        double delivery_time;
        size_t sender_id;
        size_t receiver_id;
        Message message;

        bool operator>(const Event& other) const {
            return delivery_time > other.delivery_time;
        }
    };

    const Graph& graph_;
    bool is_synchronous_;
    size_t min_delay_;
    size_t max_delay_;
    double current_time_;
    size_t message_counter_;

    std::priority_queue<Event, std::vector<Event>, std::greater<Event>> event_queue_;
    std::map<size_t, NodeState> states_;

    std::mt19937 rng_{std::random_device{}()};
    std::uniform_int_distribution<size_t> delay_dist_;

    InitHandler on_init_;
    ReceiveHandler on_receive_;
};

#endif // SIMULATOR_HPP
