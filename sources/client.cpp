
#include <iostream>
#include <ncurses.h>
#include <nlohmann/json.hpp>

#include <string>
#include <set>
#include <map>
#include <vector>

enum class UIMode {
    InputMenu,
    TaskOperationMenu
};
int selectedTaskIndex = 0;

std::string formatDuration(double seconds) {
    int days = seconds / 86400;
    seconds -= days * 86400;
    int hours = seconds / 3600;
    seconds -= hours * 3600;
    int minutes = seconds / 60;
    int secs = static_cast<int>(seconds) % 60;

    char buffer[32];
    snprintf(buffer, sizeof(buffer), "%02d:%02d:%02d:%02d", days, hours, minutes, secs);
    return std::string(buffer);
}

UIMode currentMode = UIMode::InputMenu;

struct Task {
    std::string label;
    int fgColor;
    int bgColor;
    int colorPairId;
};
std::vector<Task> tasks;


/*
std::string formatDuration(double seconds) {
    int days = seconds / 86400;
    seconds -= days * 86400;
    int hours = seconds / 3600;
    seconds -= hours * 3600;
    int minutes = seconds / 60;
    seconds -= minutes * 60;

    char buffer[32];
    snprintf(buffer, sizeof(buffer), "%02d:%02d:%02d:%02d", days, hours, minutes, (int)seconds);
    return std::string(buffer);
}
*/


std::map<std::string, int> colorMap = {
    {"Default", -1},  // Use default terminal color
    {"Black", COLOR_BLACK},
    {"Red", COLOR_RED},
    {"Green", COLOR_GREEN},
    {"Yellow", COLOR_YELLOW},
    {"Blue", COLOR_BLUE},
    {"Magenta", COLOR_MAGENTA},
    {"Cyan", COLOR_CYAN},
    {"White", COLOR_WHITE},
    {"BrightBlack", COLOR_BLACK + 8},
    {"BrightRed", COLOR_RED + 8},
    {"BrightGreen", COLOR_GREEN + 8},
    {"BrightYellow", COLOR_YELLOW + 8},
    {"BrightBlue", COLOR_BLUE + 8},
    {"BrightMagenta", COLOR_MAGENTA + 8},
    {"BrightCyan", COLOR_CYAN + 8},
    {"BrightWhite", COLOR_WHITE + 8}
};
int colorPairCounter = 1;




class TimePeriod {
public:
    using TimePoint = std::chrono::system_clock::time_point;
public:
    std::string taskID = "";
    //time to time (i.e. 10 AM to 10:10 AM, in form of list, so like from 0 to n times )
    std::vector<std::pair<TimePoint, TimePoint>> segments;

    std::optional<TimePoint> currentlyRunning;
public:
    TimePeriod(const std::string& id) : taskID(id) {}

    void start();
    void pause();
    void stop(); // optional: stop = final pause

    double getTotalDurationSeconds() const; // returns sum of durations
    inline const std::vector<std::pair<TimePoint, TimePoint>>& getSegments() const {
        return segments; 
    }
};
void TimePeriod::start() {
    if (!currentlyRunning.has_value())
        currentlyRunning = std::chrono::system_clock::now();
}
void TimePeriod::pause() {
    if (currentlyRunning.has_value()) {
        auto now = std::chrono::system_clock::now();
        segments.emplace_back(*currentlyRunning, now);
        currentlyRunning.reset();
    }
}
void TimePeriod::stop() {
    pause(); // reuse pause for stop
}
double TimePeriod::getTotalDurationSeconds() const {
    double total = 0.0;
    for (const auto& [start, end] : segments) {
        total += std::chrono::duration<double>(end - start).count();
    }
    if (currentlyRunning.has_value()) {
        total += std::chrono::duration<double>(
            std::chrono::system_clock::now() - *currentlyRunning
        ).count();
    }
    return total;
}

class TasksTimer {
public:
    TasksTimer();

    void AddTask(const std::string& taskID);
    bool EraseTask(const std::string& taskID);

    void StartTask(const std::string& taskID);
    void PauseTask(const std::string& taskID);
    void StopTask(const std::string& taskID);

    std::shared_ptr<TimePeriod> GetTask(const std::string& taskID);

    inline const std::set<std::string>& GetAllTasksLabels() {
        return currentTasks;
    }
    inline const std::string& GetCurrentTaskLabel() const {
        return currentTask;
    }
protected:
    std::string currentTask = "";
    std::set<std::string> currentTasks;
    std::map<std::string, std::shared_ptr<TimePeriod>> taskPeriods;
};
TasksTimer tasksTimer;

TasksTimer::TasksTimer() {}

void TasksTimer::AddTask(const std::string& taskID) {
    if (currentTasks.count(taskID) == 0) {
        currentTasks.insert(taskID);
        taskPeriods[taskID] = std::make_shared<TimePeriod>(taskID);
    }
}

bool TasksTimer::EraseTask(const std::string& taskID) {
    if (currentTasks.erase(taskID)) {
        taskPeriods.erase(taskID);
        return true;
    }
    return false;
}

void TasksTimer::StartTask(const std::string& taskID) {
    if (taskPeriods.count(taskID)) {
        if (!currentTask.empty() && currentTask != taskID)
            PauseTask(currentTask);  // Pause current task
        currentTask = taskID;
        taskPeriods[taskID]->start();
    }
}

void TasksTimer::PauseTask(const std::string& taskID) {
    if (taskPeriods.count(taskID)) {
        taskPeriods[taskID]->pause();
        if (currentTask == taskID)
            currentTask = "";
    }
}

void TasksTimer::StopTask(const std::string& taskID) {
    PauseTask(taskID);
}


std::shared_ptr<TimePeriod> TasksTimer::GetTask(const std::string& taskID) {
    if (taskPeriods.count(taskID))
        return taskPeriods[taskID];
    return nullptr;
}



void addDefaultTasks() {
    struct DefaultTaskSpec {
        std::string fg;
        std::string bg;
        std::string label;
    };

    std::vector<DefaultTaskSpec> defaultSpecs = {
        {"White", "Blue", "Programming"},
        {"BrightGreen", "Default", "Reading"},
        {"Default", "BrightRed", "Alert"}
    };

    for (const auto& spec : defaultSpecs) {
        if (colorMap.count(spec.fg) && colorMap.count(spec.bg)) {
            int fg = colorMap[spec.fg];
            int bg = colorMap[spec.bg];
            if ((fg >= 0 && fg < COLORS) || fg == -1) {
                if ((bg >= 0 && bg < COLORS) || bg == -1) {
                    init_pair(colorPairCounter, fg, bg);
                }
            }
            tasks.push_back(Task{spec.label, fg, bg, colorPairCounter++});
            tasksTimer.AddTask(spec.label);
        }
    }
}

void redrawTop(WINDOW* topWin) {
    werase(topWin);
    box(topWin, 0, 0);
    mvwprintw(topWin, 0, 2, " Tasks ");

    int y = 1;
    for (const auto& task : tasks) {
        wattron(topWin, COLOR_PAIR(task.colorPairId));
        mvwprintw(topWin, y++, 2, "%s", task.label.c_str());
        wattroff(topWin, COLOR_PAIR(task.colorPairId));

        std::shared_ptr<TimePeriod> tp = tasksTimer.GetTask(task.label);
        if (tp) {
            // Total Time
            std::string total = formatDuration(tp->getTotalDurationSeconds());
            mvwprintw(topWin, y++, 4, "Total: %s", total.c_str());

            // Active Time (recent)
            std::string recent = "00:00:00:00";
            if (tp->currentlyRunning.has_value()) {
                double recentSecs = std::chrono::duration<double>(
                    std::chrono::system_clock::now() - tp->currentlyRunning.value()
                ).count();
                recent = formatDuration(recentSecs);
            }
            mvwprintw(topWin, y++, 4, "Active: %s", recent.c_str());
        } else {
            y += 2;
        }

        y++; // spacing between tasks
    }

    wrefresh(topWin);
}

bool parseNewTask(const std::string& input) {
    size_t start = input.find("(");
    size_t end = input.find(")");
    if (start == std::string::npos || end == std::string::npos || end <= start + 1) return false;

    std::string args = input.substr(start + 1, end - start - 1);
    std::istringstream iss(args);
    std::string fgStr, bgStr, label;

    if (!std::getline(iss, fgStr, ',') || !std::getline(iss, bgStr, ',') || !std::getline(iss, label)) return false;

    auto trim = [](std::string& s) {
        s.erase(0, s.find_first_not_of(" \t"));
        s.erase(s.find_last_not_of(" \t") + 1);
    };
    trim(fgStr); trim(bgStr); trim(label);

    if (colorMap.count(fgStr) == 0 || colorMap.count(bgStr) == 0 || label.empty()) return false;

    int fg = colorMap[fgStr];
    int bg = colorMap[bgStr];

    if ((fg >= 0 && fg < COLORS) || fg == -1) {
        if ((bg >= 0 && bg < COLORS) || bg == -1) {
            init_pair(colorPairCounter, fg, bg);
        }
    }
    tasks.push_back(Task{label, fg, bg, colorPairCounter++});
    return true;
}

bool removeTask(const std::string& label) {
    auto it = std::remove_if(tasks.begin(), tasks.end(), [&](const Task& t) {
        return t.label == label;
    });

    if (it != tasks.end()) {
        tasks.erase(it, tasks.end());
        return true;
    }
    return false;
}
void handleInput(const std::string& input, WINDOW* bottomWin, WINDOW* topWin) {
    if (input.find("NewTask(") == 0) {
        if (parseNewTask(input)) {
            mvwprintw(bottomWin, 2, 2, "Task created.");
        } else {
            mvwprintw(bottomWin, 2, 2, "Syntax error.");
        }
    } else if (input.find("RemoveTask(") == 0) {
        size_t start = input.find("(");
        size_t end = input.find(")");
        if (start != std::string::npos && end != std::string::npos) {
            std::string label = input.substr(start + 1, end - start - 1);
            if (removeTask(label)) {
                mvwprintw(bottomWin, 2, 2, "Task removed.");
            } else {
                mvwprintw(bottomWin, 2, 2, "Not found.");
            }
        }
    } else if (input.find("TaskInfo(") == 0) {
        mvwprintw(bottomWin, 2, 2, "TaskInfo not implemented.");
    } else {
        mvwprintw(bottomWin, 2, 2, "Unknown command.");
    }
    redrawTop(topWin);
    wrefresh(bottomWin);
}

void updateBottomPanel(WINDOW* bottomWin) {
    werase(bottomWin);
    box(bottomWin, 0, 0);

    if (currentMode == UIMode::InputMenu) {
        mvwprintw(bottomWin, 1, 2, "Input > ");
    } else if (currentMode == UIMode::TaskOperationMenu) {
        mvwprintw(bottomWin, 1, 2, "Task Menu:");
        mvwprintw(bottomWin, 2, 2, "[Arrows] Select  [S] Start  [P] Pause  [T] Stop  [ESC] Back");

        if (!tasks.empty()) {
            const Task& task = tasks[selectedTaskIndex % tasks.size()];
            wattron(bottomWin, COLOR_PAIR(task.colorPairId));
            mvwprintw(bottomWin, 3, 2, "Selected: %s", task.label.c_str());
            wattroff(bottomWin, COLOR_PAIR(task.colorPairId));
        } else {
            mvwprintw(bottomWin, 3, 2, "No tasks available");
        }
    }

    wrefresh(bottomWin);
}

int main(int argc, char* argv[]) {
    set_escdelay(200);
    initscr();
    start_color();
    use_default_colors();
    cbreak();             // Disable line buffering
    noecho();             // Don’t echo typed chars
    keypad(stdscr, TRUE); // Enable arrow keys and function keys
    timeout(1000); // waits 1 second max between redraws
    curs_set(1);          // Show cursor

    if (COLORS >= 16) {
        // Terminal supports bright colors
    } else {
        mvprintw(0, 0, "Warning: Terminal doesn't support extended colors.");
        refresh();
    }


    int height, width;
    getmaxyx(stdscr, height, width);

    int topHeight = height * 0.7;
    int bottomHeight = height - topHeight;

    WINDOW* topWin = newwin(topHeight, width, 0, 0);
    WINDOW* bottomWin = newwin(bottomHeight, width, topHeight, 0);

    box(topWin, 0, 0);
    box(bottomWin, 0, 0);

    mvwprintw(topWin, 1, 2, "Task Overview");
    updateBottomPanel(bottomWin);

    wrefresh(topWin);
    wrefresh(bottomWin);
    redrawTop(topWin); // ⬅️ will update every second, even with no input

    addDefaultTasks();     // ← Add this
    redrawTop(topWin);     // Also make sure to call this after default task setup
    keypad(bottomWin, TRUE);
    std::string input;
    int ch;
    while (true) {
        ch = wgetch(bottomWin);
        if (ch == ERR) {
            // No input received during timeout — update UI
            redrawTop(topWin);
            continue;
        }
        if (ch == 27) { // ESC key toggles menu
            if (currentMode == UIMode::InputMenu) {
                currentMode = UIMode::TaskOperationMenu;
            } else {
                currentMode = UIMode::InputMenu;
            }
            input.clear();
            updateBottomPanel(bottomWin);
            continue;
        }

        // 🟩 Insert this here:
        if (currentMode == UIMode::InputMenu) {
            if (ch == '\n') {
                handleInput(input, bottomWin, topWin);
                input.clear();
                mvwprintw(bottomWin, 1, 10, "                    ");
                wmove(bottomWin, 1, 10);
            } else if (ch == KEY_BACKSPACE || ch == 127) {
                if (!input.empty()) {
                    input.pop_back();
                    int y, x; getyx(bottomWin, y, x);
                    mvwaddch(bottomWin, y, x - 1, ' ');
                    wmove(bottomWin, y, x - 1);
                }
            } else if (isprint(ch)) {
                input.push_back((char)ch);
                waddch(bottomWin, ch);
            }
        } else if (currentMode == UIMode::TaskOperationMenu) {
            // For now: just log that we're in task operation mode
        if (ch == KEY_UP) {
            if (!tasks.empty()) {
                selectedTaskIndex = (selectedTaskIndex - 1 + tasks.size()) % tasks.size();
                updateBottomPanel(bottomWin);
            }
        } else if (ch == KEY_DOWN) {
            if (!tasks.empty()) {
                selectedTaskIndex = (selectedTaskIndex + 1) % tasks.size();
                updateBottomPanel(bottomWin);
            }
        } else if (ch == 'S' || ch == 's') {
            if (!tasks.empty()) {
                std::string label = tasks[selectedTaskIndex].label;
                tasksTimer.StartTask(label);
                mvwprintw(bottomWin, 4, 2, "Started: %s", label.c_str());
            }
        } else if (ch == 'P' || ch == 'p') {
            if (!tasks.empty()) {
                std::string label = tasks[selectedTaskIndex].label;
                tasksTimer.PauseTask(label);
                mvwprintw(bottomWin, 4, 2, "Paused: %s", label.c_str());
            }
        } else if (ch == 'T' || ch == 't') {
            if (!tasks.empty()) {
                std::string label = tasks[selectedTaskIndex].label;
                tasksTimer.StopTask(label);
                mvwprintw(bottomWin, 4, 2, "Stopped: %s", label.c_str());
            }
        }
        }

        wrefresh(bottomWin);
        redrawTop(topWin); // ⬅️ will update every second, even with no input
    }


    // Clean up
    delwin(topWin);
    delwin(bottomWin);
    endwin();

    return 0;
}


