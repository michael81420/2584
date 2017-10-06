#pragma once
#include <string>
#include <random>
#include <sstream>
#include <map>
#include <type_traits>
#include <algorithm>
#include "board.h"
#include "action.h"
using namespace std ;


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
 * player (dummy)
 * select an action randomly
 */
class player : public agent {
public:
	player(const std::string& args = "") : agent("name=player " + args) {
		if (property.find("seed") != property.end())
			engine.seed(int(property["seed"]));
	}

	virtual action take_action(const board& before) {
		static int pre_action = 3;

		//0, 1, 2, 3
		//U, R, D, L
		int *opcode;
		int op_table[4][4] = {
			{ 2, 3, 0, 1 },
			{ 3, 2, 0, 1 },
			{ 3, 2, 0, 1 },
			{ 2, 3, 0, 1 }
			/*{1, 2, 3, 0},
			{2, 3, 0, 1},
			{3, 0, 1, 2},
			{0, 1, 2, 3}*/
		};

		switch (pre_action) {
			case 0:
				opcode = op_table[0];
				break;
			case 1:
				opcode = op_table[1];
				break;
			case 2:
				opcode = op_table[2];
				break;
			case 3:
				opcode = op_table[3];
				break;
		}

		for (int i = 0; i < 4; i++) {
			int op = opcode[i];
			board b = before;
			if (b.move(op) != -1) {
				pre_action = op;
				return action::move(op);
			}
			if (i == 1)
			{
				if (b(13) > b(8)) {
					int tmp = opcode[2];
					opcode[2] = opcode[3];
					opcode[3] = tmp;
				} else if(b(8) == b(13)) {
					if (b(13) && b(14) && b(15)) {
						int tmp = opcode[2];
						opcode[2] = opcode[3];
						opcode[3] = tmp;
					}
				} 
			}
		}

		return action();
		
		/*std::shuffle(opcode, opcode + 4, engine);
		for (int op : opcode) {
			board b = before;
			if (b.move(op) != -1) return action::move(op);
		}*/
		

	}

private:
	std::default_random_engine engine;
};
