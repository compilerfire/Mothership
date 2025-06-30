#include <ncurses.h>
#include <string>
#include <vector>
#include <map>
#include <ctime>
#include <chrono>
#include <sstream>
#include <algorithm>
#include <thread>
#include <cctype>
#include <set>

using namespace std;
using namespace std::chrono;

struct Timeframe {
    time_t start;
    time_t end;
};

struct Task {
    string name;
    vector<Timeframe> timeframes;
    int color_pair = 1;
    bool active = false;
    time_t last_start = 0;
};

vector<Task> tasks;
vector<string> command_history;
int history_index = -1;

string command_input;
bool running = true;
int palette_height;
vector<string> command_list = {"insert", "color", "erase", "rename", "switch", "pause"};
vector<string> color_names = {"red", "green", "blue", "white"};
enum InputPhase { COMMAND, TASK_NAME, ARGUMENT, IDLE };
InputPhase current_phase = COMMAND;

string active_command;
string active_task;
vector<string> matches;
int selected_index = 0;

// Color enums
enum ColorEnum {
    DEFAULT = 1,
    BRIGHT_WHITE,
    RED,
    GREEN,
    BLUE
};

void init_colors() {
    start_color();
    use_default_colors();
    init_pair(DEFAULT, COLOR_WHITE, -1);
    init_pair(BRIGHT_WHITE, COLOR_WHITE, COLOR_BLACK);
    init_pair(RED, COLOR_RED, -1);
    init_pair(GREEN, COLOR_GREEN, -1);
    init_pair(BLUE, COLOR_BLUE, -1);
}

time_t now() { return time(nullptr); }

string format_duration(int seconds) {
    int mins = seconds / 60;
    int secs = seconds % 60;
    char buffer[16];
    snprintf(buffer, sizeof(buffer), "%02d:%02d", mins, secs);
    return string(buffer);
}

int total_time(const Task& task) {
    int sum = 0;
    for (const auto& tf : task.timeframes)
        sum += tf.end - tf.start;
    if (task.active)
        sum += now() - task.last_start;
    return sum;
}

void update_matches() {
    matches.clear();
    if (current_phase == COMMAND) {
        for (auto& cmd : command_list)
            if (cmd.find(command_input) == 0) matches.push_back(cmd);
    } else if (current_phase == TASK_NAME) {
        for (auto& task : tasks)
            if (task.name.find(command_input) == 0) matches.push_back(task.name);
    } else if (current_phase == ARGUMENT && active_command == "color") {
        for (auto& clr : color_names)
            if (clr.find(command_input) == 0) matches.push_back(clr);
    }
    if (selected_index >= matches.size()) selected_index = matches.empty() ? 0 : matches.size() - 1;
}

void draw_ui() {
    clear();
    int height, width;
    getmaxyx(stdscr, height, width);
    palette_height = height * 0.2;
    int task_area_height = height - palette_height;

    for (size_t i = 0; i < tasks.size(); ++i) {
        const Task& task = tasks[i];
        attron(COLOR_PAIR(task.color_pair));
        mvprintw(i, 0, "%s", task.name.c_str());
        string time_str = format_duration(total_time(task));
        mvprintw(i, width - time_str.length(), "%s", time_str.c_str());
        attroff(COLOR_PAIR(task.color_pair));
    }

    move(task_area_height + 1, 0);
    hline('-', width);
    mvprintw(task_area_height + 2, 2, "> %s", command_input.c_str());

    for (size_t i = 0; i < matches.size(); ++i) {
        if ((int)i == selected_index) attron(A_REVERSE);
        mvprintw(task_area_height + 3 + i, 4, "%s", matches[i].c_str());
        if ((int)i == selected_index) attroff(A_REVERSE);
    }

    refresh();
}

void insert_task(const string& name) {
    tasks.push_back({name, {}, BRIGHT_WHITE, false});
}

void erase_task(const string& name) {
    tasks.erase(remove_if(tasks.begin(), tasks.end(), [&](Task& t) {
        return t.name == name;
    }), tasks.end());
}

void rename_task(const string& name, const string& new_name) {
    for (auto& task : tasks)
        if (task.name == name) task.name = new_name;
}

void color_task(const string& name, const string& color) {
    int col = BRIGHT_WHITE;
    if (color == "red") col = RED;
    else if (color == "green") col = GREEN;
    else if (color == "blue") col = BLUE;
    for (auto& task : tasks)
        if (task.name == name) task.color_pair = col;
}

void activate_task(const string& name) {
    for (auto& t : tasks) {
        if (t.active) {
            t.timeframes.push_back({t.last_start, now()});
            t.active = false;
        }
    }
    for (auto& t : tasks)
        if (t.name == name) {
            t.active = true;
            t.last_start = now();
        }
}

void pause_task() {
    for (auto& task : tasks)
        if (task.active) {
            task.timeframes.push_back({task.last_start, now()});
            task.active = false;
        }
}

void run_command() {
    if (active_command == "insert") insert_task(active_task);
    else if (active_command == "erase") erase_task(active_task);
    else if (active_command == "switch") activate_task(active_task);
    else if (active_command == "pause") pause_task();
    else if (active_command == "rename") rename_task(active_task, command_input);
    else if (active_command == "color") color_task(active_task, command_input);

    command_history.push_back(active_command + " " + active_task + " " + command_input);
    command_input.clear();
    active_command.clear();
    active_task.clear();
    current_phase = COMMAND;
    selected_index = 0;
}

void main_loop() {
    mousemask(ALL_MOUSE_EVENTS, NULL);
    MEVENT event;

    while (running) {
        update_matches();
        draw_ui();
        int ch = getch();
        if (ch == 27) {
            running = false;
        } else if (ch == '\n') {
            if (!matches.empty()) command_input = matches[selected_index];
            if (current_phase == COMMAND) {
                active_command = command_input;
                command_input.clear();
                current_phase = (active_command == "pause") ? COMMAND : TASK_NAME;
            } else if (current_phase == TASK_NAME) {
                active_task = command_input;
                command_input.clear();
                current_phase = (active_command == "rename" || active_command == "color") ? ARGUMENT : IDLE;
                if (current_phase == IDLE) run_command();
            } else if (current_phase == ARGUMENT) {
                run_command();
            }
        } else if (ch == '\t') {
            if (!matches.empty()) {
                command_input = matches[selected_index];
            }
        } else if (ch == KEY_UP) {
            if (selected_index > 0) selected_index--;
            else if (!command_history.empty() && history_index + 1 < (int)command_history.size()) {
                history_index++;
                command_input = command_history[command_history.size() - 1 - history_index];
            }
        } else if (ch == KEY_DOWN) {
            if ((size_t)selected_index < matches.size() - 1) selected_index++;
            else if (history_index > 0) {
                history_index--;
                command_input = command_history[command_history.size() - 1 - history_index];
            }
        } else if (ch == KEY_MOUSE) {
            if (getmouse(&event) == OK) {
                int height, width;
                getmaxyx(stdscr, height, width);
                int y = event.y - (height - palette_height + 3);
                if (y >= 0 && (size_t)y < matches.size()) {
                    selected_index = y;
                }
            }
        } else if (ch == KEY_BACKSPACE || ch == 127 || ch == 8) {
            if (!command_input.empty()) command_input.pop_back();
        } else if (isprint(ch)) {
            command_input += ch;
            selected_index = 0;
        }

        this_thread::sleep_for(milliseconds(30));
    }
}

int main() {
    initscr();
    noecho();
    cbreak();
    keypad(stdscr, TRUE);
    curs_set(1);
    init_colors();
    main_loop();
    endwin();
    return 0;
}