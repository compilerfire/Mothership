#include <chrono>
#include <iostream>
#include <ctime>
#include <ncurses.h>
#include <map>

WINDOW* leftWin = nullptr;
WINDOW* rightWin = nullptr;
WINDOW* bottomWin = nullptr;

struct SingleTimeframe {
    std::chrono::time_point<std::chrono::system_clock> startTime, endTime;

    bool started = false;
    bool finished = false;

    SingleTimeframe();
    bool Start() {
        if (started) {
            return false;
        }
        startTime = std::chrono::system_clock::now();
        started = true;
        return true;
    }
    bool Stop() {
        if (finished || !started) {
            return false;
        }
        endTime = std::chrono::system_clock::now();
        finished = true;
        started = false;
        return true;
    }
    inline bool Finished() {
        return (finished);
    }
    inline bool Started() {
        return started;
    }
    inline std::chrono::duration<double> ElapsedTime() {
        if (!started && !finished)
            return std::chrono::duration<double>(0);
        return endTime - startTime;
    }
};
class MothershipColor {
public:
    bool isStandardColor = true;
    bool isCustomColor = false;

    int colorCode = -1;
    unsigned short r = -1;
    unsigned short g = -1;
    unsigned short b = -1;

    MothershipColor(int colorCode) {
        this->colorCode = colorCode;
    }
    MothershipColor(unsigned short r, unsigned short g, unsigned short b) {
        isCustomColor = true;
        isStandardColor = false;

        this->r = r;
        this->g = g;
        this->b = b;
    }
};
class MothershipTask {
public:

    std::string title = "The Task";
    std::map<int, SingleTimeframe> timeFrames;
    std::chrono::duration<double> totalTime;
    int currentTimeframe = -1;

    MothershipColor color;

    MothershipTask(const std::string& title, const MothershipColor& color) : title(title), color(color) {

    }

    inline void CalculateTotalTime() {
        totalTime = std::chrono::duration<double>();
        for (auto timeframe : timeFrames) {
            SingleTimeframe& tf = timeframe.second;
            if (tf.Finished()) {
                totalTime += tf.ElapsedTime();
            } else {
                if (tf.Started()) {
                    totalTime += std::chrono::system_clock::now() - tf.startTime;
                    return;
                } else {
                    continue;
                }
            }
        }
    }
    inline bool Stopped() {
        if (timeFrames.empty()) {return true;}
        if (currentTimeframe == -1) {return true;}
        if (currentTimeframe != -1 && currentTimeframe >= 0) {
            auto it = timeFrames.find(currentTimeframe);
            if (it != timeFrames.end()) {
                SingleTimeframe& stf = it->second;
                return stf.Finished();
            } else {
                return true;
            }
        } else {
            return true;
        }
    }
    inline bool Started() {
        if (timeFrames.empty()) {return false;}
        if (currentTimeframe == -1) {return false;}
        if (currentTimeframe != -1 && currentTimeframe >= 0) {
            auto it = timeFrames.find(currentTimeframe);
            if (it != timeFrames.end()) {
                SingleTimeframe& stf = it->second;
                return stf.Started();
            } else {
                return false;
            }
        } else {
            return false;
        }
    }
    bool StartTimeframe() {
        if (timeFrames.empty()) {
            auto ip = timeFrames.insert(std::make_pair(
                0, SingleTimeframe()
            ));
            this->currentTimeframe = 0;
            ip.first->second.Start();
            return true;
        }
        if (currentTimeframe >= 0) {
            auto it = timeFrames.find(currentTimeframe);
            if (it != timeFrames.end()) {
                bool hasStarted = it->second.Started();
                if (hasStarted) {
                    return false;
                } else {
                    bool hasFinished = it->second.Finished();
                    if (hasFinished) {
                        currentTimeframe += 1;
                        auto ip = timeFrames.insert(std::make_pair(
                            currentTimeframe,
                            SingleTimeframe()
                        ));
                        ip.first->second.Start();
                        return true;
                    } else {
                        it->second.Start();
                        return true;
                    }
                }
            } else {
                auto ip = timeFrames.insert(std::make_pair(
                    currentTimeframe, SingleTimeframe()
                ));
                ip.first->second.Start();
                return true;
            }
        }
        return false;
    }
    bool FinishTimeframe() {
        if (timeFrames.empty()) {
            return false;
        } else {
            if (currentTimeframe >= 0) {
                auto it = timeFrames.find(currentTimeframe);
                if (it != timeFrames.end()) {
                    if (it->second.Started() && it->second.Finished() == false) {
                        it->second.Stop();   
                        CalculateTotalTime();
                        return true;
                    }
                }
            }
            return false;
        }
        return false;
    }
    unsigned int Count() {
        return currentTimeframe + 1;
    }
};
class MothershipData {
public:
    typedef std::map<std::string, MothershipTask> _Tasks;

    _Tasks tasks;

    bool AddTask(const std::string& taskTitle, MothershipColor fgColor, bool start) {
        auto ip = tasks.insert(
            std::make_pair(
                taskTitle,
                MothershipTask(taskTitle, fgColor)
            )
        );
        if (ip.second) {
            if (start) {
                ip.first->second.StartTimeframe();
            }
            return true;
        }
        return false;
    }
    bool EraseTask(const std::string& taskTitle) {
        auto it = tasks.find(taskTitle);
        if (it != tasks.end()) {
            tasks.erase(it);
            return true;
        }
        return false;
    }
    bool ResumeTask(const std::string& taskTitle) {
        auto it = tasks.find(taskTitle);
        if (it != tasks.end()) {
            if (it->second.Stopped()) {
                it->second.StartTimeframe();
                return true;
            }
            return false;
        }
        return false;
    }
    bool StopTask(const std::string& taskTitle) {
        auto it = tasks.find(taskTitle);
        if (it != tasks.end()) {
            if (it->second.Started()) {
                it->second.FinishTimeframe();
                return true;
            }
            return false;
        }
        return false;
    }
};

MothershipData mothershipData;

void InitializeWindows(int maxY, int maxX) {
    int bottomHeight = 6;
    int topHeight = maxY - bottomHeight;
    if (topHeight < 3) topHeight = 3;
    if (bottomHeight < 3) bottomHeight = 3;

    int halfX = maxX / 2;

    // Sanity fallback: full screen single window if too small
    if (topHeight + bottomHeight > maxY || halfX < 10) {
        mvprintw(0, 0, "Terminal too small or bad split.");
        refresh();
        getch();
        endwin();
        exit(1);
    }

    leftWin = newwin(topHeight, halfX, 0, 0);
    rightWin = newwin(topHeight, maxX - halfX, 0, halfX);
    bottomWin = newwin(bottomHeight, maxX, topHeight, 0);

    box(leftWin, 0, 0);
    box(rightWin, 0, 0);
    box(bottomWin, 0, 0);
}

void OutputToWindows() {
    init_pair(1, COLOR_WHITE, COLOR_BLACK);
    init_pair(2, COLOR_RED, COLOR_BLACK);
    init_pair(3, COLOR_GREEN, COLOR_BLACK);
    init_pair(4, COLOR_CYAN, COLOR_BLACK);

    // Paint entire windows with background color
    wbkgd(leftWin, COLOR_PAIR(1));
    wbkgd(rightWin, COLOR_PAIR(1));
    wbkgd(bottomWin, COLOR_PAIR(1));

    // Optional: clear to propagate the color across window area
    wclear(leftWin);
    wclear(rightWin);
    wclear(bottomWin);

    // Redraw borders
    box(leftWin, 0, 0);
    box(rightWin, 0, 0);
    box(bottomWin, 0, 0);

    // Draw text with foreground color on colored background
    wattron(leftWin, A_BOLD);
    mvwprintw(leftWin, 1, 2, "Tasks Regime");
    wattroff(leftWin, A_BOLD);

    wattron(rightWin, A_BOLD);
    mvwprintw(rightWin, 1, 2, "Current Progress");
    wattroff(rightWin, A_BOLD);

    wattron(bottomWin, A_BOLD);
    mvwprintw(bottomWin, 1, 2, "Command");
    wattroff(bottomWin, A_BOLD);

    // Refresh windows
    wrefresh(leftWin);
    wrefresh(rightWin);
    wrefresh(bottomWin);
}

int main() {
    initscr();
    start_color();
    cbreak();
    noecho();
    curs_set(0);
    keypad(stdscr, TRUE);

    int maxY, maxX;
    getmaxyx(stdscr, maxY, maxX);

    // Force refresh base screen to activate drawing
    refresh();

    // Also print visible debug message
    mvprintw(0, 0, "NCURSES ACTIVE - INIT OK");
    refresh();

    InitializeWindows(maxY, maxX);
    OutputToWindows();

    getch();
    endwin();
    return 0;
}
