
#include <iostream>
#include <ncurses.h>
#include <nlohmann/json.hpp>

#include <string>
#include <set>
#include <map>

class TimePeriod {
public:
    using TimePoint = std::chrono::system_clock::time_point;
protected:
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


int main(int argc, char* argv[]) {
    initscr();
    cbreak();             // Disable line buffering
    noecho();             // Don’t echo typed chars
    keypad(stdscr, TRUE); // Enable arrow keys and function keys
    curs_set(1);          // Show cursor

    int height, width;
    getmaxyx(stdscr, height, width);

    int topHeight = height * 0.7;
    int bottomHeight = height - topHeight;

    WINDOW* topWin = newwin(topHeight, width, 0, 0);
    WINDOW* bottomWin = newwin(bottomHeight, width, topHeight, 0);

    box(topWin, 0, 0);
    box(bottomWin, 0, 0);

    mvwprintw(topWin, 1, 2, "Task Overview");
    mvwprintw(bottomWin, 1, 2, "Input > ");

    wrefresh(topWin);
    wrefresh(bottomWin);

    // Input loop
    int ch;
    std::string input;

    while ((ch = wgetch(bottomWin)) != 27) { // Escape key = 27
        if (ch == '\n') {
            // Process input
            mvwprintw(bottomWin, 2, 2, "You entered: %s", input.c_str());
            input.clear();
            mvwprintw(bottomWin, 1, 9, "                    "); // Clear input line
            wmove(bottomWin, 1, 9); // Move cursor back
        } else if (ch == KEY_BACKSPACE || ch == 127) {
            if (!input.empty()) {
                input.pop_back();
                int x, y;
                getyx(bottomWin, y, x);
                mvwaddch(bottomWin, y, x - 1, ' ');
                wmove(bottomWin, y, x - 1);
            }
        } else if (ch == KEY_UP || ch == KEY_DOWN || ch == KEY_LEFT || ch == KEY_RIGHT) {
            mvwprintw(bottomWin, 3, 2, "Arrow key pressed: %d  ", ch);
        } else if (ch == KEY_NPAGE) {
            mvwprintw(bottomWin, 3, 2, "Page Down key");
        } else if (ch == KEY_PPAGE) {
            mvwprintw(bottomWin, 3, 2, "Page Up key");
        } else if (isprint(ch)) {
            input.push_back((char)ch);
            waddch(bottomWin, ch);
        }

        wrefresh(bottomWin);
    }

    // Clean up
    delwin(topWin);
    delwin(bottomWin);
    endwin();

    return 0;
}


