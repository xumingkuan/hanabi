#include <bits/stdc++.h>
using namespace std;
constexpr int kNumSuits = 5;  // 0 (printed as "A") = multicolor
constexpr int kNumNumbers = 5;
const int kNumbers[kNumNumbers] = {3, 2, 2, 2, 1};
constexpr int kInitHintToken = 8;
constexpr int kMaxHintToken = 8;
constexpr int kInitLifeToken = 3;
int num_players = 5;
int num_cards = 4;

// public info
int board[kNumSuits];  // stores the next number (0-5)
int num_discarded[kNumSuits][kNumNumbers];
int drawn_cards, total_cards;
int total_score;

enum Discardable {
	kUnknown, // unknown to the player
	kNo, // no to the player
	kYes, // yes to the player
	kForcedUnknown // still unknown after one move to the player
};

class OtherPlayer {
public:
	vector<int> cards;
	vector<int> plays;  // The card positions to be played next for that player.
	vector<Discardable> discardable;  // The information known to that player.
	OtherPlayer(const vector<int> &cards): cards(cards) {
		discardable.assign(num_cards, kUnknown);
	}
};

bool agent_play(int agent_id, int position);
bool agent_discard(int agent_id, int position);
bool agent_hint(int agent_id, int hinted_agent_id, int suit, int number, const vector<int> &positions);

class Agent {
public:
	vector<OtherPlayer> others;  // others[0] is the next player
	vector<int> plays;
	vector<Discardable> discardable;
	int agent_id;
	Agent(int agent_id, const vector<vector<int>> &other_player_cards): agent_id(agent_id) {
		for (int i = 0; i < num_players - 1; i++) {
			others.emplace_back(other_player_cards[i]);
		}
		discardable.assign(num_cards, kUnknown);
	}
	void print_self_info() const {
		printf("Player %d:", agent_id + 1);
		if (!plays.empty()) {
			printf(" to play cards");
			for (const auto &pos : plays) {
				printf(" %d", pos + 1);
			}
			printf(",");
		}
		printf(" discardable ");
		for (int i = 0; i < (int)discardable.size(); i++) {
			printf("%c", discardable[i] == kYes ? 'Y' : discardable[i] == kNo ? 'N' : discardable[i] == kUnknown ? '.' : 'U');
		}
		printf("\n");
	}
	bool others_card_playable(int relative_id, int position) const {
		auto this_card = others[relative_id].cards[position];
		if (board[this_card / kNumNumbers] != this_card % kNumNumbers) {
			return false;
		}
		for (const auto &other : others) {
			for (const auto &other_play : other.plays) {
				if (other.cards[other_play] == this_card) {
					// cannot guarantee that this card will be played before the other
					return false;
				}
			}
		}
		// playable at any time
		return true;
	}
	bool others_card_discardable(int relative_id, int position) const {
		auto this_card = others[relative_id].cards[position];
		auto current_num = kNumbers[this_card % kNumNumbers] - num_discarded[this_card / kNumNumbers][this_card % kNumNumbers];
		if (current_num <= 1) {
			return false;
		}
		for (const auto &other : others) {
			for (int i = 0; i < (int)other.cards.size(); i++) {
				if (other.cards[i] == this_card && other.discardable[i] == kYes) {
					// cannot guarantee that this card will be discarded before the other
					current_num--;
					if (current_num <= 1) {
						return false;
					}
				}
			}
		}
		// discardable at any time
		return true;
	}
	void update_other_play(int other_id, int position, int new_card) {
		int relative_id = (other_id - agent_id - 1 + num_players) % num_players;
		others[relative_id].cards.erase(others[relative_id].cards.begin() + position);
		others[relative_id].discardable.erase(others[relative_id].discardable.begin() + position);
		others[relative_id].plays.erase(others[relative_id].plays.begin());
		for (auto &play_pos : others[relative_id].plays) {
			if (play_pos > position) {
				play_pos--;
			}
		}
		if (new_card != -1) {
			others[relative_id].cards.push_back(new_card);
			others[relative_id].discardable.push_back(kUnknown);
		}
		if (relative_id == 0) {
			// the next player played, so my new card is discardable.
			if (discardable.back() == kUnknown) {
				discardable.back() = kYes;
			}
		} else {
			if (others[relative_id - 1].discardable.back() == kUnknown) {
				others[relative_id - 1].discardable.back() = kYes;
			}
		}
	}
	void update_other_discard(int other_id, int position, int new_card) {
		int relative_id = (other_id - agent_id - 1 + num_players) % num_players;
		auto card_discardable = others[relative_id].discardable[position];
		others[relative_id].cards.erase(others[relative_id].cards.begin() + position);
		others[relative_id].discardable.erase(others[relative_id].discardable.begin() + position);
		auto it = find(others[relative_id].plays.begin(), others[relative_id].plays.end(), position);
		if (it != others[relative_id].plays.end()) {
			others[relative_id].plays.erase(it);
		}
		for (auto &play_pos : others[relative_id].plays) {
			if (play_pos > position) {
				play_pos--;
			}
		}
		if (new_card != -1) {
			others[relative_id].cards.push_back(new_card);
			others[relative_id].discardable.push_back(kUnknown);
		}
		if (others[relative_id].plays.empty() && card_discardable == kYes) {
			if (relative_id == 0) {
				// the next player discarded, so my new card is discardable.
				if (discardable.back() == kUnknown) {
					discardable.back() = kYes;
				}
			} else {
				if (others[relative_id - 1].discardable.back() == kUnknown) {
					others[relative_id - 1].discardable.back() = kYes;
				}
			}
		} else if (!others[relative_id].plays.empty() ||
				(card_discardable != kYes && find(others[relative_id].discardable.begin(), others[relative_id].discardable.end(), kYes) != others[relative_id].discardable.end())) {
			if (relative_id == 0) {
				// the next player discarded when that player has a play, so my new card is not discardable.
				if (discardable.back() == kUnknown) {
					discardable.back() = kNo;
				}
			} else {
				if (others[relative_id - 1].discardable.back() == kUnknown) {
					others[relative_id - 1].discardable.back() = kNo;
				}
			}
		} else {
			// TODO: check if |relative_id| is able to give a hint -- if yes then |kNo| here instead would have been more accurate
			if (relative_id == 0) {
				// this is a random discard with no information.
				if (discardable.back() == kUnknown) {
					discardable.back() = kForcedUnknown;
				}
			} else {
				if (others[relative_id - 1].discardable.back() == kUnknown) {
					others[relative_id - 1].discardable.back() = kForcedUnknown;
				}
			}
		}
	}
	void update_hint(int hinter_id, int hinted_id, int suit, int number, const vector<int> &positions) {
		bool playable = (suit != -1);
		if (number != -1) {
			for (int i = 0; i < kNumSuits; i++) {
				if (board[i] == number) {
					playable = true;
					break;
				}
			}
		}
		if (hinted_id == agent_id) {
			if (playable) {
				for (int i = 0; i < (int)positions.size(); i++) {
					if (find(plays.begin(), plays.end(), positions[i]) == plays.end()) {
						plays.push_back(positions[i]);
						break;
					}
				}
			} else {
				for (int i = 0; i < (int)positions.size(); i++) {
					if (discardable[i] == kUnknown || discardable[i] == kForcedUnknown) {
						discardable[i] = kNo;
						break;
					}
				}
			}
		} else {
			int relative_id = (hinted_id - agent_id - 1 + num_players) % num_players;
			if (playable) {
				for (int i = 0; i < (int)positions.size(); i++) {
					if (find(others[relative_id].plays.begin(), others[relative_id].plays.end(), positions[i]) == others[relative_id].plays.end()) {
						others[relative_id].plays.push_back(positions[i]);
						break;
					}
				}
			} else {
				for (int i = 0; i < (int)positions.size(); i++) {
					if (others[relative_id].discardable[i] == kUnknown || others[relative_id].discardable[i] == kForcedUnknown) {
						others[relative_id].discardable[i] = kNo;
						break;
					}
				}
			}
		}
		// check hinter's other possible moves
		if (hinter_id != agent_id) {
			int relative_id = (hinter_id - agent_id - 1 + num_players) % num_players;
			if (!others[relative_id].plays.empty() ||
					find(others[relative_id].discardable.begin(), others[relative_id].discardable.end(), kYes) != others[relative_id].discardable.end()) {
				if (relative_id == 0) {
					// the next player hinted when that player has a play or discard, so my new card is not discardable.
					if (discardable.back() == kUnknown) {
						discardable.back() = kNo;
					}
				} else {
					if (others[relative_id - 1].discardable.back() == kUnknown) {
						others[relative_id - 1].discardable.back() = kNo;
					}
				}
			} else {
				if (relative_id == 0) {
					// we have no information here.
					if (discardable.back() == kUnknown) {
						discardable.back() = kForcedUnknown;
					}
				} else {
					if (others[relative_id - 1].discardable.back() == kUnknown) {
						others[relative_id - 1].discardable.back() = kForcedUnknown;
					}
				}
			}
		}
	}
	void play() {
		// Play the "best" move except for giving a hint to the previous player
		// if the previous player's last card (if unknown) is discardable.
		// Otherwise, play something that is not the "best" from the previous player's view,
		// (TODO: or give a hint to the previous player).
		// "Best" is defined as play > discard > give hint.
		// If no useful hint is available, discard the first card that is not undiscardable instead.
		auto try_to_discard_the_last_discardable_card = [&] () {
			int discard_position = -1;
			for (int i = (int)discardable.size() - 1; i >= 0; i--) {
				if (discardable[i] == kYes) {
					discard_position = i;
					break;
				}
			}
			if (discard_position != -1) {
				// discard a card
				bool draw = (drawn_cards < total_cards);
				agent_discard(agent_id, discard_position);
				discardable.erase(discardable.begin() + discard_position);
				auto it = find(plays.begin(), plays.end(), discard_position);
				if (it != plays.end()) {
					plays.erase(it);
				}
				for (auto &play_pos : plays) {
					if (play_pos > discard_position) {
						play_pos--;
					}
				}
				if (draw) {
					discardable.push_back(kUnknown);
				}
				return true;
			}
			return false;
		};
		auto try_to_give_a_playable_hint = [&] () {
			for (int relative_id = 0; relative_id < num_players - 1; relative_id++) {
				for (int i = 0; i < (int)others[relative_id].cards.size(); i++) {
					if (find(others[relative_id].plays.begin(), others[relative_id].plays.end(), i) == others[relative_id].plays.end()) {
						if (others_card_playable(relative_id, i)) {
							// try to give a number hint -- need this card to be the first in the same number
							int number = others[relative_id].cards[i] % kNumNumbers;
							bool hint_ok = true;
							for (int j = 0; j < i; j++) {
								if (others[relative_id].cards[j] % kNumNumbers == number) {
									if (find(others[relative_id].plays.begin(), others[relative_id].plays.end(), i) == others[relative_id].plays.end()) {
										hint_ok = false;
										break;
									}
								}
							}
							if (hint_ok) {
								vector<int> hint_positions;
								for (int j = 0; j < (int)others[relative_id].cards.size(); j++) {
									if (others[relative_id].cards[j] % kNumNumbers == number) {
										hint_positions.push_back(j);
									}
								}
								agent_hint(agent_id, (agent_id + relative_id + 1) % num_players, -1, number, hint_positions);
								return true;
							}
							// try to give a suit hint -- need this card to be the first in the same suit
							int this_suit = others[relative_id].cards[i] / kNumNumbers;
							for (int suit = 1; suit < kNumSuits; suit++) {
								if (this_suit != 0 && suit != this_suit) {
									// this card is not multicolor, we don't need this for loop
									continue;
								}
								// try to give |suit| hint
								hint_ok = true;
								for (int j = 0; j < i; j++) {
									if (others[relative_id].cards[j] / kNumNumbers == suit || others[relative_id].cards[j] / kNumNumbers == 0) {
										if (find(others[relative_id].plays.begin(), others[relative_id].plays.end(), i) == others[relative_id].plays.end()) {
											hint_ok = false;
											break;
										}
									}
								}
								if (hint_ok) {
									vector<int> hint_positions;
									for (int j = 0; j < (int)others[relative_id].cards.size(); j++) {
										if (others[relative_id].cards[j] / kNumNumbers == suit || others[relative_id].cards[j] / kNumNumbers == 0) {
											hint_positions.push_back(j);
										}
									}
									agent_hint(agent_id, (agent_id + relative_id + 1) % num_players, suit, -1, hint_positions);
									return true;
								}
							}
						}
					}
				}
			}
			return false;
		};
		auto try_to_give_a_not_discardable_hint = [&] () {
			for (int relative_id = 0; relative_id < num_players - 1; relative_id++) {
				for (int i = 0; i < (int)others[relative_id].cards.size(); i++) {
					if ((others[relative_id].discardable[i] == kUnknown || others[relative_id].discardable[i] == kForcedUnknown) &&
							!others_card_discardable(relative_id, i)) {
						// only possible to give the hint when it is impossible to play
						// TODO: while considering multicolor cards, figure out when a suit hint is impossible to play
						bool possible_to_give_hint = true;
						int number = others[relative_id].cards[i] % kNumNumbers;
						for (int j = 0; j < kNumSuits; j++) {
							if (board[j] == number) {
								possible_to_give_hint = false;
								break;
							}
						}
						if (!possible_to_give_hint)
							continue;
						bool hint_ok = true;
						for (int j = 0; j < i; j++) {
							if (others[relative_id].cards[j] % kNumNumbers == number) {
								if (others[relative_id].discardable[j] == kUnknown || others[relative_id].discardable[j] == kForcedUnknown) {
									hint_ok = false;
									break;
								}
							}
						}
						if (hint_ok) {
							vector<int> hint_positions;
							for (int j = 0; j < (int)others[relative_id].cards.size(); j++) {
								if (others[relative_id].cards[j] % kNumNumbers == number) {
									hint_positions.push_back(j);
								}
							}
							agent_hint(agent_id, (agent_id + relative_id + 1) % num_players, -1, number, hint_positions);
							return true;
						}
					}
				}
			}
			return false;
		};
		auto discard_first_unknown = [&] () {
			if (discardable.empty()) {
				printf("Player %d has no cards to discard.\n", agent_id + 1);
				printf("Game failed. Total score = %d\n", total_score);
				exit(0);
			}
			int discard_position = -1;
			for (int i = 0; i < (int)discardable.size(); i++) {
				if (discardable[i] == kUnknown || discardable[i] == kForcedUnknown) {
					discard_position = i;
					break;
				}
			}
			if (discard_position == -1) {
				printf("Player %d knows that all cards in their hand are not discardable, but still needs to discard one.\n", agent_id + 1);
				discard_position = 0;
			}
			// discard a card
			bool draw = (drawn_cards < total_cards);
			agent_discard(agent_id, discard_position);
			discardable.erase(discardable.begin() + discard_position);
			auto it = find(plays.begin(), plays.end(), discard_position);
			if (it != plays.end()) {
				plays.erase(it);
			}
			for (auto &play_pos : plays) {
				if (play_pos > discard_position) {
					play_pos--;
				}
			}
			if (draw) {
				discardable.push_back(kUnknown);
			}
		};
		if (others.back().discardable.back() == kUnknown && !others_card_discardable(num_players - 2, (int)others.back().discardable.size() - 1)) {
			// play "second best"
			if (!plays.empty()) {
				// do not play a card
				others.back().discardable.back() = kNo;
				if (!try_to_discard_the_last_discardable_card()) {
					// give a hint
					if (!try_to_give_a_playable_hint() && !try_to_give_a_not_discardable_hint()) {
						discard_first_unknown();
					}
				}
			} else if (find(discardable.begin(), discardable.end(), kYes) != discardable.end()) {
				// do not discard a card
				others.back().discardable.back() = kNo;
				// give a hint
				if (!try_to_give_a_playable_hint() && !try_to_give_a_not_discardable_hint()) {
					discard_first_unknown();
				}
			} else {
				others.back().discardable.back() = kForcedUnknown;
				// no information, just give a hint
				if (!try_to_give_a_playable_hint() && !try_to_give_a_not_discardable_hint()) {
					discard_first_unknown();
				}
			}
		} else if (!plays.empty()) {
			// play a card
			int position = plays.front();
			bool draw = (drawn_cards < total_cards);
			agent_play(agent_id, position);
			plays.erase(plays.begin());
			discardable.erase(discardable.begin() + position);
			for (auto &play_pos : plays) {
				if (play_pos > position) {
					play_pos--;
				}
			}
			if (draw) {
				discardable.push_back(kUnknown);
			}
		} else if (!try_to_discard_the_last_discardable_card()) {
			others.back().discardable.back() = kForcedUnknown;
			if (!try_to_give_a_playable_hint() && !try_to_give_a_not_discardable_hint()) {
				discard_first_unknown();
			}
		}
	}
};

vector<Agent> agents;
vector<int> cards;
vector<vector<int>> player_cards;
int hint_token;
int life_token;

void increase_hint() {
	if (++hint_token > kMaxHintToken) {
		hint_token = kMaxHintToken;
		printf("Hint tokens not increased.\n");
	} else {
		printf("Increased hint tokens to %d.\n", hint_token);
	}
}

bool agent_play(int agent_id, int position) {
	printf("Player %d plays card %d: %c%d.\n", agent_id + 1, position + 1, player_cards[agent_id][position] / kNumNumbers + 'A', player_cards[agent_id][position] % kNumNumbers + 1);
	bool valid_play = true;
	if (board[player_cards[agent_id][position] / kNumNumbers] != player_cards[agent_id][position] % kNumNumbers) {
		life_token--;
		printf("Invalid play. Life tokens decreased to %d.\n", life_token);
		if (life_token < 0) {
			printf("Game failed. Total score = %d\n", total_score);
			exit(0);
		}
		valid_play = false;
	}
	if (valid_play) {
		++total_score;
		if (++board[player_cards[agent_id][position] / kNumNumbers] == kNumNumbers) {
			increase_hint();
		}
	}
	printf("Total score = %d. Current board is:", total_score);
	for (int i = 0; i < kNumSuits; i++) {
		printf(" %c%d", i + 'A', board[i]);
	}
	printf("\n");
	player_cards[agent_id].erase(player_cards[agent_id].begin() + position);
	int new_card = -1;
	if (drawn_cards < total_cards) {
		new_card = cards[drawn_cards++];
		player_cards[agent_id].push_back(new_card);
		printf("Player %d draws a card. The hand is:", agent_id + 1);
		for (int j = 0; j < num_cards; j++) {
			printf(" %c%d", player_cards[agent_id][j] / kNumNumbers + 'A', player_cards[agent_id][j] % kNumNumbers + 1);
		}
		printf("\n");
	}
	for (int i = 0; i < num_players; i++) {
		if (i != agent_id) {
			agents[i].update_other_play(agent_id, position, new_card);
		}
	}
	return valid_play;
}

bool agent_discard(int agent_id, int position) {
	printf("Player %d discards card %d: %c%d.\n", agent_id + 1, position + 1, player_cards[agent_id][position] / kNumNumbers + 'A', player_cards[agent_id][position] % kNumNumbers + 1);
	if (++num_discarded[player_cards[agent_id][position] / kNumNumbers][player_cards[agent_id][position] % kNumNumbers] == kNumbers[player_cards[agent_id][position] % kNumNumbers]) {
		printf("The game cannot win after this discard.\n");
		// assert(false);
		// return false;
	}
	increase_hint();
	player_cards[agent_id].erase(player_cards[agent_id].begin() + position);
	int new_card = -1;
	if (drawn_cards < total_cards) {
		new_card = cards[drawn_cards++];
		player_cards[agent_id].push_back(new_card);
		printf("Player %d draws a card. The hand is:", agent_id + 1);
		for (int j = 0; j < num_cards; j++) {
			printf(" %c%d", player_cards[agent_id][j] / kNumNumbers + 'A', player_cards[agent_id][j] % kNumNumbers + 1);
		}
		printf("\n");
	}
	for (int i = 0; i < num_players; i++) {
		if (i != agent_id) {
			agents[i].update_other_discard(agent_id, position, new_card);
		}
	}
	return true;
}

bool agent_hint(int agent_id, int hinted_agent_id, int suit, int number, const vector<int> &positions) {
	if (--hint_token < 0) {
		// we don't restrict the number of hint tokens for now
		printf("Decreased hint tokens to %d.\n", hint_token);
	} else {
		printf("Decreased hint tokens to %d.\n", hint_token);
	}
	printf("Player %d hints player %d: card", agent_id + 1, hinted_agent_id + 1);
	if (positions.size() == 1) {
		printf(" %d is of ", positions[0] + 1);
	} else {
		printf("s {");
		for (int i = 0; i < (int)positions.size() - 1; i++) {
			printf("%d, ", positions[i] + 1);
		}
		printf("%d} are of ", positions.back() + 1);
	}
	if (suit != -1) {
		printf("suit %c.\n", suit + 'A');
	} else {
		printf("number %d.\n", number + 1);
	}
	for (int i = 0; i < num_players; i++) {
		agents[i].update_hint(agent_id, hinted_agent_id, suit, number, positions);
	}
	return true;
}

bool check_win() {
	for (int i = 0; i < kNumSuits; i++) {
		if (board[i] != kNumNumbers) {
			return false;
		}
	}
	return true;
}

void init() {
	memset(board, 0, sizeof(board));
	memset(num_discarded, 0, sizeof(num_discarded));
	drawn_cards = 0;
	hint_token = kInitHintToken;
	life_token = kInitLifeToken;
	cards.clear();
	for (int i = 0; i < kNumSuits; i++) {
		for (int j = 0; j < kNumNumbers; j++) {
			for (int k = 0; k < kNumbers[j]; k++) {
				cards.push_back(i * kNumNumbers + j);
			}
		}
	}
	total_cards = (int)cards.size();
	total_score = 0;
	random_shuffle(cards.begin(), cards.end());
	player_cards.assign(num_players, vector<int>());
	for (int i = 0; i < num_players; i++) {
		printf("Player %d initial cards:", i + 1);
		for (int j = 0; j < num_cards; j++) {
			player_cards[i].push_back(cards[drawn_cards++]);
			printf(" %c%d", player_cards[i][j] / kNumNumbers + 'A', player_cards[i][j] % kNumNumbers + 1);
		}
		printf("\n");
	}
	agents.clear();
	for (int i = 0; i < num_players; i++) {
		vector<vector<int>> other_player_cards;
		other_player_cards.insert(other_player_cards.end(), player_cards.begin() + i + 1, player_cards.end());
		other_player_cards.insert(other_player_cards.end(), player_cards.begin(), player_cards.begin() + i);
		agents.emplace_back(i, other_player_cards);
	}
}

void run_game() {
	int current_player = 0;
	while (true) {
		agents[current_player].play();
		if (check_win()) {
			printf("Win!\n");
			break;
		}
		current_player = (current_player + 1) % num_players;
		for (int i = 0; i < num_players; i++) {
			agents[i].print_self_info();
		}
	}
}

int main() {
	init();
	run_game();
	return 0;
}
