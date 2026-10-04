#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <math.h>

#ifdef __EMSCRIPTEN__
#define printf(...) ((void)0)
#endif

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

typedef struct {
    int id;
    int left;
    int right;
    char state[16];
    int held[2];
    int held_count;
    int waiting_for;
    int eat_ticks;
    int think_ticks;
} Philosopher;

typedef struct {
    char title[128];
    char text[512];
} StrategyInfo;

typedef struct {
    char value[32];
} StrategySelect;

typedef struct {
    int value;
} CountInput;

static CountInput count_input = { 5 };
static StrategySelect strategy_select = { "unsafe" };
static Philosopher philosophers[10];
static int philosopher_count = 0;
static int chopsticks[10];
static int chopstick_count = 0;
static int step_number = 0;
static bool running = false;
static bool deadlocked = false;

static StrategyInfo strategy_unsafe = {
    "Naive acquisition",
    "Every philosopher takes the left chopstick first. They can each hold one resource while waiting for the next, completing a circular wait."
};

static StrategyInfo strategy_limit = {
    "At most N - 1 contenders",
    "A philosopher may begin taking resources only while fewer than N - 1 philosophers already hold chopsticks. Someone can always make progress and free resources."
};

static StrategyInfo strategy_ordering = {
    "Global resource ordering",
    "Each philosopher takes the lower-numbered neighboring chopstick first. Every wait edge points toward a higher resource, so a cycle cannot form."
};

static StrategyInfo strategy_waiter = {
    "Arbitrator / waiter",
    "A central waiter grants both chopsticks together only when both are free. Philosophers never hold one chopstick while waiting for another."
};

static StrategyInfo strategy_asymmetric = {
    "Asymmetric acquisition",
    "Even-numbered philosophers take left first; odd-numbered philosophers take right first. The mixed order prevents a uniform circular wait."
};

static StrategyInfo* get_strategy(const char* key) {
    if (strcmp(key, "unsafe") == 0) return &strategy_unsafe;
    if (strcmp(key, "limit") == 0) return &strategy_limit;
    if (strcmp(key, "ordering") == 0) return &strategy_ordering;
    if (strcmp(key, "waiter") == 0) return &strategy_waiter;
    if (strcmp(key, "asymmetric") == 0) return &strategy_asymmetric;
    return &strategy_unsafe;
}

static Philosopher create_philosopher(int id, int count) {
    Philosopher p;
    p.id = id;
    p.left = (id + count - 1) % count;
    p.right = id;
    strncpy(p.state, "Thinking", sizeof(p.state) - 1);
    p.state[sizeof(p.state) - 1] = '\0';
    p.held_count = 0;
    p.held[0] = -1;
    p.held[1] = -1;
    p.waiting_for = -1;
    p.eat_ticks = 0;
    p.think_ticks = 0;
    return p;
}

static void build_table(void) {
    int count = count_input.value;
    if (count < 2) count = 2;
    if (count > 10) count = 10;
    count_input.value = count;
    philosopher_count = count;
    chopstick_count = count;

    for (int id = 0; id < count; id++) {
        philosophers[id] = create_philosopher(id, count);
        chopsticks[id] = -1;
    }
}

static void acquisition_order(Philosopher* philosopher, int* out_hands) {
    out_hands[0] = philosopher->left;
    out_hands[1] = philosopher->right;

    if (strcmp(strategy_select.value, "ordering") == 0) {
        if (out_hands[0] > out_hands[1]) {
            int temp = out_hands[0];
            out_hands[0] = out_hands[1];
            out_hands[1] = temp;
        }
        return;
    }

    if (strcmp(strategy_select.value, "asymmetric") == 0 && philosopher->id % 2 == 1) {
        int temp = out_hands[0];
        out_hands[0] = out_hands[1];
        out_hands[1] = temp;
    }
}

static void begin_eating(Philosopher* philosopher) {
    strncpy(philosopher->state, "Eating", sizeof(philosopher->state) - 1);
    philosopher->state[sizeof(philosopher->state) - 1] = '\0';
    philosopher->waiting_for = -1;
    philosopher->eat_ticks = 3;
    printf("P%d acquired both chopsticks and entered the critical section.\n", philosopher->id + 1);
}

static void release_resources(Philosopher* philosopher) {
    for (int i = 0; i < philosopher->held_count; i++) {
        int index = philosopher->held[i];
        if (chopsticks[index] == philosopher->id) {
            chopsticks[index] = -1;
        }
    }
    philosopher->held_count = 0;
}

static void release_after_eating(Philosopher* philosopher) {
    release_resources(philosopher);
    strncpy(philosopher->state, "Thinking", sizeof(philosopher->state) - 1);
    philosopher->state[sizeof(philosopher->state) - 1] = '\0';
    philosopher->waiting_for = -1;
    philosopher->think_ticks = 2;
    printf("P%d finished eating and released both chopsticks.\n", philosopher->id + 1);
}

static void request_resources(Philosopher* philosopher) {
    if (strcmp(strategy_select.value, "waiter") == 0) {
        bool someone_else_holds = false;
        for (int i = 0; i < philosopher_count; i++) {
            if (philosophers[i].id != philosopher->id && philosophers[i].held_count > 0) {
                someone_else_holds = true;
                break;
            }
        }

        if (someone_else_holds) {
            strncpy(philosopher->state, "Waiting", sizeof(philosopher->state) - 1);
            philosopher->state[sizeof(philosopher->state) - 1] = '\0';
            philosopher->waiting_for = -1;
            return;
        }

        if (chopsticks[philosopher->left] == -1 && chopsticks[philosopher->right] == -1) {
            chopsticks[philosopher->left] = philosopher->id;
            chopsticks[philosopher->right] = philosopher->id;
            philosopher->held[0] = philosopher->left;
            philosopher->held[1] = philosopher->right;
            philosopher->held_count = 2;
            begin_eating(philosopher);
        } else {
            strncpy(philosopher->state, "Waiting", sizeof(philosopher->state) - 1);
            philosopher->state[sizeof(philosopher->state) - 1] = '\0';
        }
        return;
    }

    int order[2];
    acquisition_order(philosopher, order);
    int next_stick = philosopher->held_count == 0
        ? order[0]
        : (philosopher->left == philosopher->held[0] ? philosopher->right : philosopher->left);

    if (strcmp(strategy_select.value, "limit") == 0 && philosopher->held_count == 0) {
        int holders = 0;
        for (int i = 0; i < philosopher_count; i++) {
            if (philosophers[i].held_count > 0) holders++;
        }
        if (holders >= philosopher_count - 1) {
            strncpy(philosopher->state, "Waiting", sizeof(philosopher->state) - 1);
            philosopher->state[sizeof(philosopher->state) - 1] = '\0';
            philosopher->waiting_for = -1;
            return;
        }
    }

    if (chopsticks[next_stick] != -1 && chopsticks[next_stick] != philosopher->id) {
        strncpy(philosopher->state, "Waiting", sizeof(philosopher->state) - 1);
        philosopher->state[sizeof(philosopher->state) - 1] = '\0';
        philosopher->waiting_for = next_stick;
        return;
    }

    chopsticks[next_stick] = philosopher->id;
    bool already_held = false;
    for (int i = 0; i < philosopher->held_count; i++) {
        if (philosopher->held[i] == next_stick) {
            already_held = true;
            break;
        }
    }
    if (!already_held) philosopher->held[philosopher->held_count++] = next_stick;

    if (philosopher->held_count == 2) {
        begin_eating(philosopher);
    } else {
        strncpy(philosopher->state, "Waiting", sizeof(philosopher->state) - 1);
        philosopher->state[sizeof(philosopher->state) - 1] = '\0';
        philosopher->waiting_for = philosopher->left == next_stick ? philosopher->right : philosopher->left;
        printf("P%d holds C%d and waits for C%d.\n", philosopher->id + 1, next_stick + 1, philosopher->waiting_for + 1);
    }
}

static bool detect_deadlock(void) {
    if (philosopher_count < 2) return false;

    for (int i = 0; i < philosopher_count; i++) {
        if (strcmp(philosophers[i].state, "Waiting") != 0 || philosophers[i].held_count == 0 || philosophers[i].waiting_for == -1) {
            return false;
        }
    }

    int current = 0;
    bool visited[10] = { false };
    int visited_count = 0;

    for (int step = 0; step < philosopher_count; step++) {
        if (visited[current]) return false;
        visited[current] = true;
        visited_count++;
        int owner = chopsticks[philosophers[current].waiting_for];
        if (owner == -1 || owner == current) return false;
        current = owner;
    }

    return current == 0 && visited_count == philosopher_count;
}

static void render(void) {
    int eating_count = 0;
    int waiting_count = 0;
    int free_count = 0;

    for (int i = 0; i < philosopher_count; i++) {
        if (strcmp(philosophers[i].state, "Eating") == 0) eating_count++;
        if (strcmp(philosophers[i].state, "Waiting") == 0) waiting_count++;
    }

    for (int i = 0; i < chopstick_count; i++) {
        if (chopsticks[i] == -1) free_count++;
    }

    printf("Step %d | eating=%d waiting=%d free=%d deadlock=%s\n",
           step_number, eating_count, waiting_count, free_count, deadlocked ? "yes" : "no");

    for (int i = 0; i < philosopher_count; i++) {
        printf("P%d: %s | held=%d/%d | waiting_for=%d\n",
               philosophers[i].id + 1,
               philosophers[i].state,
               philosophers[i].held_count,
               2,
               philosophers[i].waiting_for);
    }
}

static void advance_simulation(void) {
    step_number += 1;

    for (int i = 0; i < philosopher_count; i++) {
        Philosopher* philosopher = &philosophers[i];
        if (strcmp(philosopher->state, "Eating") == 0) {
            philosopher->eat_ticks -= 1;
            if (philosopher->eat_ticks <= 0) {
                release_after_eating(philosopher);
            }
        } else if (strcmp(philosopher->state, "Thinking") == 0) {
            philosopher->think_ticks -= 1;
            if (philosopher->think_ticks <= 0) {
                strncpy(philosopher->state, "Hungry", sizeof(philosopher->state) - 1);
                philosopher->state[sizeof(philosopher->state) - 1] = '\0';
            }
        }
    }

    for (int i = 0; i < philosopher_count; i++) {
        Philosopher* philosopher = &philosophers[i];
        if (strcmp(philosopher->state, "Hungry") == 0 || strcmp(philosopher->state, "Waiting") == 0) {
            request_resources(philosopher);
        }
    }

    deadlocked = detect_deadlock();
    render();

    if (deadlocked) {
        printf("Deadlock detected.\n");
    }
}

__attribute__((visibility("default")))
void normal_init(int count, int strategy) {
    static const char* strategy_keys[] = { "unsafe", "limit", "ordering", "waiter", "asymmetric" };
    count_input.value = count;
    if (strategy < 0 || strategy >= 5) strategy = 0;
    strncpy(strategy_select.value, strategy_keys[strategy], sizeof(strategy_select.value) - 1);
    strategy_select.value[sizeof(strategy_select.value) - 1] = '\0';
    build_table();
    step_number = 0;
    running = false;
    deadlocked = false;
}

__attribute__((visibility("default")))
void normal_start(void) {
    if (deadlocked) return;
    if (step_number == 0) {
        for (int i = 0; i < philosopher_count; i++) {
            strncpy(philosophers[i].state, "Hungry", sizeof(philosophers[i].state) - 1);
            philosophers[i].state[sizeof(philosophers[i].state) - 1] = '\0';
        }
    }
    running = true;
}

__attribute__((visibility("default")))
void normal_pause(void) {
    running = false;
}

__attribute__((visibility("default")))
void normal_step(void) {
    if (!running || deadlocked) return;
    advance_simulation();
}

__attribute__((visibility("default")))
int normal_count(void) { return philosopher_count; }

__attribute__((visibility("default")))
int normal_running(void) { return running; }

__attribute__((visibility("default")))
int normal_deadlocked(void) { return deadlocked; }

__attribute__((visibility("default")))
int normal_step_number(void) { return step_number; }

__attribute__((visibility("default")))
int normal_state(int id) {
    if (id < 0 || id >= philosopher_count) return 0;
    if (strcmp(philosophers[id].state, "Hungry") == 0) return 1;
    if (strcmp(philosophers[id].state, "Waiting") == 0) return 2;
    if (strcmp(philosophers[id].state, "Eating") == 0) return 3;
    return 0;
}

__attribute__((visibility("default")))
int normal_held(int id, int slot) {
    if (id < 0 || id >= philosopher_count || slot < 0 || slot >= 2) return -1;
    return philosophers[id].held[slot];
}

__attribute__((visibility("default")))
int normal_waiting_for(int id) {
    if (id < 0 || id >= philosopher_count) return -1;
    return philosophers[id].waiting_for;
}

__attribute__((visibility("default")))
int normal_chopstick_owner(int id) {
    if (id < 0 || id >= chopstick_count) return -1;
    return chopsticks[id];
}

#ifndef __EMSCRIPTEN__
static void start_simulation(void) {
    running = true;
    StrategyInfo* strategy = get_strategy(strategy_select.value);
    printf("Strategy: %s\n%s\n", strategy->title, strategy->text);

    if (step_number == 0) {
        for (int i = 0; i < philosopher_count; i++) {
            strncpy(philosophers[i].state, "Hungry", sizeof(philosophers[i].state) - 1);
            philosophers[i].state[sizeof(philosophers[i].state) - 1] = '\0';
        }
    }

    for (int i = 0; i < 5; i++) {
        advance_simulation();
        if (deadlocked) break;
    }
}

int main(void) {
    build_table();
    printf("Simulation initialized with %d philosophers.\n", philosopher_count);
    start_simulation();
    return 0;
}
#endif
