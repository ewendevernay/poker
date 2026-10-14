#include "basic.c"

#include "poker.h"
#include "raylib.h"

#include <math.h>
#include <time.h>
#define CRASH(error) do { printf("%s:%d: %s\n", __FILE__, __LINE__, error); abort(); } while (0)

#ifndef M_PI
#define M_PI 3.1415f
#endif

#define DEFAULT_SERVER_PORT 6966

static int mouse_x;
static int mouse_y;

static int screen_width;
static int screen_height;

typedef struct {
 enum {
  C_SPADES = 0,
  C_CLUBS,
  C_HEARTS,
  C_DIAMONDS,
 } color;
 
 enum {
  C_EMPTY = 0,
  C_1,
  C_2,
  C_3,
  C_4,
  C_5,
  C_6,
  C_7,
  C_8,
  C_9,
  C_10,
  C_JACK,
  C_QUEEN,
  C_KING
 } number;
} Card;

typedef struct {
 Card deck[52];
 uint32_t deck_count;

 Card common_cards[5];
 uint32_t common_cards_count;

 uint32_t turn;
} PokerState;

static void poker_state_init(PokerState *state) {
 state->common_cards_count = 0;
 state->deck_count = 0;
 state->turn = 0;

 for (uint32_t color = C_SPADES; color <= C_DIAMONDS; color++) {
  for (uint32_t number = C_1; number <= C_KING; number++) {
   state->deck[state->deck_count++] = (Card) { .color = color, .number = number };
  }
 }

 ba_assert(state->deck_count == 52);

 // Shuffling the cards
 for (uint32_t i = 0; i < state->deck_count; i++) {
  Card tmp;
  uint32_t j = rand() % state->deck_count;

  tmp = state->deck[i];
  state->deck[i] = state->deck[j];
  state->deck[j] = tmp;
 }

 // Drawing the cards   
 for (uint32_t i = 0; i < 3 + 1 + 1; i++) {
  state->common_cards[i] = state->deck[--state->deck_count];
 }
}

static void poker_get_cards(PokerState *state, Card *cards) {
 ba_assert(state->deck_count >= 2);

 cards[0] = state->deck[state->deck_count - 1];
 cards[1] = state->deck[state->deck_count - 2];

 state->deck_count -= 2;
}

static bool8_t poker_draw_cards(PokerState *state) { // Returns if it's the last turn
 if (state->turn >= 3) return true;

 static uint32_t cards_per_turn[] = { 3, 1, 1 };
 uint32_t count = cards_per_turn[state->turn];
 ba_assert(state->deck_count >= count);

 state->common_cards_count += cards_per_turn[state->turn];
                
 state->turn++;

 return false;
}

static uint32_t poker_card_kicker(Card card) {
 return card.number == 1 ? 14 : card.number;
}

static bool8_t poker_successive_cards(Card *total_cards, uint32_t total_cards_count, uint32_t duplicates, uint8_t *already_taken, Card *last_card, uint32_t old_number) {
 uint32_t four_number = 0;
 uint32_t count = 0;
 uint32_t last_i = 0;

 bool8_t is_four = false;

 for (uint32_t i = 0; i < total_cards_count; i++) {
  if (total_cards[i].number == old_number) continue;

  if (total_cards[i].number != four_number) {
   count = 0;
   four_number = total_cards[i].number;
   last_i = i;
  } else {
   count++;
   if (count == duplicates - 1 &&
       (i == total_cards_count - 1 ||
        total_cards[i + 1].number != total_cards[i].number))
   {
    *last_card = total_cards[i];
    is_four = true;
    for (uint32_t j = last_i; j <= i; j++) {
     already_taken[j] = true;
    }

    if (four_number == 1) break;
   }
  }
 }

 return is_four;
}

static uint32_t mod(int32_t a, int32_t b) {
 int32_t r = a % b;
 return r < 0 ? r + b : r;
}

static bool8_t poker_straight(Card *total_cards, uint32_t total_cards_count, bool8_t *already_taken, Card *best_card) {
 int32_t i = 0;

 for (; total_cards[i].number == C_1; i++);
 i--;

 bool8_t is_straight = false;
 uint32_t count = 0;
        
 do {
  int32_t ind = mod(i, total_cards_count);
  i--;
                
  int32_t next_ind = mod(ind - 1, total_cards_count);
                
  if (total_cards[next_ind].number == total_cards[ind].number) continue;

  if (total_cards[next_ind].number + 1 == total_cards[ind].number ||
      (total_cards[next_ind].number == C_KING && total_cards[ind].number == C_1))
  {
   if (count == 0) *best_card = total_cards[ind];
   count++;

   already_taken[ind] = true;
   already_taken[next_ind] = true;
                        
   if (count >= 4) {
    is_straight = true;
    break;
   }
  } else {
   count = 0;

   for (uint32_t i = 0; i < total_cards_count; i++)
    already_taken[i] = false;
  }
 } while (i > -(int32_t)(total_cards_count));

 return is_straight;
}

static bool8_t poker_straight_flush(Card *total_cards, uint32_t total_cards_count, bool8_t *already_taken, Card *best_card) {
 Card cards_by_color[(5 + 2) * 4] = {};
 uint32_t cards_by_color_counts[4] = {};
        
 for (uint32_t i = 0; i < total_cards_count; i++) {
  uint32_t color = total_cards[i].color;

  cards_by_color[(5 + 2) * color + cards_by_color_counts[color]] = total_cards[i];
  cards_by_color_counts[color] += 1;
 }

 bool8_t already_taken_color[5 + 2] = {};
 memzero(best_card);
 uint32_t best_color = 0;
 bool8_t is_straight = false;
        
 for (uint32_t color = 0; color < 4; color++) {
  if (cards_by_color_counts[color] >= 5) {
   for (uint32_t i = 0; i < 5 + 2; i++) {
    already_taken_color[i] = false;
   }
                
   is_straight = poker_straight(&cards_by_color[(5 + 2) * color], cards_by_color_counts[color], already_taken_color, best_card);

   if (is_straight) {
    best_color = color;
    break;
   }
  }
 }

 if (is_straight) {
  for (uint32_t i = 0; i < cards_by_color_counts[best_color]; i++) {
   if (already_taken_color[i]) {
    for (uint32_t j = 0; j < total_cards_count; j++) {
     Card c1 = total_cards[j];
     Card c2 = cards_by_color[(5 + 2) * best_color + i];
                                
     if (c1.number == c2.number && c1.color == c2.color) {
      already_taken[j] = true;
      break;
     }
    }
   }
  }
 }
        
 return is_straight;
}

static uint32_t poker_hand_score(Card *common_cards, Card *cards) {
 Card total_cards[5 + 2];

 memcpy(total_cards, common_cards, sizeof (Card) * 5);
 memcpy(total_cards + 5, cards, sizeof (Card) * 2);

 for (uint32_t i = 0; i < ARRAY_COUNT(total_cards); i++) { // Sort
  uint32_t max_ind = i;
  for (uint32_t j = i + 1; j < ARRAY_COUNT(total_cards); j++) {
   if (total_cards[j].number < total_cards[max_ind].number) {
    max_ind = j;
   }
  }

  Card tmp = total_cards[i];
  total_cards[i] = total_cards[max_ind];
  total_cards[max_ind] = tmp;
 }

 bool8_t already_taken_four[5 + 2] = {};
 bool8_t already_taken_three[5 + 2] = {};
 bool8_t already_taken_pair[5 + 2] = {};
 bool8_t already_taken_second_pair[5 + 2] = {};
 bool8_t already_taken_straight[5 + 2] = {};
 bool8_t already_taken_straight_flush[5 + 2] = {};
 bool8_t already_taken_flush[5 + 2] = {};

 bool8_t is_straight = false;
 bool8_t is_straight_flush = false;

 bool8_t is_four = false;
 bool8_t is_flush = false;
 bool8_t is_full_house = false;
 bool8_t is_three = false;
 bool8_t is_pair = false;
 bool8_t is_two_pairs = false;

 // Checking for four of a kind
 Card four_last_card = {};
 is_four = poker_successive_cards(total_cards, ARRAY_COUNT(total_cards), 4, already_taken_four, &four_last_card, 0);

 // Checking three
 Card three_last_card = {};
 is_three = poker_successive_cards(total_cards, ARRAY_COUNT(total_cards), 3, already_taken_three, &three_last_card, 0);

 // Checking pairs
 Card pair_last_card = {};
 is_pair = poker_successive_cards(total_cards, ARRAY_COUNT(total_cards), 2, already_taken_pair, &pair_last_card, 0);

 Card second_pair_last_card = {};
 if (is_pair && !is_three) {
  is_two_pairs = poker_successive_cards(total_cards, ARRAY_COUNT(total_cards), 2, already_taken_second_pair, &second_pair_last_card, pair_last_card.number);
 }

 // Checking for straight and straight flush
 Card straight_last_card = {};
 is_straight = poker_straight(total_cards, ARRAY_COUNT(total_cards), already_taken_straight, &straight_last_card);

 Card straight_flush_last_card = {};
 is_straight_flush = poker_straight_flush(total_cards, ARRAY_COUNT(total_cards), already_taken_straight_flush, &straight_flush_last_card);
        
 uint32_t color_counts[4] = {};
 Card colors_last_cards[4] = {};

 for (uint32_t i = 0; i < ARRAY_COUNT(total_cards); i++) {
  color_counts[total_cards[i].color] += 1;
  if (colors_last_cards[total_cards[i].color].number != C_1) {
   colors_last_cards[total_cards[i].color] = total_cards[i];
  }
 }

 Card flush_last_card = {};
 for (uint32_t color = 0; color < 4; color++) {
  if (color_counts[color] >= 5) {
   flush_last_card = colors_last_cards[color];
   is_flush = true;
   break;
  }
 }

 if (is_flush) { // Calculate the used cards
  int32_t j = 0;
  for (uint32_t i = 0; i < ARRAY_COUNT(total_cards); i++) {
   if (total_cards[i].color == flush_last_card.color && total_cards[i].number == C_1) {
    already_taken_flush[i] = true;
    j = 1;
    break;
   }
  }

  for (int32_t i = ARRAY_COUNT(total_cards) - 1; i >= 0 && j < 5; i--) {
   if (total_cards[i].color == flush_last_card.color && total_cards[i].number != C_1) {
    already_taken_flush[i] = true;
    j++;
   }
  }
 }
        
 Card best_card = {};

 bool8_t already_taken_empty[5 + 2] = {};
        
 bool8_t *already_taken = 0;

 if (is_straight_flush) {
  already_taken = already_taken_straight_flush;
 } else if (is_four) {
  already_taken = already_taken_four;
 } else if (is_three && is_pair) {
  for (uint32_t i = 0; i < ARRAY_COUNT(total_cards); i++) {
   if (already_taken_pair[i]) {
    already_taken_three[i] = true;
   }
  }

  already_taken = already_taken_three;
 } else if (is_flush) {
  already_taken = already_taken_flush;
 } else if (is_straight) {
  already_taken = already_taken_straight;
 } else if (is_three) {
  already_taken = already_taken_three;
 } else if (is_two_pairs) {
  for (uint32_t i = 0; i < ARRAY_COUNT(total_cards); i++) {
   if (already_taken_pair[i]) {
    already_taken_second_pair[i] = true;
   }
  }

  already_taken = already_taken_second_pair;
 } else if (is_pair) {
  already_taken = already_taken_pair;
 } else {
  already_taken = already_taken_empty;
 }
        
 for (int32_t i = ARRAY_COUNT(total_cards) - 1; i >= 0; i--) {
  if (!already_taken[i]) {
   if (total_cards[i].number == C_1) {
    best_card = total_cards[i];
    break;
   }

   if (total_cards[i].number > best_card.number) {
    best_card = total_cards[i];
   }
  }
 }

 if (is_straight && is_straight_flush) {
  return 8000000 + poker_card_kicker(straight_last_card) * 10000;
 } else if (is_four) {
  return 7000000 + poker_card_kicker(four_last_card) * 10000 + poker_card_kicker(best_card);
 } else if (is_three && is_pair) {
  return (6000000 + poker_card_kicker(three_last_card) * 10000 + poker_card_kicker(pair_last_card) * 100 + poker_card_kicker(best_card));
 } else if (is_flush) {
  return 5000000 + poker_card_kicker(flush_last_card) * 10000;
 } else if (is_straight) {
  return 4000000 + poker_card_kicker(straight_last_card) * 10000;
 } else if (is_three) {
  return 3000000 + poker_card_kicker(three_last_card) * 10000 + poker_card_kicker(best_card);
 } else if (is_two_pairs) {
  return (2000000 + poker_card_kicker(pair_last_card) * 10000 + poker_card_kicker(second_pair_last_card) * 100 + poker_card_kicker(best_card));
 } else if (is_pair) {
  return 1000000 + poker_card_kicker(pair_last_card) * 10000 + poker_card_kicker(best_card);
 } else {
  return poker_card_kicker(best_card);
 }
}

#define BLIND_PRICE 10
#define MAX_RAISE 1000000

#define MAX_PLAYERS_COUNT 1024
#define MAX_CONNECTED_PLAYERS_COUNT 10
#define MAX_TURNS_COUNT 16
#define PLAYER_NAME_MAX_LEN 12
#define DISTANT_IP_MAX_LEN 64

#define PLAYER_DEFAULT_BALANCE 1000

typedef enum {
 P_ACK = 0,
 P_HELLO,
 P_BYE,
 P_WELCOME,
 P_SHUTDOWN,
 P_START,
 P_GAME_BEGIN,
 P_PLAYERS_WAITING,
 P_PLAYERS_PLAYING,
 P_NEW_PLAYER,
 P_CALL,
 P_FOLD,
 P_RAISE,
 P_PLAYED,
 P_OVER_FORFEIT,
 P_OVER_WIN
} PacketType;

typedef struct {
 String name;

 PkAddr address;
        
 int32_t balance;
 int32_t hand_balance;
 
 Card cards[2];
 uint32_t highest_bet;

 uint32_t pot_balance;
        
 uint32_t pot_players[MAX_CONNECTED_PLAYERS_COUNT];
 uint32_t pot_players_count;
        
 uint32_t all_in_turn;

 uint32_t hand_score;

 int32_t global_score;
        
 enum player_flags {
  P_EXISTS = 1 << 0,
  P_CONNECTED = 1 << 1,
  P_PLAYING = 1 << 2,
  P_ALL_IN = 1 << 3,
  P_ALL_IN_FIRST_TURN = 1 << 4
 } flags;
} Player;

ba_static_assert(sizeof(enum player_flags) == 4, player_flags_is_u8);

typedef uint32_t PlayerID;

#define PACKET_MAX_SIZE 1400//16384

typedef struct {
 uint8_t packet_buffer[PACKET_MAX_SIZE];

 PkSock *socket;

 uint32_t players_count;
 Player players[MAX_PLAYERS_COUNT];

 uint32_t pot_balance;
 uint32_t highest_bet;

 uint32_t pot_last_turns[5];

 PlayerID player_order[MAX_PLAYERS_COUNT];
 uint32_t player_order_index;
 uint32_t player_order_count;
        
 bool8_t is_turn_over;

 PokerState poker_state;

 enum {
  G_WAITING = 0,
  G_PLAYING
 } game_state;
} ServerState;

typedef struct {
 uint8_t packet_buffer[PACKET_MAX_SIZE];

 PkSock *socket;
 PkAddr distant_server_addr;

 String username;
 PlayerID player_id;

 uint32_t players_count;
 Player players[MAX_PLAYERS_COUNT];

 PlayerID player_order[MAX_PLAYERS_COUNT];
 uint32_t player_order_index;
 uint32_t player_order_count;

 uint32_t pot_balance;
 uint32_t highest_bet;

 Card common_cards[5];
 uint32_t common_cards_count;

 PlayerID winning_player_id;
 PlayerID winning_players_ids[MAX_PLAYERS_COUNT];
 uint32_t winning_players_count;

 enum {
  CLIENT_SENT_HELLO = 1 << 0,
  CLIENT_PLAYER_DEAD = 1 << 1
 } flags;

 enum {
  CLIENT_STATE_WAITING = 0,
  CLIENT_STATE_PLAYING,
  CLIENT_STATE_WATCHING,
  CLIENT_STATE_GAME_OVER_FORFEIT,
  CLIENT_STATE_GAME_OVER_WIN
 } state;
} ClientState;

static ServerState server_state;
static ClientState client_state;

static bool8_t address_equal(PkAddr *a, PkAddr *b) {
 return memcmp(a, b, sizeof (PkAddr)) == 0;
}

#define PACKET_CLIENT_HEADER_SIZE 1
#define PACKET_SERVER_HEADER_SIZE 1

static Pool main_pool;
static Pool frame_pool;

static uint8_t send_packet_buffer[PACKET_MAX_SIZE];

static uint64_t client_get_packet(uint8_t *packet, uint32_t packet_size) {
 PkAddr src_addr;
 uint64_t bytes_count;
 uint64_t bytes_received = 0;

 NBResult res = receive_packet(client_state.socket, packet, packet_size, &src_addr, &bytes_count);
 if (res == R_READY &&
     address_equal(&src_addr, &client_state.distant_server_addr) &&
     bytes_count >= PACKET_CLIENT_HEADER_SIZE)
 {
  bytes_received = bytes_count;
 }

 return bytes_received;
}

static void client_send_packet(uint8_t *packet, uint32_t packet_size) {
 uint64_t bytes_sent;
 NBResult res = send_packet(client_state.socket, &client_state.distant_server_addr, packet, packet_size, &bytes_sent);

 if (res != R_READY || bytes_sent != packet_size) {
  CRASH("Failed to send packet");
 }
}

static uint64_t server_get_packet(uint8_t *packet, uint32_t packet_size, PkAddr *src_addr) {
 uint64_t bytes_count;
 uint64_t bytes_received = 0;

 NBResult res = receive_packet(server_state.socket, packet, packet_size, src_addr, &bytes_count);
 if (res == R_READY && bytes_count >= PACKET_SERVER_HEADER_SIZE) {
  bytes_received = bytes_count;
 }

 return bytes_received;
}

static void server_send_packet(PlayerID player_id, uint8_t *packet, uint32_t packet_size) {
 Player *player = &server_state.players[player_id];
 if (player->flags & P_CONNECTED) {
  uint64_t bytes_sent;
  NBResult res = send_packet(server_state.socket, &player->address, packet, packet_size, &bytes_sent);
  if (res != R_READY || bytes_sent != packet_size) {
   CRASH("Failed to send packet");
  }
 } else {
  CRASH("Sending data to not-connected player");
 }
}

static void server_init() {
 for (uint32_t i = 0; i < server_state.players_count; i++) {
  if (server_state.players[i].flags & P_EXISTS) {
   server_state.players[i].flags = P_EXISTS;
  }
 }

 server_state.game_state = G_WAITING;
}

static uint32_t players_array_count(Player *players, uint32_t players_count, uint32_t mask) {
 uint32_t count = 0;
 for (uint32_t p_id = 1; p_id < players_count; p_id++) {
  if ((players[p_id].flags & mask) == mask) {
   count++;
  }
 }

 return count;
}

static uint32_t server_players_count(uint32_t mask) {
 return players_array_count(server_state.players, server_state.players_count, mask);
}

static void disconnect() {
 if (server_state.socket) {
  for (uint32_t p_id = 1; p_id < server_state.players_count; p_id++) {
   if (p_id != client_state.player_id && server_state.players[p_id].flags & P_CONNECTED) {
    uint8_t packet = P_SHUTDOWN;

    server_send_packet(p_id, &packet, 1);
    server_state.players[p_id].flags = P_EXISTS;
   }
  }

  pk_socket_close(server_state.socket);
  server_state.socket = 0;
 } else if (client_state.socket) {
  if (client_state.player_id) {
   se_begin(send_packet_buffer, PACKET_MAX_SIZE);

   se_write_u8(P_BYE);
   se_write_u32(client_state.player_id);

   uint64_t packet_size = 0;
   se_end(&packet_size);

   client_send_packet(send_packet_buffer, packet_size);
  }
 }
}

static void se_write_player(PlayerID p_id, Player *player) {
 se_write_u32(p_id);
 se_write_string(player->name);
 se_write_s32(player->hand_balance);
 se_write_u32(player->flags);
 se_write_s32(player->global_score);
}

static void se_write_server_player_order() {
 se_write_u32(server_state.player_order_count);
 for (uint32_t i = 0; i < server_state.player_order_count; i++) {
  se_write_u32(server_state.player_order[i]);
 }
}

static void server_send_to_connected(uint8_t *packet, uint64_t packet_size) {
 for (uint32_t p_id = 1; p_id < server_state.players_count; p_id++) {
  if (server_state.players[p_id].flags & P_CONNECTED) {
   server_send_packet(p_id, send_packet_buffer, packet_size);
  }
 }
}

static uint32_t server_active_players_count() {
  return server_players_count(P_PLAYING) - server_players_count(P_ALL_IN | P_PLAYING);
}

static void server_next_turn(bool8_t not_increase_turn) {
 uint32_t player_id = server_state.player_order[server_state.player_order_index];

 bool8_t is_finished = false;

 se_begin(send_packet_buffer, PACKET_MAX_SIZE);

 if (server_players_count(P_PLAYING) == 1) {
  is_finished = true;
                
  // Win by forfeit
  Player *last_player = 0;
  uint32_t last_player_id = 0;
  for (last_player_id = 1; last_player_id < server_state.players_count; last_player_id++) {
   if (server_state.players[last_player_id].flags & P_PLAYING) {
    last_player = &server_state.players[last_player_id];
    break;
   }
  }

  last_player->hand_balance += server_state.pot_balance;

  server_state.game_state = G_WAITING;

  se_write_u8(P_OVER_FORFEIT);
  se_write_u32(last_player_id);
  se_write_s32(last_player->hand_balance);
 } else if (!not_increase_turn) {
  bool8_t is_next_turn = false;
  bool8_t is_everybody_allin = false;
  
  Player *player = 0;
  
  if (server_active_players_count() > 0) {
   do { // Adavancing the current player index
    server_state.player_order_index = ((server_state.player_order_index + 1) %
                                       server_state.player_order_count);

    if (server_state.player_order_index == 0) {
     server_state.is_turn_over = 1;
    }

    player = &server_state.players[server_state.player_order[server_state.player_order_index]];
   } while (!(player->flags & P_PLAYING) || (player->flags & P_ALL_IN));
  } else {
   is_everybody_allin = true;
  }
                
  if (is_everybody_allin || (server_state.is_turn_over && player->highest_bet == server_state.highest_bet)) {
   // Next turn
   is_next_turn = true;

   server_state.is_turn_over = 0;
   server_state.player_order_index = 0;

   server_state.highest_bet = 0;

   uint32_t pot_this_turn = server_state.pot_balance;
   for (uint32_t i = 0; i < server_state.poker_state.turn; i++) {
    pot_this_turn -= server_state.pot_last_turns[i];
   }
                                
   server_state.pot_last_turns[server_state.poker_state.turn] = pot_this_turn;

   if (server_active_players_count() <= 1 || poker_draw_cards(&server_state.poker_state)) {
    is_finished = true;
   } else {
    Player *p = &server_state.players[server_state.player_order[server_state.player_order_index]];
    while (!(p->flags & P_PLAYING) || (p->flags & P_ALL_IN)) { // Selecting first player
     server_state.player_order_index++;
     p = &server_state.players[server_state.player_order[server_state.player_order_index]];
    }

    // Hand not over, resetting the players' bets
    for (uint32_t p_id = 1; p_id < server_state.players_count; p_id++) {
     if (server_state.players[p_id].flags & P_PLAYING) {
      server_state.players[p_id].highest_bet = 0;
      server_state.players[p_id].flags &= ~P_ALL_IN_FIRST_TURN;
     }
    }
   }
  }

  // Send next turn to clients
  se_begin(send_packet_buffer, PACKET_MAX_SIZE);

  if (is_finished) {
   // Cards reveal
   server_state.game_state = G_WAITING;

   // Calculating scores
   for (uint32_t p_id = 1; p_id < server_state.players_count; p_id++) {
    Player *p = &server_state.players[p_id];
    if (p->flags & (P_ALL_IN | P_PLAYING)) {
     p->hand_score = poker_hand_score(server_state.poker_state.common_cards, p->cards);
    }
   }
                        
   // Pot distribution
   for (uint32_t pot_id = 1; pot_id < server_state.players_count; pot_id++) {
    if ((server_state.players[pot_id].flags & P_ALL_IN)) {
     uint32_t owning_players[MAX_CONNECTED_PLAYERS_COUNT];
     uint32_t owning_players_count = 0;

     for (uint32_t i = 0; i < server_state.players[pot_id].pot_players_count; i++) {
      uint32_t owner_id = server_state.players[pot_id].pot_players[i];
      if (server_state.players[owner_id].flags & P_PLAYING) {
       owning_players[owning_players_count++] = owner_id; // Adding owners of same turn
      }
     }

     for (uint32_t p_id = 1; p_id < server_state.players_count; p_id++) {
      bool8_t added;
                                                
      added = (((server_state.players[p_id].flags & P_ALL_IN) &&
                server_state.players[p_id].all_in_turn > server_state.players[pot_id].all_in_turn) ||
               (!(server_state.players[p_id].flags & P_ALL_IN) &&
                (server_state.players[p_id].flags & P_PLAYING)));

      if (added) {
       bool8_t found = false;
       for (uint32_t i = 0; i < owning_players_count; i++) {
        if (owning_players[i] == p_id) {
         found = true;
         break;
        }
       }

       if (!found) {
        owning_players[owning_players_count++] = p_id; // Adding all of succeding turns
       }
      }
     }

     SORT(owning_players, owning_players_count, server_state.players, hand_score, false);

     uint32_t divisor = 1;
     for (uint32_t i = 1; i < owning_players_count; i++) {
      if (server_state.players[owning_players[i - 1]].hand_score != server_state.players[owning_players[i]].hand_score) {
       break;
      } else {
       divisor++;
      }
     }
     
     if (owning_players_count > 0) {
      uint32_t amount = server_state.players[pot_id].pot_balance / divisor;
      uint32_t rest = server_state.players[pot_id].pot_balance % divisor;
                                                
      for (uint32_t i = 0; i < divisor; i++) {
       server_state.players[owning_players[i]].hand_balance += amount;
       if (i == 0) server_state.players[owning_players[i]].hand_balance += rest;
      }

      Player *pot = &server_state.players[pot_id];
      server_state.pot_last_turns[pot->all_in_turn] -= pot->pot_balance;
     }
    }
   }

   for (uint32_t turn = 0; turn < server_state.poker_state.turn + 1; turn++) {
    uint32_t owning_players[MAX_CONNECTED_PLAYERS_COUNT];
    uint32_t owning_players_count = 0;

    for (uint32_t p_id = 1; p_id < server_state.players_count; p_id++) {
     if (((server_state.players[p_id].flags & P_PLAYING) &&
          !(server_state.players[p_id].flags & P_ALL_IN)) ||
         ((server_state.players[p_id].flags & P_ALL_IN) &&
          server_state.players[p_id].all_in_turn > turn))
     {
      owning_players[owning_players_count++] = p_id;
     }
    }

    SORT(owning_players, owning_players_count, server_state.players, hand_score, false);

    uint32_t divisor = 1;
    for (uint32_t i = 1; i < owning_players_count; i++) {
     if (server_state.players[owning_players[i - 1]].hand_score != server_state.players[owning_players[i]].hand_score) {
      break;
     } else {
      divisor++;
     }
    }

    if (owning_players_count > 0) {
     uint32_t amount = server_state.pot_last_turns[turn] / divisor;
     uint32_t rest = server_state.pot_last_turns[turn] % divisor;
                                                
     for (uint32_t i = 0; i < divisor; i++) {
      server_state.players[owning_players[i]].hand_balance += amount;
      if (i == 0) server_state.players[owning_players[i]].hand_balance += rest;
     }
    }
   }

   bool8_t is_player_dead = false;

   for (uint32_t p_id = 1; p_id < server_state.players_count; p_id++) {
    Player *p = &server_state.players[p_id];
    if (p->hand_balance == 0) {
     is_player_dead = true;
     break;
    }
   }

   if (is_player_dead) {
    for (uint32_t p_id = 1; p_id < server_state.players_count; p_id++) {
     Player *p = &server_state.players[p_id];

     if (p->flags & P_EXISTS) p->global_score += (p->hand_balance - PLAYER_DEFAULT_BALANCE);
    }
   }

   se_write_u8(P_OVER_WIN);

   uint32_t last_players_count = 0;
   for (uint32_t p_id = 1; p_id < server_state.players_count; p_id++) {
    if (server_state.players[p_id].flags & (P_PLAYING | P_ALL_IN)) {
     last_players_count++;
    }
   }

   se_write_u32(last_players_count);
                        
   for (uint32_t p_id = 1; p_id < server_state.players_count; p_id++) {
    Player *p = &server_state.players[p_id];

    if (p->flags & (P_PLAYING | P_ALL_IN)) {
     se_write_u32(p_id);
     se_write_u32(p->flags);
     se_write_s32(p->hand_balance);
     se_write_s32(p->balance);

     for (uint32_t j = 0; j < 2; j++) {
      se_write_u8(p->cards[j].color);
      se_write_u8(p->cards[j].number);
     }
    }
   }

   if (server_state.poker_state.turn < 3) {
    se_write_u8(1);
    for (uint32_t i = 0; i < 3 + 1 + 1; i++) {
     se_write_u8(server_state.poker_state.common_cards[i].color);
     se_write_u8(server_state.poker_state.common_cards[i].number);
    }
   } else {
    se_write_u8(0);
   }

   // Reset the game if dead
   if (is_player_dead) {
    se_write_u8(1);
    
    se_write_u32(server_players_count(P_EXISTS));
    
    for (uint32_t p_id = 1; p_id < server_state.players_count; p_id++) {
     Player *p = &server_state.players[p_id];

     if (p->flags & P_EXISTS) {
      se_write_u32(p_id);
      se_write_s32(p->global_score);
      
      p->hand_balance = PLAYER_DEFAULT_BALANCE;
     }
    }

   } else {
    se_write_u8(0);
   }
  } else {                      
   Player *player = &server_state.players[player_id];

   se_write_u8(P_PLAYED);
   se_write_u32(server_state.pot_balance);
   se_write_u32(server_state.highest_bet);
   se_write_u32(server_state.player_order_index);

   se_write_u32(player_id);
   se_write_s32(player->hand_balance);
   se_write_u32(player->highest_bet);
   se_write_u32(player->flags);

   uint32_t all_in_count = server_players_count(P_ALL_IN);
   se_write_u32(all_in_count);

   for (uint32_t p_id = 1; p_id < server_state.players_count; p_id++) {
    if (server_state.players[p_id].flags & P_ALL_IN) {
     se_write_u32(p_id);
     se_write_u32(server_state.players[p_id].pot_balance);
    }
   }
                        
   if (is_next_turn) {
    se_write_u32(server_state.poker_state.common_cards_count);

    for (uint32_t i = 0; i < server_state.poker_state.common_cards_count; i++) {
     se_write_u8(server_state.poker_state.common_cards[i].color);
     se_write_u8(server_state.poker_state.common_cards[i].number);
    }
   } else {
    se_write_u32(0);
   }
  }               
 }

 uint64_t packet_size = 0;
 se_end(&packet_size);

 server_send_to_connected(send_packet_buffer, packet_size);

 if (is_finished) {
  for (uint32_t p_id = 1; p_id < server_state.players_count; p_id++) {
   if (server_state.players[p_id].flags & P_EXISTS) {
    server_state.players[p_id].balance = server_state.players[p_id].hand_balance;
    printf("Confirming the transaction, balance is %d...\n", server_state.players[p_id].balance);

    server_state.players[p_id].flags = P_EXISTS;
   }
  }
 }
}

void player_pay(Player *player, uint32_t raise) {
 uint32_t new_highest_bet = server_state.highest_bet + raise;
 int32_t to_pay = new_highest_bet - player->highest_bet;

 if (to_pay >= player->hand_balance) {
  // Going all in
  player->flags |= P_ALL_IN | P_ALL_IN_FIRST_TURN;
  player->all_in_turn = server_state.poker_state.turn;
  to_pay = player->hand_balance;
  new_highest_bet = player->highest_bet + to_pay;
 }

 // Actually pay
 player->hand_balance -= to_pay;
 player->highest_bet = player->highest_bet + to_pay;
 server_state.highest_bet = MAX(server_state.highest_bet, new_highest_bet);
        
 server_state.pot_balance += to_pay;

 // Sort the current side pots
 uint32_t players_side_pots[MAX_PLAYERS_COUNT];
 uint32_t players_side_pots_count = 0;

 for (uint32_t p_id = 1; p_id < server_state.players_count; p_id++) {
  if (server_state.players[p_id].flags & P_ALL_IN_FIRST_TURN) {
   players_side_pots[players_side_pots_count++] = p_id;
  }
 }

 SORT(players_side_pots, players_side_pots_count, server_state.players, highest_bet, true);

 // Recompute the side pots
 uint32_t already_payed = 0;
 for (uint32_t i = 0; i < players_side_pots_count; i++) {
  uint32_t pot_id = players_side_pots[i];
  Player *pot_player = &server_state.players[pot_id];

  pot_player->pot_balance = 0;

  if (server_state.poker_state.turn == 0 && i == 0) {
   pot_player->pot_balance = BLIND_PRICE;
  }
                
  pot_player->pot_players_count = 1;
  pot_player->pot_players[0] = pot_id;

  uint32_t pot_highest_bet = pot_player->highest_bet;
                
  uint32_t to_pay = pot_highest_bet - already_payed;

  for (uint32_t p_id = 1; p_id < server_state.players_count; p_id++) {
   if (server_state.players[p_id].flags & P_CONNECTED) {
    uint32_t highest_bet = server_state.players[p_id].highest_bet;
                                
    if (highest_bet > already_payed) {
     pot_player->pot_balance += MIN(to_pay, highest_bet - already_payed);

     bool8_t found = false;
                                        
     for (uint32_t i = 0; i < pot_player->pot_players_count; i++) {
      if (pot_player->pot_players[i] == p_id) {
       found = true;
       break;
      }
     }
                                        
     if (!found) pot_player->pot_players[pot_player->pot_players_count++] = p_id;
    }
   }
  }
                
  already_payed = pot_highest_bet;
 }
}

static void server_update() {
 uint64_t bytes_count;
 PkAddr src_addr;
        
 while ((bytes_count = server_get_packet(server_state.packet_buffer, sizeof server_state.packet_buffer, &src_addr))) {
  se_begin(server_state.packet_buffer, PACKET_MAX_SIZE);
  uint8_t packet_type;
  se_read_u8(&packet_type);

  switch (packet_type) {
  case P_HELLO: {
   String username = se_read_string(&frame_pool);

   PlayerID player_id = 0;
   Player *player = 0;
   bool8_t found = false;

   for (uint32_t id = 0; id < server_state.players_count; id++) {
    if (string_equal(username, server_state.players[id].name)) {
     player_id = id;
     found = true;
     break;
    }
   }

   if (!found) {
    if (server_state.players_count + 1 >= MAX_PLAYERS_COUNT) break;

    player_id = server_state.players_count;
    server_state.players_count++;

    player = &server_state.players[player_id];
    memzero(player);

    player->name = string_copy(&main_pool, username);
    player->flags |= P_EXISTS;
    player->global_score = 0;

    if (string_equal(STR_L("allin"), player->name)) { // TODO: For debug
     player->balance = 100;
    } else if (string_equal(STR_L("test"), player->name)) {
     player->balance = 60;
    } else if (string_equal(STR_L("other"), player->name)) {
     player->balance = 80;
    } else {
     player->balance = PLAYER_DEFAULT_BALANCE;
    }
   } else {
    player = &server_state.players[player_id];
   }

   player->address = src_addr;
   player->flags |= P_CONNECTED;
   player->hand_balance = player->balance;
                        
   // Send welcome packet to connected player
   se_begin(send_packet_buffer, PACKET_MAX_SIZE);
   se_write_u8(P_WELCOME);
   se_write_u32(player_id);

   uint64_t packet_size;
   se_end(&packet_size);

   server_send_packet(player_id, send_packet_buffer, packet_size);

   // Send players and game info to connected player
   se_begin(send_packet_buffer, PACKET_MAX_SIZE);

   if (server_state.game_state == G_WAITING) {
    se_write_u8(P_PLAYERS_WAITING);
   } else if (server_state.game_state == G_PLAYING) {
    se_write_u8(P_PLAYERS_PLAYING);
   } else {
    CRASH("Unreachable!\n");
   }

   uint32_t connected_players_count = server_players_count(P_CONNECTED);

   se_write_u32(connected_players_count);

   for (uint32_t p_id = 1; p_id < server_state.players_count; p_id++) {
    if (server_state.players[p_id].flags & P_CONNECTED) {
     se_write_player(p_id, &server_state.players[p_id]);
    }
   }

   if (server_state.game_state == G_PLAYING) {
    se_write_server_player_order();
   }

   se_end(&packet_size);

   server_send_packet(player_id, send_packet_buffer, packet_size);

   // Informing other players that there is a new player
   se_begin(send_packet_buffer, PACKET_MAX_SIZE);
   se_write_u8(P_NEW_PLAYER);
   se_write_player(player_id, &server_state.players[player_id]);
   se_end(&packet_size);

   for (uint32_t p_id = 1; p_id < server_state.players_count; p_id++) {
    if (server_state.players[p_id].flags & P_CONNECTED && p_id != player_id) {
     server_send_packet(p_id, send_packet_buffer, packet_size);
    }
   }
  } break;
  case P_BYE: {
   uint32_t player_id = 0;
   se_read_u32(&player_id);

   printf("[Server] Disconnecting: %d\n", player_id);
   if (server_state.game_state == G_PLAYING) {
    server_state.players[player_id].flags &= ~P_PLAYING;
   } else {
    server_state.players[player_id].flags = P_EXISTS;
   }

   // Transmitting the packet
   for (uint32_t p_id = 1; p_id < server_state.players_count; p_id++) {
    if ((server_state.players[p_id].flags & P_CONNECTED) && p_id != player_id) {
     server_send_packet(p_id, server_state.packet_buffer, bytes_count);
    }
   }

   if (server_state.game_state == G_PLAYING) {
    server_next_turn(server_state.player_order[server_state.player_order_index] != player_id);
   }
  } break;
  case P_START: {
   if (server_state.game_state == G_WAITING) {
    uint32_t connected_players_count = server_players_count(P_CONNECTED);
    if (connected_players_count >= 2) {
     { // Reset server state    
      for (uint32_t p_id = 1; p_id < server_state.players_count; p_id++) {
       if (server_state.players[p_id].flags & P_CONNECTED) {
        server_state.players[p_id].flags = P_CONNECTED | P_EXISTS;

        server_state.players[p_id].highest_bet = 0;
        server_state.players[p_id].pot_balance = 0;
        server_state.players[p_id].pot_players_count = 0;
       }
      } 

      poker_state_init(&server_state.poker_state);

      server_state.player_order_index = 0;
      server_state.is_turn_over = 0;

      server_state.player_order_count = 0;
      for (uint32_t p_id = 1; p_id < server_state.players_count; p_id++) {
       if (server_state.players[p_id].flags & P_CONNECTED) {
        server_state.player_order[server_state.player_order_count++] = p_id;
       }
      }

      // Shuffle order
      for (uint32_t i = 0; i < server_state.player_order_count; i++) {
       uint32_t j = rand() % server_state.player_order_count;
       PlayerID tmp = server_state.player_order[i];
       server_state.player_order[i] = server_state.player_order[j];
       server_state.player_order[j] = tmp;
      }

      for (uint32_t p_id = 1; p_id < server_state.players_count; p_id++) {
       if (server_state.players[p_id].flags & P_CONNECTED) {
        server_state.players[p_id].flags |= P_PLAYING;
        poker_get_cards(&server_state.poker_state, server_state.players[p_id].cards);

        server_state.game_state = G_PLAYING;
       }
      }

      Player *blind = &server_state.players[server_state.player_order[server_state.player_order_count - 1]];
      if (blind->hand_balance > BLIND_PRICE) {
       blind->hand_balance -= BLIND_PRICE;
       server_state.pot_balance = BLIND_PRICE;
       server_state.highest_bet = BLIND_PRICE;
      } else {
       server_state.pot_balance = 0;
       server_state.highest_bet = 0;      
      }
     }

     for (uint32_t p_id = 1; p_id < server_state.players_count; p_id++) {
      if (server_state.players[p_id].flags & P_CONNECTED) {
       se_begin(send_packet_buffer, PACKET_MAX_SIZE);

       se_write_u8(P_GAME_BEGIN);

       for (uint32_t i = 0; i < 2; i++) {
        se_write_u8(server_state.players[p_id].cards[i].color);
        se_write_u8(server_state.players[p_id].cards[i].number);
       }

       se_write_server_player_order();

       uint64_t packet_size = 0;
       se_end(&packet_size);

       server_send_packet(p_id, send_packet_buffer, packet_size);
      }
     }
    }
   }
  } break;
  case P_CALL:
  case P_FOLD:
  case P_RAISE: {
   if (server_state.game_state == G_PLAYING) {
    uint32_t player_id;
    se_read_u32(&player_id);

    if (server_state.player_order[server_state.player_order_index] == player_id) {
     Player *player = &server_state.players[player_id];
                                        
     if (packet_type == P_CALL) {
      player_pay(player, 0);
     } else if (packet_type == P_FOLD) {
      player->flags &= ~P_PLAYING;
     } else {
      uint32_t raise = 0;
      se_read_u32(&raise);

      player_pay(player, raise);
     }

     server_next_turn(false);
    }
   }
  } break;
  default: {
   printf("[Server] Received unkown packet!\n");
  } break;
  }

  se_end(0);
 }
}

static void client_init() {
 client_state.players_count = 0;
        
 client_state.player_order_index = 0;
 client_state.player_order_count = 0;
 client_state.pot_balance = 0;
 client_state.highest_bet = 0;

 client_state.player_id = 0;

 client_state.common_cards_count = 0;
 client_state.winning_player_id = 0;
 client_state.winning_players_count = 0;

 client_state.state = CLIENT_STATE_WAITING;
 client_state.flags = 0;
}

static void se_read_client_player_order() {
 se_read_u32(&client_state.player_order_count);
 for (uint32_t i = 0; i < client_state.player_order_count; i++) {
  se_read_u32(&client_state.player_order[i]);
 }
}

static void se_read_client_players(uint32_t players_count) {
 for (uint32_t i = 0; i < players_count; i++) {
  uint32_t player_id = 0;
  se_read_u32(&player_id);

  if (player_id + 1 >= client_state.players_count) {
   client_state.players_count = player_id + 1;
  }

  client_state.players[player_id].name = se_read_string(&main_pool);
  se_read_s32(&client_state.players[player_id].hand_balance);
  uint32_t flags = 0;
  se_read_u32(&flags);
  client_state.players[player_id].flags = flags;

  se_read_s32(&client_state.players[player_id].global_score);
 }
}

static void client_send_call() {
 se_begin(send_packet_buffer, PACKET_MAX_SIZE);

 se_write_u8(P_CALL);
 se_write_u32(client_state.player_id);
 
 uint64_t packet_size;
 se_end(&packet_size);

 client_send_packet(send_packet_buffer, packet_size);
}

static void client_send_fold() {
 se_begin(send_packet_buffer, PACKET_MAX_SIZE);

 se_write_u8(P_FOLD);
 se_write_u32(client_state.player_id);

 uint64_t packet_size;
 se_end(&packet_size);

 client_send_packet(send_packet_buffer, packet_size);
}

static void client_send_raise(uint32_t raise_value) {
 se_begin(send_packet_buffer, PACKET_MAX_SIZE);

 se_write_u8(P_RAISE);
 se_write_u32(client_state.player_id);
 se_write_u32(raise_value);

 uint64_t packet_size;
 se_end(&packet_size);

 client_send_packet(send_packet_buffer, packet_size);
}

static void client_reset() {
 for (uint64_t i = 0; i < client_state.players_count; i++)
  memzero(&client_state.players[i]);

 client_state.players_count = 0;
 client_state.flags = 0;

 client_state.winning_player_id = 0;
 client_state.winning_players_count = 0;

 client_state.state = CLIENT_STATE_WAITING;
}

#define ITEMS_TEXT_HEIGHT 43
#define NAMETAG_TEXT_HEIGHT 30

static Font gui_font;
static Font small_font;

static String *menu_selected_input;
static uint32_t menu_element_position;
static uint32_t menu_element_padding;
static uint32_t menu_element_top_margin;
static uint32_t menu_input_frames;
static int menu_frame_x;
static int menu_frame_y;
static int menu_frame_width;
static int menu_frame_height;
static Color menu_color;

static void menu_begin() {
 menu_selected_input = 0;
 menu_element_position = 0;
 menu_element_padding = 120;
 menu_element_top_margin = 150;

 menu_frame_x = 0;
 menu_frame_y = 0;
 menu_frame_width = screen_width;
 menu_frame_height = screen_height;

 menu_color = (Color) { 0xff, 0xff, 0xff, 0xff };
}

static void menu_begin_framed(int x, int y, int width, int height) {
 menu_begin();
 
 menu_frame_x = x;
 menu_frame_y = y;
 menu_frame_width = width;
 menu_frame_height = height;
}

static void menu_end() {}

static void menu_set_color(Color color) {
 menu_color = color;
}

static void menu_set_padding(uint32_t padding) {
 menu_element_padding = padding;
}

static void menu_set_top_margin(uint32_t top_margin) {
 menu_element_top_margin = top_margin;
}

static void menu_label(char *text) {
 Vector2 text_dims = MeasureTextEx(gui_font, text, ITEMS_TEXT_HEIGHT, 2);
 float text_width = text_dims.x;

 int text_x = menu_frame_x + menu_frame_width / 2 - text_width / 2;
 int text_y = menu_frame_y + menu_frame_height / 12 + menu_element_top_margin + menu_element_padding * menu_element_position;

 DrawTextEx(gui_font, text, (Vector2){ text_x, text_y }, ITEMS_TEXT_HEIGHT, 2, menu_color);

 menu_element_position++;
}

static bool8_t menu_button(char *text) {
 Vector2 text_dims = MeasureTextEx(gui_font, text, ITEMS_TEXT_HEIGHT, 2);
 float text_width = text_dims.x;
 float text_height = text_dims.y;

 int text_x = menu_frame_x + menu_frame_width / 2 - text_width / 2;
 int text_y = menu_frame_y + menu_frame_height / 12 + menu_element_top_margin + menu_element_padding * menu_element_position;

 bool8_t is_hovered = (mouse_x >= text_x && mouse_x <= text_x + text_width &&
                       mouse_y >= text_y && mouse_y <= text_y + ITEMS_TEXT_HEIGHT);

#define BUTTON_MARGIN_X 10
#define BUTTON_MARGIN_Y 2

 Rectangle button_background_rec = {
  .x = text_x - BUTTON_MARGIN_X,
  .y = text_y - BUTTON_MARGIN_Y,
  .width = text_width + BUTTON_MARGIN_X * 2,
  .height = text_height + BUTTON_MARGIN_Y * 2,
 };
 
 DrawRectangleRounded(button_background_rec, 0.5f, 20, is_hovered ? BLACK : WHITE);
 
 DrawTextEx(gui_font, text, (Vector2){ text_x, text_y }, ITEMS_TEXT_HEIGHT, 2, is_hovered ? WHITE : BLACK);
 menu_element_position++;

 return is_hovered && IsMouseButtonReleased(MOUSE_BUTTON_LEFT);
}

#define INPUT_BLINK_PERIOD 50

static void menu_input(String *value, uint32_t max_size) {
 if (menu_selected_input == 0) { menu_selected_input = value; }

 int input_y = menu_frame_y + menu_frame_height / 12 + menu_element_top_margin + menu_element_padding * menu_element_position;

 if (mouse_y >= input_y && mouse_y <= input_y + ITEMS_TEXT_HEIGHT) { menu_selected_input = value; }

 char *value_cstr = string_to_cstr(&frame_pool, *value);

 Vector2 text_dims = MeasureTextEx(gui_font, value_cstr, ITEMS_TEXT_HEIGHT, 2);
 float text_width = text_dims.x;

 float text_x = menu_frame_x + menu_frame_width / 2 - text_width / 2;

 DrawTextEx(gui_font, value_cstr, (Vector2){ text_x, input_y }, ITEMS_TEXT_HEIGHT, 2, menu_color);

 if (menu_selected_input == value) {
  int key;
  while ((key = GetCharPressed())) {
   if (value->count < max_size) {
    value->data[value->count++] = (uint8_t)key;
   }
  }

  if ((IsKeyPressed(KEY_BACKSPACE) || IsKeyPressedRepeat(KEY_BACKSPACE)) && value->count > 0) {
   value->count--;
  }

  if (menu_input_frames % INPUT_BLINK_PERIOD < (INPUT_BLINK_PERIOD / 2)) {
   DrawRectangle(text_x + text_width + 2, input_y, 2, ITEMS_TEXT_HEIGHT, menu_color);
  }
 }

 menu_element_position++;
 menu_input_frames++;
}

enum {
 GUI_MAIN_MENU = 0,
 GUI_CONNECT_MENU,
 GUI_USERNAME_MENU,
 GUI_GAME,
 GUI_DATABASE
} gui_state;

#define DEFAULT_SERVER_PORT 6966

#define ALLOCATED_MEMORY_SIZE Megabytes(5)

#define CARD_WIDTH (500 * 5 / 7)
#define CARD_HEIGHT 500

static Texture2D cards_texture;
static Texture2D background_texture;

static void draw_background() {
 DrawTexture(background_texture,
             screen_width / 2 - background_texture.width / 2,
             screen_height / 2 - background_texture.height / 2,
             (Color) { 0xff, 0xff, 0xff, 0xff });
}

static void draw_card(Card card, int x, int y, int width) {
 int src_x = CARD_WIDTH * card.number;
 int src_y = CARD_HEIGHT * card.color;
        
 Rectangle source = { src_x, src_y, CARD_WIDTH, CARD_HEIGHT };
 Rectangle dest = { x, y, width, width * CARD_HEIGHT / CARD_WIDTH };
        
 DrawTexturePro(cards_texture, source, dest, (Vector2){ 0, 0 }, 0.f, WHITE);
}

#define NUMBER_INPUT_MAX_SIZE 16

static void draw_info(int pos_x, int pos_y, char *info_text) {
 Vector2 info_text_dims = MeasureTextEx(gui_font, info_text, ITEMS_TEXT_HEIGHT, 2);
       
#define INFO_PADDING_X 5
       
 DrawRectangle(pos_x - info_text_dims.x / 2 - INFO_PADDING_X,
               pos_y - info_text_dims.y / 2,
               info_text_dims.x + INFO_PADDING_X * 2,
               info_text_dims.y,
               BLACK);

 DrawTextEx(gui_font, info_text,
            (Vector2){
             pos_x - info_text_dims.x / 2,
             pos_y - info_text_dims.y / 2
            }, ITEMS_TEXT_HEIGHT, 2, WHITE);
}
/*
FILE *save_file;

void restore_state() {
 pool_temp_begin(&frame_pool);
 
 fseek(file, 0, SEEK_END);

 long size = ftell(file);

 rewind(file);

 if (size > 0) {
  uint8_t *file_data = fread();
 }

 pool_temp_end(&frame_pool);
}
*/
static void poker_main(int argc, char **argv) {
 if (!initialize_networking()) {
  CRASH("Network initalization");
 }

 void *mem = allocate_pages(ALLOCATED_MEMORY_SIZE);
 if (mem == 0) {
  CRASH("Not enough memory!\n");
 }
 
 pool_init(&main_pool, mem, ALLOCATED_MEMORY_SIZE);
 frame_pool = pool_subpool(&main_pool, ALLOCATED_MEMORY_SIZE / 2);

 memzero(&server_state);
 memzero(&client_state);

 server_state.players_count = 1;

 client_state.username.data = pool_alloc(&main_pool, PLAYER_NAME_MAX_LEN);
 client_state.socket = create_client_socket();

 screen_width = 1600;
 screen_height = 900;

 // xorshift_seed(time(0));
 srand(time(0));
/*
 save_file = fopen("save.dat", "w+");
 if (save_file == 0) {
  CRASH("Failed to create save file!\n");
 }

 restore_state();
*/ 
 SetConfigFlags(FLAG_WINDOW_RESIZABLE);
 InitWindow(screen_width, screen_height, "Poker");

 SetTargetFPS(60);

 // Loading resources
 gui_font = LoadFontEx("garamond.ttf", ITEMS_TEXT_HEIGHT, 0, 250);
 small_font = LoadFontEx("garamond.ttf", NAMETAG_TEXT_HEIGHT, 0, 250);
 
 Image cards_image = LoadImage("cards.png");
 Image background_image = LoadImage("background.png");

 cards_texture = LoadTextureFromImage(cards_image);
 UnloadImage(cards_image);

 SetTextureFilter(cards_texture, TEXTURE_FILTER_BILINEAR); 

 background_texture = LoadTextureFromImage(background_image);
 UnloadImage(background_image);
 
 bool8_t should_close = false;

 gui_state = GUI_USERNAME_MENU;

 Color background_color = {
  0xf7, 0xf7, 0xf7, 0xff
 };

 String distant_ip_address = {};
 distant_ip_address.data = pool_alloc(&main_pool, DISTANT_IP_MAX_LEN);

 String raised_value_string = {};
 raised_value_string.data = pool_alloc(&main_pool, NUMBER_INPUT_MAX_SIZE);

 while (!should_close)  {
  mouse_x = GetMouseX();
  mouse_y = GetMouseY();

  if (IsWindowResized()) {
   screen_width = GetScreenWidth();
   screen_height = GetScreenHeight();
  }
  
  BeginDrawing();
  ClearBackground(background_color);
  EndDrawing();

  pool_temp_begin(&frame_pool);

  switch (gui_state) {
  case GUI_MAIN_MENU: {
   draw_background();
   
   menu_begin();

   if (menu_button("Create local server")) {
    if (server_state.socket == 0) server_state.socket = create_server_socket(DEFAULT_SERVER_PORT);

    if (server_state.socket == 0) {
     CRASH("Couldn't create server");
    }

    static uint8_t localhost_ip[] = {127, 0, 0, 1};
    memcpy(client_state.distant_server_addr.ip, localhost_ip, sizeof (localhost_ip));

    client_state.distant_server_addr.port = le_to_be_u16(DEFAULT_SERVER_PORT);

    server_init();
    client_init();

    gui_state = GUI_GAME;
   }

   if (menu_button("Connect to server")) {
    gui_state = GUI_CONNECT_MENU;
   }

   if (menu_button("Explore local database")) {

   }

   menu_end();
  } break;
  case GUI_USERNAME_MENU: {
   draw_background();

   menu_begin();
   menu_set_padding(80);

   menu_label("Choose your username");

   menu_input(&client_state.username, PLAYER_NAME_MAX_LEN);

   if (menu_button("OK") || IsKeyPressed(KEY_ENTER)) {
    gui_state = GUI_MAIN_MENU;
   }

   menu_end();
  } break;
  case GUI_CONNECT_MENU: {
   draw_background();
   menu_set_padding(80);

   menu_begin();

   menu_label("Server IP");

   menu_input(&distant_ip_address, DISTANT_IP_MAX_LEN);

   if (menu_button("Connect") || IsKeyPressed(KEY_ENTER)) {
    uint8_t ip_address[4];
    if (parse_ipv4(distant_ip_address, ip_address)) {
     memcpy(client_state.distant_server_addr.ip, ip_address, sizeof (ip_address));
     client_state.distant_server_addr.port = le_to_be_u16(DEFAULT_SERVER_PORT);

     client_init();

     gui_state = GUI_GAME;
    }
   }

   if (menu_button("Back")) {
    gui_state = GUI_MAIN_MENU;
   }

   menu_end();
  } break;
  case GUI_DATABASE: {

  } break;
  case GUI_GAME: {
   if (server_state.socket != 0) server_update();

   if (!(client_state.flags & CLIENT_SENT_HELLO)) {
    uint64_t packet_size = 0;
    se_begin(send_packet_buffer, PACKET_MAX_SIZE);

    se_write_u8(P_HELLO);
    se_write_string(client_state.username);

    se_end(&packet_size);

    client_send_packet(send_packet_buffer, packet_size);

    client_state.flags |= CLIENT_SENT_HELLO;
   }

   uint64_t bytes_count;
   while ((bytes_count = client_get_packet(client_state.packet_buffer, sizeof client_state.packet_buffer))) {
    se_begin(client_state.packet_buffer, PACKET_MAX_SIZE);

    uint8_t packet_type;
    se_read_u8(&packet_type);

    switch (packet_type) {
    case P_WELCOME: {
     se_read_u32(&client_state.player_id);

     printf("My player id is %u\n", client_state.player_id);
    } break;
    case P_PLAYERS_WAITING: {
     client_state.state = CLIENT_STATE_WAITING;

     uint32_t players_count;
     se_read_u32(&players_count);

     se_read_client_players(players_count);
    } break;
    case P_PLAYERS_PLAYING: {
     client_state.state = CLIENT_STATE_WATCHING;

     uint32_t players_count;
     se_read_u32(&players_count);

     se_read_client_players(players_count);
     se_read_client_player_order();
    } break;
    case P_NEW_PLAYER: {
     se_read_client_players(1);
    } break;
    case P_BYE: {
     uint32_t player_id;
     se_read_u32(&player_id);

     if (client_state.state == CLIENT_STATE_PLAYING) {
      client_state.players[player_id].flags &= ~P_PLAYING;
     } else {
      client_state.players[player_id].flags &= ~(P_PLAYING | P_CONNECTED);
     }
    } break;
    case P_SHUTDOWN: {
     gui_state = GUI_MAIN_MENU;
    } break;
    case P_GAME_BEGIN: {
     // Change player states to connected
     for (uint32_t p_id = 1; p_id < client_state.players_count; p_id++) {
      if (client_state.players[p_id].flags & P_PLAYING) {
       client_state.players[p_id].flags = P_CONNECTED | P_EXISTS;
      }
     }

     // Reset client game state
     client_state.common_cards_count = 0;
     client_state.player_order_count = 0;
     client_state.pot_balance = 0;
     client_state.highest_bet = 0;

     // Read data
     for (uint32_t i = 0; i < 2; i++) {
      uint8_t color;
      uint8_t number;
      se_read_u8(&color);
      se_read_u8(&number);

      client_state.players[client_state.player_id].cards[i].color = color;
      client_state.players[client_state.player_id].cards[i].number = number;
     }

     se_read_client_player_order();
     client_state.player_order_index = 0;

     Player *blind = &client_state.players[client_state.player_order[client_state.player_order_count - 1]];
     if (blind->hand_balance > BLIND_PRICE) {
      blind->hand_balance -= BLIND_PRICE;

      client_state.pot_balance = BLIND_PRICE;
      client_state.highest_bet = BLIND_PRICE;
     } else {
      server_state.pot_balance = 0;
      server_state.highest_bet = 0;
     }

     for (uint32_t p_id = 1; p_id < client_state.players_count; p_id++) {
      Player *player = &client_state.players[p_id];
      if (player->flags & P_CONNECTED) {
       player->flags |= P_PLAYING;
      }
     }

     client_state.state = CLIENT_STATE_PLAYING;
    } break;
    case P_PLAYED: {
     if (client_state.state == CLIENT_STATE_PLAYING ||
         client_state.state == CLIENT_STATE_WATCHING)
     {
      se_read_u32(&client_state.pot_balance);
      se_read_u32(&client_state.highest_bet);
      se_read_u32(&client_state.player_order_index);

      uint32_t player_id;
      se_read_u32(&player_id);
      se_read_s32(&client_state.players[player_id].hand_balance);
      se_read_u32(&client_state.players[player_id].highest_bet);
      se_read_u32((uint32_t*)&client_state.players[player_id].flags);

      uint32_t all_in_count = 0;
      se_read_u32(&all_in_count);
      for (uint32_t i = 0; i < all_in_count; i++) {
       uint32_t p_id = 0;
       se_read_u32(&p_id);
       se_read_u32(&client_state.players[p_id].pot_balance);
      }

      uint32_t common_cards_count;
      se_read_u32(&common_cards_count);

      if (common_cards_count > 0) {
       client_state.common_cards_count = common_cards_count;

       for (uint32_t i = 0; i < client_state.common_cards_count; i++) {
        uint8_t color = 0;
        uint8_t number = 0;
        se_read_u8(&color);
        se_read_u8(&number);

        client_state.common_cards[i].color = color;
        client_state.common_cards[i].number = number;
       }

       for (uint32_t p_id = 1; p_id < client_state.players_count; p_id++) {
        if (client_state.players[p_id].flags & P_PLAYING) {
         client_state.players[p_id].highest_bet = 0;
        }
       }
      }

      if (player_id == client_state.player_id &&
          !(client_state.players[player_id].flags & P_PLAYING))
      {
       client_state.state = CLIENT_STATE_WATCHING;
      }
     }
    } break;
    case P_OVER_FORFEIT: {
     se_read_u32(&client_state.winning_player_id);
     se_read_s32(&client_state.players[client_state.winning_player_id].hand_balance);
                                                       
     client_state.state = CLIENT_STATE_GAME_OVER_FORFEIT;
    } break;
    case P_OVER_WIN: {
     uint32_t sorted_players_count = 0;
     se_read_u32(&sorted_players_count);

     client_state.winning_players_count = 0;
     for (uint32_t p_id = 1; p_id < client_state.players_count; p_id++) {
      if (client_state.players[p_id].flags & P_EXISTS) {
       client_state.players[p_id].flags &= ~P_PLAYING;
      }
     }

     for (uint32_t i = 0; i < sorted_players_count; i++) {
      uint32_t p_id = 0;
      se_read_u32(&p_id);

      client_state.winning_players_ids[client_state.winning_players_count] = p_id;
      client_state.winning_players_count++;

      se_read_u32((uint32_t*)&client_state.players[p_id].flags);
      se_read_s32(&client_state.players[p_id].hand_balance);
      se_read_s32(&client_state.players[p_id].balance);
                                                                
      for (uint32_t j = 0; j < 2; j++) {
       se_read_u8((uint8_t*)&client_state.players[p_id].cards[j].color);
       se_read_u8((uint8_t*)&client_state.players[p_id].cards[j].number);
      }
     }

     uint8_t cards_updated = 0;
     se_read_u8(&cards_updated);

     if (cards_updated) {
      for (uint32_t i = 0; i < 3 + 1 + 1; i++) {
       se_read_u8((uint8_t*)&client_state.common_cards[i].color);
       se_read_u8((uint8_t*)&client_state.common_cards[i].number);
      }
     }

     bool8_t is_player_dead = false;
     se_read_u8(&is_player_dead);

     if (is_player_dead) {
      client_state.flags |= CLIENT_PLAYER_DEAD;

      uint32_t existing_players = 0;
      se_read_u32(&existing_players);
      
      for (uint32_t i = 0; i < existing_players; i++) {
       uint32_t p_id = 0;
       se_read_u32(&p_id);

       se_read_s32(&client_state.players[p_id].global_score);
      }
     }

     client_state.state = CLIENT_STATE_GAME_OVER_WIN;
    } break;
    }

    bool8_t success = se_end(0);
    if (!success) { CRASH("Failed to encode message!"); }
   }

   draw_background();
   
   switch (client_state.state) {
   case CLIENT_STATE_WAITING: {
    menu_begin();
    menu_set_padding(50);

    menu_label("Waiting for players:");

    for (uint32_t i = 0; i < client_state.players_count; i++) {
     if (client_state.players[i].flags & P_CONNECTED) {
      menu_label(string_to_cstr(&frame_pool, client_state.players[i].name));
     }
    }

    uint32_t connected_players_count = server_players_count(P_CONNECTED);
    if (server_state.socket != 0 && connected_players_count >= 2) {
     if (menu_button("Start game") || IsKeyPressed(KEY_ENTER)) {
      uint8_t packet = P_START;
      client_send_packet(&packet, 1);
     }
    }

    if (menu_button("Back")) {
     gui_state = GUI_MAIN_MENU;
     disconnect();
    }

    menu_end();
   } break;
   case CLIENT_STATE_PLAYING:
   case CLIENT_STATE_WATCHING:
   case CLIENT_STATE_GAME_OVER_WIN: {
    uint32_t shown_cards_count = client_state.state == CLIENT_STATE_GAME_OVER_WIN ? 5 : client_state.common_cards_count;

#define COMMON_CARDS_PADDING 10
#define COMMON_CARDS_SHOWN_WIDTH 100
#define COMMON_CARD_SHOWN_HEIGHT (COMMON_CARDS_SHOWN_WIDTH * 7) / 5

    int common_cards_width = 5 * (COMMON_CARDS_SHOWN_WIDTH + COMMON_CARDS_PADDING);
     
    int common_cards_x = screen_width / 2 - common_cards_width / 2;
    int common_cards_y = screen_height / 2 - COMMON_CARD_SHOWN_HEIGHT / 2;
    
    for (uint32_t i = 0; i < shown_cards_count; i++) {
     draw_card(client_state.common_cards[i],
               common_cards_x + i * (COMMON_CARDS_SHOWN_WIDTH + COMMON_CARDS_PADDING),
               common_cards_y,
               COMMON_CARDS_SHOWN_WIDTH);
    }

    uint32_t playing_players_count = 0;
    for (uint32_t i = 0; i < client_state.player_order_count; i++) {
     if (client_state.players[client_state.player_order[i]].flags & P_CONNECTED) {
      playing_players_count++;
     }
    }

    uint32_t player_id_index = 0;
    for (uint32_t i = 0; i < client_state.player_order_count; i++) {
     if (client_state.player_order[i] == client_state.player_id) {
      player_id_index = i;
      break;
     }
    }
    
    for (uint32_t i = 0; i < client_state.player_order_count; i++) {
     uint32_t order_i = (i + player_id_index) % client_state.player_order_count;
     uint32_t p_id = client_state.player_order[order_i];
     Player *player = &client_state.players[p_id];
     
     if (player->flags & P_CONNECTED) {
#define CARDS_PADDING 10
#define CARDS_SHOWN_WIDTH 100
#define CARDS_SHOWN_HEIGHT (CARDS_SHOWN_WIDTH * 7) / 5
#define CARDS_ANGLE_OFFSET (M_PI / 2.f)
      
      float cards_radius = screen_width * 3.f / 8.f;
      
      int pos_x = screen_width / 2 + cards_radius * cosf(2.f * M_PI * (float)i / (float)playing_players_count + CARDS_ANGLE_OFFSET);
      int pos_y = screen_height / 2 + (cards_radius * screen_height / screen_width) * sinf(2.f * M_PI * (float)i / (float)playing_players_count + CARDS_ANGLE_OFFSET) + 25;

      int cards_width = (CARDS_SHOWN_WIDTH * 2 + CARDS_PADDING);
      int upper_corner_x = pos_x - cards_width / 2;
      int upper_corner_y = pos_y - CARDS_SHOWN_HEIGHT / 2;
      
      char *name_text = string_to_cstr(&frame_pool, client_state.players[p_id].name);
      Vector2 name_text_dims = MeasureTextEx(gui_font, name_text, NAMETAG_TEXT_HEIGHT, 2);

      char *score_text = tprintf(&frame_pool, "%d", client_state.players[p_id].hand_balance);
      Vector2 score_text_dims = MeasureTextEx(small_font, score_text, NAMETAG_TEXT_HEIGHT, 2);

      int upper_y_margin = name_text_dims.y + 5;

      Rectangle rect = {
       upper_corner_x - 10,
       upper_corner_y - upper_y_margin,
       cards_width + 20,
       CARDS_SHOWN_HEIGHT + upper_y_margin + 10
      };

      Color background_color;
      if (order_i == client_state.player_order_index && client_state.state != CLIENT_STATE_GAME_OVER_WIN) {
       background_color = (Color) { 0, 0, 0, 0xff };
      } else {
       background_color = (Color) { 0, 0, 0, 80 };
      }
      
      DrawRectangleRounded(rect, 0.2f, 10, background_color);

      DrawTextEx(small_font, name_text,
                 (Vector2){
                  pos_x - cards_width / 2,
                  pos_y - upper_y_margin - CARDS_SHOWN_HEIGHT / 2
                 }, NAMETAG_TEXT_HEIGHT, 2, WHITE);

      DrawTextEx(small_font, score_text,
                 (Vector2){
                  pos_x + cards_width / 2 - score_text_dims.x,
                  pos_y - upper_y_margin - CARDS_SHOWN_HEIGHT / 2
                 }, NAMETAG_TEXT_HEIGHT, 2, WHITE);

      for (uint32_t j = 0; j < 2; j++) {
       Card to_draw = {};
       
       if (p_id == client_state.player_id || client_state.state == CLIENT_STATE_GAME_OVER_WIN) {
        to_draw = player->cards[j];
       }
       
       draw_card(to_draw,
                 upper_corner_x + (CARDS_SHOWN_WIDTH + CARDS_PADDING) * j,
                 upper_corner_y, CARDS_SHOWN_WIDTH);
      }

      if ((player->flags & P_ALL_IN) && client_state.state != CLIENT_STATE_GAME_OVER_WIN) {
       draw_info(pos_x, pos_y, tprintf(&frame_pool, "Pot: %d", player->pot_balance));
      }

      if (!(player->flags & P_ALL_IN) && !(player->flags & P_PLAYING)) {
       draw_info(pos_x, pos_y, "FOLD");
      }

      if (player->flags & P_PLAYING) {
       int32_t delta = player->hand_balance - player->balance;
      
       if (delta != 0 && client_state.state == CLIENT_STATE_GAME_OVER_WIN) {
        char *added_text = tprintf(&frame_pool, "%s%d\n", delta > 0 ? "+" : "", delta);
        Vector2 added_text_dims = MeasureTextEx(small_font, added_text, NAMETAG_TEXT_HEIGHT, 2);

        DrawTextEx(small_font, added_text,
                   (Vector2){
                    pos_x + cards_width / 2 - added_text_dims.x,
                    pos_y - upper_y_margin - CARDS_SHOWN_HEIGHT / 2 - NAMETAG_TEXT_HEIGHT
                   }, NAMETAG_TEXT_HEIGHT, 2, delta > 0 ? GREEN : RED);
       }
      }
     }
    }

    if (client_state.state == CLIENT_STATE_GAME_OVER_WIN) {
     menu_begin();

     menu_set_color((Color) { 0, 0, 0, 0xff });
     
     if (menu_button("Play Again") || IsKeyPressed(KEY_ENTER)) {
      client_reset();
     }
     
     if (client_state.flags & CLIENT_PLAYER_DEAD) {
      menu_label("Player died, resetting balances!");
     }
     
     menu_end();
    } else {
     // Highest bet / Pot
     
     char *highest_bet_text = tprintf(&frame_pool, "Highest bet: %d", client_state.highest_bet);
     char *pot_balance_text = tprintf(&frame_pool, "Pot balance: %d", client_state.pot_balance);

     Vector2 highest_bet_dims = MeasureTextEx(gui_font, highest_bet_text, ITEMS_TEXT_HEIGHT, 2);

     DrawTextEx(gui_font, highest_bet_text,
                (Vector2){
                 common_cards_x,
                 common_cards_y - highest_bet_dims.y - 10
                }, ITEMS_TEXT_HEIGHT, 2, WHITE);

     Vector2 pot_balance_dims = MeasureTextEx(gui_font, pot_balance_text, ITEMS_TEXT_HEIGHT, 2);

     DrawTextEx(gui_font, pot_balance_text,
                (Vector2){
                 common_cards_x + common_cards_width - pot_balance_dims.x,
                 common_cards_y - pot_balance_dims.y - 10
                }, ITEMS_TEXT_HEIGHT, 2, WHITE);

     // Actions menu
     
     int frame_width = 400;
     int frame_height = 400;
     
     menu_begin_framed(screen_width - frame_width, screen_height - frame_height,
                       frame_width, frame_height);

     menu_set_padding(50);
     menu_set_top_margin(100);

     if (client_state.state == CLIENT_STATE_PLAYING) {
      uint32_t p_id = client_state.player_order[client_state.player_order_index];
      
      if (p_id == client_state.player_id) {
       char *call_button_text = 0;
       if (client_state.highest_bet == 0) {
        call_button_text = "Check";
       } else {
        call_button_text = tprintf(&frame_pool, "Call (Pay %d)", client_state.highest_bet - client_state.players[p_id].highest_bet);
       }

       uint8_t letter_hit = false;
       
       if (menu_button(call_button_text) || IsKeyPressed(KEY_C)) {
        client_send_call();
        letter_hit = true;
       }
       
       if (menu_button("Fold") || IsKeyPressed(KEY_F)) {
        client_send_fold();
        letter_hit = true;
       }

       if (!letter_hit) {
        menu_input(&raised_value_string, NUMBER_INPUT_MAX_SIZE);
       }

       if (menu_button("Raise") || IsKeyPressed(KEY_ENTER)) {
        int64_t raise_value;
        if (string_to_int(raised_value_string, &raise_value) &&
            raise_value > 0 && raise_value <= MAX_RAISE)
        {
         client_send_raise((uint32_t)raise_value);
         raised_value_string.count = 0;
        }
       }
      }
     }

     if (menu_button("Disconnect")) {
      gui_state = GUI_MAIN_MENU;
      disconnect();
     }
     
     menu_end();

     // Scoreboard
     uint32_t scoreboard_players_count = 0;
     
     for (uint32_t p_id = 1; p_id < client_state.players_count; p_id++) {
      Player *p = &client_state.players[p_id];
      if (p->flags & P_EXISTS) {
       scoreboard_players_count++;
      }
     }

     // int scoreboard_frame_height = scoreboard_players_count * (ITEMS_TEXT_HEIGHT + 10);
     menu_begin_framed(0, 0, 300, 0);

     menu_set_top_margin(20);
     menu_set_padding(ITEMS_TEXT_HEIGHT);

     static uint8_t scores_shown = false;

     if (scores_shown) {
      if (menu_button("Hide scores")) scores_shown = false;
     } else {
      if (menu_button("Show scores")) scores_shown = true;
     }
     
     if (scores_shown) {
      struct {
       int32_t live_score;
      } players_scores[MAX_PLAYERS_COUNT];

      uint32_t players[MAX_PLAYERS_COUNT];
      uint32_t players_count = 0;
      
      for (uint32_t p_id = 1; p_id < client_state.players_count; p_id++) {
       Player *p = &client_state.players[p_id];
       if (p->flags & P_EXISTS) {
        players_scores[p_id].live_score = p->global_score + (p->hand_balance - PLAYER_DEFAULT_BALANCE);
        players[players_count++] = p_id;
       }
      }

      SORT(players, players_count, players_scores, live_score, false);
      
      for (uint32_t i = 0; i < players_count; i++) {
       uint32_t p_id = players[i];
       Player *p = &client_state.players[p_id];
       menu_label(tprintf(&frame_pool, STR_FMT_LIT": %d", STR_FMT(p->name), players_scores[p_id]));
      }
     }
     
     menu_end();
    }
   } break;
   case CLIENT_STATE_GAME_OVER_FORFEIT: {
    menu_begin();

    if (client_state.winning_player_id == client_state.player_id) {
     menu_label(tprintf(&frame_pool, "You won, you now have %d in your account!",
                        client_state.players[client_state.player_id].hand_balance));
    } else {
     menu_label("You lost!");
    }

    if (menu_button("Play Again") || IsKeyPressed(KEY_ENTER)) {
     client_reset();
    }

    menu_end();
   } break;
   }
  } break;
  }

  pool_temp_end(&frame_pool);

  if (WindowShouldClose()) should_close = true;
 }

 disconnect();

 CloseWindow();
}
