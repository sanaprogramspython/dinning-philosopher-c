#include <stdbool.h>
#include <stdio.h>
#include <string.h>

typedef enum {
    STATE_THINKING,
    STATE_HUNGRY,
    STATE_WAITING,
    STATE_EATING
} PhilosopherState;

typedef struct {
    int id;
    int left;
    int right;
    PhilosopherState state;
    double health;
    int held[2];
    int held_count;
    int waiting_for;
    bool fed;
} Philosopher;

static Philosopher philosophers[10];
static int chopsticks[10];
static int philosopher_count;
static int chopstick_count;
static int selected_id;
static int score;
static int elapsed_ticks;
static int conflicts;
static int fed_count;
static bool running;
static bool started;
static bool round_over;
static bool deadlocked;
static bool deadlock_occurred;
static int message_type;
static char message[512];
static char result_title[160];
static char result_feedback[512];

static Philosopher* selected_philosopher(void) {
    return &philosophers[selected_id];
}

static void set_message(const char* text, int type) {
    snprintf(message, sizeof(message), "%s", text);
    message_type = type;
}

static Philosopher create_philosopher(int id, int count) {
    Philosopher philosopher = { 0 };
    philosopher.id = id;
    philosopher.left = (id + count - 1) % count;
    philosopher.right = id;
    philosopher.state = STATE_THINKING;
    philosopher.health = 100.0;
    philosopher.held[0] = -1;
    philosopher.held[1] = -1;
    philosopher.waiting_for = -1;
    return philosopher;
}

static void release_resources(Philosopher* philosopher) {
    for (int i = 0; i < philosopher->held_count; i++) {
        int stick = philosopher->held[i];
        if (stick >= 0 && chopsticks[stick] == philosopher->id) chopsticks[stick] = -1;
    }
    philosopher->held_count = 0;
    philosopher->held[0] = -1;
    philosopher->held[1] = -1;
    philosopher->waiting_for = -1;
}

static bool detect_deadlock(void) {
    if (philosopher_count < 2) return false;
    for (int i = 0; i < philosopher_count; i++) {
        if (philosophers[i].state != STATE_WAITING || philosophers[i].held_count == 0 || philosophers[i].waiting_for < 0) {
            return false;
        }
    }

    bool visited[10] = { false };
    int current = 0;
    int visited_count = 0;
    for (int step = 0; step < philosopher_count; step++) {
        if (visited[current]) return false;
        visited[current] = true;
        visited_count++;
        int owner = chopsticks[philosophers[current].waiting_for];
        if (owner < 0 || owner == current) return false;
        current = owner;
    }
    return current == 0 && visited_count == philosopher_count;
}

static void update_deadlock(void) {
    bool was_deadlocked = deadlocked;
    deadlocked = detect_deadlock();
    if (deadlocked && !was_deadlocked) {
        deadlock_occurred = true;
        score -= 30;
        running = false;
        set_message("Deadlock detected: every philosopher holds a chopstick and waits in a circle. Release a chopstick to break the cycle. -30 points.", 2);
    } else if (!deadlocked && was_deadlocked) {
        set_message("Circular wait broken. Resume when you are ready.", 0);
    }
}

static void penalize_conflict(const char* text) {
    char feedback[384];
    conflicts++;
    score -= 10;
    snprintf(feedback, sizeof(feedback), "%s Conflict penalty: -10 points.", text);
    set_message(feedback, 1);
}

static void begin_eating(Philosopher* philosopher) {
    char feedback[256];
    philosopher->state = STATE_EATING;
    philosopher->waiting_for = -1;
    if (philosopher->held_count == 2) {
        snprintf(feedback, sizeof(feedback), "P%d acquired C%d and C%d and is eating. Click P%d again to release both.",
            philosopher->id + 1, philosopher->held[0] + 1, philosopher->held[1] + 1, philosopher->id + 1);
    } else {
        snprintf(feedback, sizeof(feedback), "P%d acquired C%d and is eating. Click P%d again to release both.",
            philosopher->id + 1, philosopher->held[0] + 1, philosopher->id + 1);
    }
    set_message(feedback, 0);
}

static void finish_game(int starved_id) {
    Philosopher* starved = &philosophers[starved_id];
    char conflict_text[80] = "";
    running = false;
    round_over = true;
    snprintf(result_title, sizeof(result_title), "Game Over - Philosopher %d starved.", starved->id + 1);
    if (conflicts > 0) snprintf(conflict_text, sizeof(conflict_text), " %d resource conflicts cost points.", conflicts);
    snprintf(result_feedback, sizeof(result_feedback),
        "Philosopher %d's health reached zero after %d of %d philosophers were fed. %s%s",
        starved->id + 1, fed_count, philosopher_count,
        deadlock_occurred ? "A circular wait also occurred during the round." : "No deadlock occurred.",
        conflict_text);
    char feedback[192];
    snprintf(feedback, sizeof(feedback), "Game Over - Philosopher %d starved.", starved->id + 1);
    set_message(feedback, 1);
}

static void acquire_chopstick(bool take_left) {
    Philosopher* philosopher = selected_philosopher();
    if (!running || round_over || deadlocked) return;
    if (philosopher->state == STATE_THINKING || philosopher->state == STATE_EATING) {
        char feedback[192];
        snprintf(feedback, sizeof(feedback), "P%d cannot request a chopstick while %s.", philosopher->id + 1,
            philosopher->state == STATE_THINKING ? "thinking" : "eating");
        penalize_conflict(feedback);
        return;
    }

    int stick = take_left ? philosopher->left : philosopher->right;
    for (int i = 0; i < philosopher->held_count; i++) {
        if (philosopher->held[i] == stick) {
            char feedback[128];
            snprintf(feedback, sizeof(feedback), "P%d already holds C%d.", philosopher->id + 1, stick + 1);
            penalize_conflict(feedback);
            return;
        }
    }

    int owner = chopsticks[stick];
    if (owner >= 0 && owner != philosopher->id) {
        char feedback[160];
        philosopher->state = STATE_WAITING;
        philosopher->waiting_for = stick;
        snprintf(feedback, sizeof(feedback), "C%d is assigned to P%d; P%d must wait.", stick + 1, owner + 1, philosopher->id + 1);
        penalize_conflict(feedback);
        update_deadlock();
        return;
    }

    chopsticks[stick] = philosopher->id;
    philosopher->held[philosopher->held_count++] = stick;
    if (philosopher->held_count == 2) {
        begin_eating(philosopher);
    } else {
        philosopher->waiting_for = philosopher->left == stick ? philosopher->right : philosopher->left;
        philosopher->state = STATE_WAITING;
        char feedback[128];
        snprintf(feedback, sizeof(feedback), "P%d holds C%d and waits for C%d.", philosopher->id + 1, stick + 1, philosopher->waiting_for + 1);
        set_message(feedback, 0);
    }
    update_deadlock();
}

static void take_both_chopsticks(void) {
    Philosopher* philosopher = selected_philosopher();
    if (!running || round_over || deadlocked) return;
    if (philosopher->state == STATE_THINKING || philosopher->state == STATE_EATING) {
        char feedback[160];
        snprintf(feedback, sizeof(feedback), "P%d must be hungry before requesting both neighboring chopsticks.", philosopher->id + 1);
        penalize_conflict(feedback);
        return;
    }

    int neighbors[2] = { philosopher->left, philosopher->right };
    for (int i = 0; i < 2; i++) {
        int stick = neighbors[i];
        int owner = chopsticks[stick];
        if (owner >= 0 && owner != philosopher->id) {
            philosopher->state = STATE_WAITING;
            philosopher->waiting_for = stick;
            char feedback[160];
            snprintf(feedback, sizeof(feedback), "C%d is assigned to P%d; P%d cannot take both.", stick + 1, owner + 1, philosopher->id + 1);
            penalize_conflict(feedback);
            update_deadlock();
            return;
        }
    }

    for (int i = 0; i < 2; i++) {
        int stick = neighbors[i];
        if (chopsticks[stick] == -1) {
            chopsticks[stick] = philosopher->id;
            philosopher->held[philosopher->held_count++] = stick;
        }
    }
    begin_eating(philosopher);
    update_deadlock();
}

#define API __attribute__((visibility("default")))

API void game_init(int count) {
    if (count < 2) count = 2;
    if (count > 10) count = 10;
    philosopher_count = count;
    chopstick_count = count;
    selected_id = 0;
    score = 0;
    elapsed_ticks = 0;
    conflicts = 0;
    fed_count = 0;
    running = false;
    started = false;
    round_over = false;
    deadlocked = false;
    deadlock_occurred = false;
    result_title[0] = '\0';
    result_feedback[0] = '\0';
    for (int i = 0; i < count; i++) {
        philosophers[i] = create_philosopher(i, count);
        chopsticks[i] = -1;
    }
    set_message("Start the game, select a philosopher, and choose their resources.", 0);
}

API void game_start(void) {
    if (running || round_over || deadlocked) return;
    running = true;
    started = true;
    set_message("Round active. Select a philosopher and allocate their chopsticks.", 0);
}

API void game_pause(void) {
    if (!running) return;
    running = false;
    set_message("Game paused. Your allocation and elapsed time are saved.", 0);
}

API void game_tick(void) {
    if (!running || round_over) return;
    elapsed_ticks++;
    if (elapsed_ticks % 2 == 0) score++;
    for (int i = 0; i < philosopher_count; i++) {
        Philosopher* philosopher = &philosophers[i];
        if (philosopher->state == STATE_THINKING) philosopher->health -= 0.15;
        if (philosopher->state == STATE_HUNGRY) philosopher->health -= 0.7;
        if (philosopher->state == STATE_WAITING) philosopher->health -= 1.0;
        if (philosopher->state == STATE_EATING) philosopher->health += 2.2;
        if (philosopher->health < 0.0) philosopher->health = 0.0;
        if (philosopher->health > 100.0) philosopher->health = 100.0;
    }
    for (int i = 0; i < philosopher_count; i++) {
        if (philosophers[i].health <= 0.0) {
            finish_game(i);
            return;
        }
    }
}

API void game_select(int id) {
    if (id < 0 || id >= philosopher_count) return;
    selected_id = id;
    Philosopher* philosopher = selected_philosopher();
    if (!round_over && philosopher->held_count > 0) {
        bool finished_meal = philosopher->state == STATE_EATING;
        release_resources(philosopher);
        philosopher->state = STATE_THINKING;
        if (finished_meal) {
            score += 100;
            if (!philosopher->fed) {
                philosopher->fed = true;
                fed_count++;
            }
            char feedback[192];
            snprintf(feedback, sizeof(feedback), "P%d finished eating and released both chopsticks. +100 points.", philosopher->id + 1);
            set_message(feedback, 0);
        } else {
            char feedback[128];
            snprintf(feedback, sizeof(feedback), "P%d released their held chopstick.", philosopher->id + 1);
            set_message(feedback, 0);
        }
        update_deadlock();
        return;
    }
    if (running && !round_over && !deadlocked && philosopher->state == STATE_THINKING) philosopher->state = STATE_HUNGRY;
    if (running && !round_over && !deadlocked && philosopher->state != STATE_EATING) take_both_chopsticks();
}

API void game_make_hungry(void) {
    Philosopher* philosopher = selected_philosopher();
    if (!running || round_over || deadlocked) return;
    if (philosopher->state != STATE_THINKING) {
        char feedback[128];
        const char* state = philosopher->state == STATE_HUNGRY ? "hungry" : philosopher->state == STATE_WAITING ? "waiting" : "eating";
        snprintf(feedback, sizeof(feedback), "P%d is already %s.", philosopher->id + 1, state);
        penalize_conflict(feedback);
        return;
    }
    philosopher->state = STATE_HUNGRY;
    char feedback[128];
    snprintf(feedback, sizeof(feedback), "P%d is hungry. Choose one of their neighboring chopsticks.", philosopher->id + 1);
    set_message(feedback, 0);
}

API void game_take_left(void) { acquire_chopstick(true); }
API void game_take_right(void) { acquire_chopstick(false); }
API void game_take_both(void) { take_both_chopsticks(); }

API int game_count(void) { return philosopher_count; }
API int game_selected(void) { return selected_id; }
API int game_state(int id) { return id >= 0 && id < philosopher_count ? philosophers[id].state : STATE_THINKING; }
API int game_health(int id) { return id >= 0 && id < philosopher_count ? (int)(philosophers[id].health + 0.5) : 0; }
API int game_fed(int id) { return id >= 0 && id < philosopher_count && philosophers[id].fed; }
API int game_held_count(int id) { return id >= 0 && id < philosopher_count ? philosophers[id].held_count : 0; }
API int game_held(int id, int slot) { return id >= 0 && id < philosopher_count && slot >= 0 && slot < 2 ? philosophers[id].held[slot] : -1; }
API int game_waiting_for(int id) { return id >= 0 && id < philosopher_count ? philosophers[id].waiting_for : -1; }
API int game_chopstick_owner(int id) { return id >= 0 && id < chopstick_count ? chopsticks[id] : -1; }
API int game_score(void) { return score; }
API int game_time(void) { return elapsed_ticks / 2; }
API int game_fed_count(void) { return fed_count; }
API int game_conflicts(void) { return conflicts; }
API int game_running(void) { return running; }
API int game_started(void) { return started; }
API int game_over(void) { return round_over; }
API int game_deadlocked(void) { return deadlocked; }
API int game_deadlock_occurred(void) { return deadlock_occurred; }
API int game_message_type(void) { return message_type; }
API const char* game_message(void) { return message; }
API const char* game_result_title(void) { return result_title; }
API const char* game_result_feedback(void) { return result_feedback; }

#ifndef __EMSCRIPTEN__
int main(void) {
    game_init(5);
    game_start();
    game_tick();
    printf("Game Mode C engine initialized: %d philosophers, state=%d, health=%d%%.\n",
        game_count(), game_state(0), game_health(0));
    return 0;
}
#endif