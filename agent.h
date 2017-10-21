#pragma once
#include <string>
#include <random>
#include <sstream>
#include <map>
#include <type_traits>
#include <algorithm>
#include "board.h"
#include "action.h"
#include "weight.h"

#define MAX_INT ((unsigned)(-1)>>1)  
#define MIN_INT (~MAX_INT)  
class agent {
public:
	agent(const std::string& args = "") {
		std::stringstream ss(args);
		for (std::string pair; ss >> pair; ) {
			std::string key = pair.substr(0, pair.find('='));
			std::string value = pair.substr(pair.find('=') + 1);
			property[key] = { value };
		}
	}
	virtual ~agent() {}
	virtual void open_episode(const std::string& flag = "") {}
	virtual void close_episode(const std::string& flag = "") {}
	virtual action take_action(const board& b) { return action(); }
	virtual bool check_for_win(const board& b) { return false; }

public:
	virtual std::string name() const {
		auto it = property.find("name");
		return it != property.end() ? std::string(it->second) : "unknown";
	}
protected:
	typedef std::string key;
	struct value {
		std::string value;
		operator std::string() const { return value; }
		template<typename numeric, typename = typename std::enable_if<std::is_arithmetic<numeric>::value, numeric>::type>
		operator numeric() const { return numeric(std::stod(value)); }
	};
	std::map<key, value> property;
};

/**
 * evil (environment agent)
 * add a new random tile on board, or do nothing if the board is full
 * 2-tile: 90%
 * 4-tile: 10%
 */
class rndenv : public agent {
public:
	rndenv(const std::string& args = "") : agent("name=rndenv " + args) {
		if (property.find("seed") != property.end())
			engine.seed(int(property["seed"]));
	}

	virtual action take_action(const board& after) {
		int space[] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 };
		std::shuffle(space, space + 16, engine);
		for (int pos : space) {
			if (after(pos) != 0) continue;
			std::uniform_int_distribution<int> popup(0, 9);
			int tile = popup(engine) ? 1 : 2;
			return action::place(tile, pos);
		}
		return action();
	}

private:
	std::default_random_engine engine;
};

/**
 * TODO: player (non-implement)
 * always return an illegal action
 */
class player : public agent {
public:
	player(const std::string& args = "") : agent("name=player " + args), alpha(0.0025f) {
		episode.reserve(32768);
		if (property.find("seed") != property.end())
			engine.seed(int(property["seed"]));
		if (property.find("alpha") != property.end())
			alpha = float(property["alpha"]);

		if (property.find("load") != property.end())
			load_weights(property["load"]);
		else {
			weights.push_back(weight(1 << 25));
			weights.push_back(weight(1 << 25));
			weights.push_back(weight(1 << 25));
			weights.push_back(weight(1 << 25));
		}
		// TODO: initialize the n-tuple network
	}
	~player() {
		if (property.find("save") != property.end())
			save_weights(property["save"]);
	}

	virtual void open_episode(const std::string& flag = "") {
		episode.clear();
		episode.reserve(32768);
	}

	virtual void close_episode(const std::string& flag = "") {
		// TODO: train the n-tuple network by TD(0)

		for (int i = episode.size() - 1; i >= 0; i--) {
			double error;
			if (i == (int)episode.size() - 1) {
				state final_state = episode[i];
				error = -calculate(final_state.after);
				update(final_state.after, error);
			} else {
				state state1 = episode[i + 1];
				state state2 = episode[i];
				error = calculate(state1.after) + state2.reward - calculate(state2.after);
				update(state2.after, error);
			}
		}
	}

	virtual action take_action(const board& before) {
		
		state board_state[4];

		for (int i = 0; i < 4; i++) { 
			board b = before;
			board_state[i].before = before;
			board_state[i].reward = b.move(i);
			board_state[i].after = b;
			board_state[i].move = action::move(i);
		}
		
		int max_index = -1;
		double max = -9999999999;

		for (int i = 0; i < 4; i++) {
			if (board_state[i].reward != -1 && max < calculate(board_state[i].after)) {
					max = calculate(board_state[i].after);
					max_index = i;
			}
		}
		
		
		if (max_index != -1) {
			episode.push_back(board_state[max_index]);
			return board_state[max_index].move;
		} else {
			return action();
		} 
		 
	}

public:
	virtual void load_weights(const std::string& path) {
		std::ifstream in;
		in.open(path.c_str(), std::ios::in | std::ios::binary);
		if (!in.is_open()) std::exit(-1);
		size_t size;
		in.read(reinterpret_cast<char*>(&size), sizeof(size));
		weights.resize(size);
		for (weight& w : weights)
			in >> w;
		in.close();
	}

	virtual void save_weights(const std::string& path) {
		std::ofstream out;
		out.open(path.c_str(), std::ios::out | std::ios::binary | std::ios::trunc);
		if (!out.is_open()) std::exit(-1);
		size_t size = weights.size();
		out.write(reinterpret_cast<char*>(&size), sizeof(size));
		for (weight& w : weights)
			out << w;
		out.flush();
		out.close();
	}

	virtual double calculate(board& after_board) {
		double value = 0;
		
		for (int i = 0; i < 4; ++i) {
			value += weights[0][(after_board(0) 	<< 20) + (after_board(1) 	<< 15) + (after_board(4) 	<< 10) + (after_board(5) 	<< 5) + after_board(6)];
			value += weights[1][(after_board(2) 	<< 20) + (after_board(3) 	<< 15) + (after_board(7) 	<< 10) + (after_board(11) 	<< 5) + after_board(15)];
			value += weights[2][(after_board(5) 	<< 20) + (after_board(6) 	<< 15) + (after_board(8) 	<< 10) + (after_board(9) 	<< 5) + after_board(10)];
			value += weights[3][(after_board(9) 	<< 20) + (after_board(10) 	<< 15) + (after_board(12) 	<< 10) + (after_board(13) 	<< 5) + after_board(14)];
			after_board.rotate_right();
		}

		return value;
	}

	virtual void update(board& after_board, double error) {
		
		double update_value = alpha * error;

		for (int i = 0; i < 4; ++i) {
			weights[0][(after_board(0) 	<< 20) + (after_board(1) 	<< 15) + (after_board(4) 	<< 10) + (after_board(5) 	<< 5) + after_board(6)] += update_value;
			weights[1][(after_board(2) 	<< 20) + (after_board(3) 	<< 15) + (after_board(7) 	<< 10) + (after_board(11) 	<< 5) + after_board(15)] += update_value;
			weights[2][(after_board(5) 	<< 20) + (after_board(6) 	<< 15) + (after_board(8) 	<< 10) + (after_board(9) 	<< 5) + after_board(10)] += update_value;
			weights[3][(after_board(9) 	<< 20) + (after_board(10) 	<< 15) + (after_board(12) 	<< 10) + (after_board(13) 	<< 5) + after_board(14)] += update_value;
			after_board.rotate_right();
		}
	}

private:
	std::vector<weight> weights;

	struct state {
		// TODO: select the necessary components of a state
		board before;
		board after;
		action move;
		int reward;
	};

	std::vector<state> episode;
	float alpha;

private:
	std::default_random_engine engine;
};
