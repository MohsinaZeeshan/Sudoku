#include "raylib.h"
#include <string>
#include <vector>
#include <algorithm>
#include <random>
#include <sstream>
#include <fstream>
#include <iomanip>
#include <ctime>
#include <cstdlib>
#include <stdexcept>   
#include <cerrno>     
#include <cstring>     

using namespace std;

class SudokuException : public std::exception {
protected:
    string message;
public:
    explicit SudokuException(const string& msg) : message(msg) {}
    virtual const char* what() const noexcept override {
        return message.c_str();
    }
};
class FileIOException : public SudokuException {
    string filename;
    string operation; 
public:
    FileIOException(const string& file, const string& op, const string& detail = "")
        : SudokuException("File " + op + " error on '" + file + "'"
                          + (detail.empty() ? "" : ": " + detail)),
          filename(file), operation(op) {}

    const string& getFilename()  const { return filename; }
    const string& getOperation() const { return operation; }
};

class CorruptSaveException : public SudokuException {
    int lineNumber;
public:
    CorruptSaveException(const string& detail, int line = -1)
        : SudokuException("Corrupt save file" +
                          (line >= 0 ? " at line " + to_string(line) : "") +
                          ": " + detail),
          lineNumber(line) {}
    int getLineNumber() const { return lineNumber; }
};

class InvalidCellValueException : public SudokuException {
    int row, col, value;
public:
    InvalidCellValueException(int r, int c, int v)
        : SudokuException("Invalid value " + to_string(v) +
                          " at cell (" + to_string(r) + "," + to_string(c) + ")"),
          row(r), col(c), value(v) {}
    int getRow()   const { return row;   }
    int getCol()   const { return col;   }
    int getValue() const { return value; }
};

class InvalidScoreException : public SudokuException {
public:
    explicit InvalidScoreException(const string& detail)
        : SudokuException("Invalid score entry: " + detail) {}
};

class GameLogger {
    string   logPath;
    ofstream logFile;
    bool     enabled;

    string timestamp() const {
        time_t now = time(nullptr);
        struct tm* t = localtime(&now);
        char buf[32];
        strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", t);
        return string(buf);
    }

public:
    explicit GameLogger(const string& path = "game_log.txt")
        : logPath(path), enabled(false)
    {
        try {
            logFile.open(path, ios::app);
            if (!logFile.is_open())
                throw FileIOException(path, "open",
                      "Logger disabled – " + string(strerror(errno)));
            enabled = true;
            log("=== Session started ===");
        }
        catch (const FileIOException& e) {
           
            enabled = false;
        }
    }

    ~GameLogger() {
        if (enabled) {
            log("=== Session ended ===");
            logFile.close();
        }
    }

    void log(const string& event) {
        if (!enabled) return;
        try {
            if (!logFile.good())
                throw FileIOException(logPath, "write", "stream in bad state");
            logFile << "[" << timestamp() << "] " << event << "\n";
            logFile.flush();
        }
        catch (const FileIOException& e) {
       
            enabled = false;
        }
    }

    bool isEnabled() const { return enabled; }
};

static GameLogger gLogger;

static const int WIN_W   = 900;
static const int WIN_H   = 700;
static const int CELL    = 56;
static const int GRID_X  = 50;
static const int GRID_Y  = 90;
static const int PANEL_X = 580;

static const Color C_BG        = { 15,  15,  35, 255 };
static const Color C_GRID_BG   = { 25,  25,  60, 255 };
static const Color C_CELL_DEF  = { 35,  35,  80, 255 };
static const Color C_CELL_FIX  = { 20,  20,  55, 255 };
static const Color C_CELL_SEL  = { 60,  80, 160, 255 };
static const Color C_CELL_SAME = { 40,  55, 110, 255 };
static const Color C_CELL_ERR  = {140,  20,  30, 255 };
static const Color C_CELL_HINT = { 20, 120,  80, 255 };
static const Color C_LINE      = { 80,  80, 140, 255 };
static const Color C_LINE_BOX  = {180, 180, 255, 255 };
static const Color C_NUM_FIX   = {220, 220, 255, 255 };
static const Color C_NUM_USR   = {100, 200, 255, 255 };
static const Color C_NUM_ERR   = {255, 100, 100, 255 };
static const Color C_BTN       = { 50,  80, 180, 255 };
static const Color C_BTN_HOV   = { 70, 110, 220, 255 };
static const Color C_BTN_EASY  = { 30, 140,  80, 255 };
static const Color C_BTN_MED   = {180, 130,  20, 255 };
static const Color C_BTN_HARD  = {160,  30,  30, 255 };
static const Color C_ACCENT    = { 80, 180, 255, 255 };
static const Color C_WIN       = { 30, 200, 100, 255 };
static const Color C_LOSE      = {220,  60,  60, 255 };

class GameTimer {
    double startTime;
    double pausedElapsed;
    bool   running;
public:
    GameTimer() : startTime(0), pausedElapsed(0), running(false) {}
    void start()  { startTime = GetTime(); running = true; }
    void stop()   { if (running) { pausedElapsed += GetTime() - startTime; running = false; } }
    void reset()  { startTime = 0; pausedElapsed = 0; running = false; }
    void setElapsed(double e) { pausedElapsed = e; running = false; } 

    int  elapsed() const {
        double e = pausedElapsed;
        if (running) e += GetTime() - startTime;
        return (int)e;
    }
    string str() const {
        int s = elapsed(), m = s / 60; s %= 60;
        ostringstream oss;
        oss << setw(2) << setfill('0') << m << ":"
            << setw(2) << setfill('0') << s;
        return oss.str();
    }
};

struct ScoreEntry {
    string name, difficulty;
    int    seconds, mistakes;
};

class SudokuBoard {
protected:
    int  board[9][9];
    int  solution[9][9];
    bool fixed[9][9];
    bool hinted[9][9];

public:
    SudokuBoard() {
        for (int r = 0; r < 9; r++)
            for (int c = 0; c < 9; c++) {
                board[r][c] = solution[r][c] = 0;
                fixed[r][c] = hinted[r][c] = false;
            }
    }

    int  getCell(int r, int c)     const { return board[r][c]; }
    int  getSolution(int r, int c) const { return solution[r][c]; }
    bool isFixed(int r, int c)     const { return fixed[r][c]; }
    bool isHinted(int r, int c)    const { return hinted[r][c]; }

    bool setCell(int r, int c, int v) {
        if (v < 0 || v > 9)
            throw InvalidCellValueException(r, c, v);
        if (fixed[r][c] || hinted[r][c]) return false;
        board[r][c] = v;
        return true;
    }

    bool isValidPlacement(int r, int c, int v, int g[9][9]) const {
        for (int j = 0; j < 9; j++) if (j != c && g[r][j] == v) return false;
        for (int i = 0; i < 9; i++) if (i != r && g[i][c] == v) return false;
        int br = (r/3)*3, bc = (c/3)*3;
        for (int i = br; i < br+3; i++)
            for (int j = bc; j < bc+3; j++)
                if ((i!=r||j!=c) && g[i][j] == v) return false;
        return true;
    }

    bool hasConflict(int r, int c) const {
        int v = board[r][c];
        if (v == 0) return false;
        return !isValidPlacement(r, c, v, const_cast<int(*)[9]>(board));
    }

    bool isBoardComplete() const {
        for (int r = 0; r < 9; r++)
            for (int c = 0; c < 9; c++)
                if (board[r][c] == 0 || hasConflict(r,c)) return false;
        return true;
    }

    virtual void render(int selR, int selC) const { (void)selR; (void)selC; }
};

class SudokuSolver : public SudokuBoard {
public:
    bool solve(int g[9][9]) {
        for (int r = 0; r < 9; r++) {
            for (int c = 0; c < 9; c++) {
                if (g[r][c] == 0) {
                    int nums[9] = {1,2,3,4,5,6,7,8,9};
                    for (int k = 8; k > 0; k--) {
                        int j = rand() % (k+1); swap(nums[k], nums[j]);
                    }
                    for (int i = 0; i < 9; i++) {
                        if (isValidPlacement(r, c, nums[i], g)) {
                            g[r][c] = nums[i];
                            if (solve(g)) return true;
                            g[r][c] = 0;
                        }
                    }
                    return false;
                }
            }
        }
        return true;
    }

    int countSolutions(int g[9][9], int cap=2) {
        for (int r = 0; r < 9; r++) {
            for (int c = 0; c < 9; c++) {
                if (g[r][c] == 0) {
                    int cnt = 0;
                    for (int v = 1; v <= 9 && cnt < cap; v++) {
                        if (isValidPlacement(r,c,v,g)) {
                            g[r][c] = v;
                            cnt += countSolutions(g, cap-cnt);
                            g[r][c] = 0;
                        }
                    }
                    return cnt;
                }
            }
        }
        return 1;
    }

    void generateSolution() {
        for (int r = 0; r < 9; r++)
            for (int c = 0; c < 9; c++) solution[r][c] = 0;
        solve(solution);
    }

    void generatePuzzle(int diff) {
        generateSolution();
        for (int r = 0; r < 9; r++)
            for (int c = 0; c < 9; c++)
                board[r][c] = solution[r][c], fixed[r][c] = true, hinted[r][c] = false;

        int toRemove = (diff==1)?35:(diff==2)?45:55;
        vector<pair<int,int>> pos;
        for (int r = 0; r < 9; r++)
            for (int c = 0; c < 9; c++) pos.push_back({r,c});
        static mt19937 rng((unsigned)time(nullptr));
        shuffle(pos.begin(), pos.end(), rng);

        int removed = 0;
        for (auto& p : pos) {
            if (removed >= toRemove) break;
            int r = p.first, c = p.second;
            int bk = board[r][c];
            board[r][c] = 0; fixed[r][c] = false;
            int tmp[9][9];
            for (int i = 0; i < 9; i++)
                for (int j = 0; j < 9; j++) tmp[i][j] = board[i][j];
            if (countSolutions(tmp) != 1) {
                board[r][c] = bk; fixed[r][c] = true;
            } else removed++;
        }
    }

    void autoSolve() {
        for (int r = 0; r < 9; r++)
            for (int c = 0; c < 9; c++) board[r][c] = solution[r][c];
    }
};

struct Button {
    Rectangle rect;
    string    label;
    Color     color;
    Color     hover;

    bool isHovered() const {
        return CheckCollisionPointRec(GetMousePosition(), rect);
    }
    bool isClicked() const {
        return isHovered() && IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
    }
    void draw(Font font, int fs = 18) const {
        Color c = isHovered() ? hover : color;
        DrawRectangleRounded(rect, 0.3f, 6, c);
        DrawRectangleRoundedLines(rect, 0.3f, 6, Fade(WHITE, 0.2f));
        Vector2 ts = MeasureTextEx(font, label.c_str(), fs, 1);
        DrawTextEx(font, label.c_str(),
                   { rect.x + (rect.width  - ts.x) * 0.5f,
                     rect.y + (rect.height - ts.y) * 0.5f },
                   fs, 1, WHITE);
    }
};

class SudokuGame : public SudokuSolver {
    enum Screen { MENU, PLAYING, WIN, GAMEOVER, SCORES };
    Screen screen;

    int       selRow, selCol;
    int       difficulty;
    int       mistakes;
    int       hintsLeft;
    GameTimer timer;
    string    playerName;
    bool      solved;

    vector<ScoreEntry> highScores;

    string  statusMsg;
    float   statusTimer;  
    bool    statusIsError;

    float winAlpha;
    float flashTimer;

    Font fontMain;
    bool fontLoaded;

    Button btnEasy, btnMed, btnHard;
    Button btnHint, btnSolve, btnClear, btnScores, btnMenu;
    Button btnSave, btnLoad;                    
    Button btnPlayAgain, btnToMenu;

    void showStatus(const string& msg, bool isError = false) {
        statusMsg     = msg;
        statusTimer   = 3.0f;   
        statusIsError = isError;
    }

    void loadScores() {
        highScores.clear();
        try {
            ifstream f("scores.txt");
            if (!f.is_open())
                throw FileIOException("scores.txt", "open",
                                      string(strerror(errno)));

            string line;
            int lineNum = 0;
            while (getline(f, line)) {
                lineNum++;
                if (line.empty()) continue;
                try {
                    istringstream ss(line);
                    ScoreEntry e;
                    string t, m;
                    if (!getline(ss, e.name,       ',') ||
                        !getline(ss, e.difficulty, ',') ||
                        !getline(ss, t,            ',') ||
                        !getline(ss, m,            ','))
                        throw CorruptSaveException("missing fields", lineNum);

                    for (char ch : t)
                        if (!isdigit(ch))
                            throw InvalidScoreException("non-numeric time '" + t + "'");
                    for (char ch : m)
                        if (!isdigit(ch))
                            throw InvalidScoreException("non-numeric mistakes '" + m + "'");

                    e.seconds  = stoi(t);
                    e.mistakes = stoi(m);

                    if (e.seconds  < 0 || e.seconds  > 99999)
                        throw InvalidScoreException("time out of range: " + t);
                    if (e.mistakes < 0 || e.mistakes > 10)
                        throw InvalidScoreException("mistakes out of range: " + m);

                    highScores.push_back(e);
                }
                catch (const InvalidScoreException& e) {
                    gLogger.log(string("Score load warning: ") + e.what());
                    
                }
                catch (const CorruptSaveException& e) {
                    gLogger.log(string("Score load warning: ") + e.what());
                }
            }
            f.close();

            sort(highScores.begin(), highScores.end(),
                 [](const ScoreEntry& a, const ScoreEntry& b){
                     return a.seconds < b.seconds;
                 });

            gLogger.log("Scores loaded (" + to_string(highScores.size()) + " entries)");
        }
        catch (const FileIOException& e) {
            
            gLogger.log(string("loadScores: ") + e.what());
        }
        catch (const exception& e) {
            gLogger.log(string("loadScores unexpected error: ") + e.what());
        }
    }

    void saveScore() {
        try {
            ofstream f("scores.txt", ios::app);
            if (!f.is_open())
                throw FileIOException("scores.txt", "write",
                                      string(strerror(errno)));

            string d = (difficulty==1)?"Easy":(difficulty==2)?"Medium":"Hard";
            f << playerName << "," << d << "," << timer.elapsed()
              << "," << mistakes << "\n";

            if (!f.good())
                throw FileIOException("scores.txt", "write", "stream error after write");

            f.close();
            gLogger.log("Score saved for " + playerName + " [" + d + "]");
        }
        catch (const FileIOException& e) {
            gLogger.log(string("saveScore error: ") + e.what());
            showStatus("Warning: score could not be saved!", true);
        }
        catch (const exception& e) {
            gLogger.log(string("saveScore unexpected error: ") + e.what());
        }
    }

    void saveGame() {
        try {
            ofstream f("game_save.txt");
            if (!f.is_open())
                throw FileIOException("game_save.txt", "open",
                                      string(strerror(errno)));

            f << playerName  << "\n"
              << difficulty  << "\n"
              << timer.elapsed() << "\n"
              << mistakes    << "\n"
              << hintsLeft   << "\n";

            auto writeGrid = [&](auto grid, bool isBool = false) {
                for (int r = 0; r < 9; r++) {
                    for (int c = 0; c < 9; c++) {
                        if (c) f << " ";
                        f << (isBool ? (int)grid[r][c] : grid[r][c]);
                    }
                    f << "\n";
                }
            };

            for (int r = 0; r < 9; r++) {
                for (int c = 0; c < 9; c++) {
                    if (c) f << " ";
                    f << board[r][c];
                }
                f << "\n";
            }
            for (int r = 0; r < 9; r++) {
                for (int c = 0; c < 9; c++) {
                    if (c) f << " ";
                    f << solution[r][c];
                }
                f << "\n";
            }
            for (int r = 0; r < 9; r++) {
                for (int c = 0; c < 9; c++) {
                    if (c) f << " ";
                    f << (int)fixed[r][c];
                }
                f << "\n";
            }
            for (int r = 0; r < 9; r++) {
                for (int c = 0; c < 9; c++) {
                    if (c) f << " ";
                    f << (int)hinted[r][c];
                }
                f << "\n";
            }

            if (!f.good())
                throw FileIOException("game_save.txt", "write", "stream error");

            f.close();
            gLogger.log("Game saved for player: " + playerName);
            showStatus("Game saved successfully!");
        }
        catch (const FileIOException& e) {
            gLogger.log(string("saveGame error: ") + e.what());
            showStatus("Error: could not save game! " + string(e.what()), true);
        }
        catch (const exception& e) {
            gLogger.log(string("saveGame unexpected error: ") + e.what());
            showStatus("Unexpected save error!", true);
        }
    }

    void loadGame() {
        try {
            ifstream f("game_save.txt");
            if (!f.is_open())
                throw FileIOException("game_save.txt", "open",
                                      "No saved game found.");

            int lineNum = 0;
            auto nextLine = [&]() -> string {
                string line;
                if (!getline(f, line))
                    throw CorruptSaveException("unexpected end of file", lineNum);
                lineNum++;
                return line;
            };

            auto readInt = [&](const string& fieldName) -> int {
                string s = nextLine();
                for (char ch : s)
                    if (!isdigit(ch) && ch != '-')
                        throw CorruptSaveException(
                              "non-integer value for " + fieldName +
                              " = '" + s + "'", lineNum);
                return stoi(s);
            };

            string name = nextLine();
            if (name.empty())
                throw CorruptSaveException("player name is empty", lineNum);

            int diff = readInt("difficulty");
            if (diff < 1 || diff > 3)
                throw CorruptSaveException("difficulty out of range: " + to_string(diff));

            int elapsed  = readInt("elapsed");
            if (elapsed < 0)
                throw CorruptSaveException("negative elapsed time");

            int errs = readInt("mistakes");
            if (errs < 0 || errs > 10)
                throw CorruptSaveException("mistakes out of range: " + to_string(errs));

            int hints = readInt("hintsLeft");
            if (hints < 0)
                throw CorruptSaveException("negative hintsLeft");

            auto readGrid = [&](int grid[9][9], const string& name_) {
                for (int r = 0; r < 9; r++) {
                    string line = nextLine();
                    istringstream ss(line);
                    for (int c = 0; c < 9; c++) {
                        int v;
                        if (!(ss >> v))
                            throw CorruptSaveException(
                                  name_ + " row " + to_string(r) +
                                  ": missing value at col " + to_string(c), lineNum);
                        if (v < 0 || v > 9)
                            throw CorruptSaveException(
                                  name_ + "[" + to_string(r) + "][" + to_string(c) +
                                  "] = " + to_string(v) + " out of range", lineNum);
                        grid[r][c] = v;
                    }
                }
            };

            int tmpBoard[9][9], tmpSol[9][9], tmpFixed[9][9], tmpHinted[9][9];
            readGrid(tmpBoard,  "board");
            readGrid(tmpSol,    "solution");
            readGrid(tmpFixed,  "fixed");
            readGrid(tmpHinted, "hinted");

            f.close();

            playerName = name;
            difficulty = diff;
            mistakes   = errs;
            hintsLeft  = hints;
            selRow = selCol = -1;
            solved = false;

            for (int r = 0; r < 9; r++)
                for (int c = 0; c < 9; c++) {
                    board[r][c]    = tmpBoard[r][c];
                    solution[r][c] = tmpSol[r][c];
                    fixed[r][c]    = (bool)tmpFixed[r][c];
                    hinted[r][c]   = (bool)tmpHinted[r][c];
                }

            timer.reset();
            timer.setElapsed(elapsed);
            timer.start();

            screen = PLAYING;
            gLogger.log("Game loaded for player: " + playerName);
            showStatus("Game loaded successfully!");
        }
        catch (const FileIOException& e) {
            gLogger.log(string("loadGame: ") + e.what());
            showStatus("No saved game found!", true);
        }
        catch (const CorruptSaveException& e) {
            gLogger.log(string("loadGame: ") + e.what());
            showStatus("Save file is corrupt – cannot load!", true);
        }
        catch (const exception& e) {
            gLogger.log(string("loadGame unexpected error: ") + e.what());
            showStatus("Unexpected load error!", true);
        }
    }

    void drawText(const string& s, int x, int y, int fs, Color c) const {
        if (fontLoaded)
            DrawTextEx(fontMain, s.c_str(), {(float)x,(float)y}, fs, 1, c);
        else
            DrawText(s.c_str(), x, y, fs, c);
    }

    Vector2 measureText(const string& s, int fs) const {
        if (fontLoaded) return MeasureTextEx(fontMain, s.c_str(), fs, 1);
        return { (float)MeasureText(s.c_str(), fs), (float)fs };
    }

    void centreText(const string& s, int y, int fs, Color c) const {
        Vector2 sz = measureText(s, fs);
        drawText(s, (int)((WIN_W - sz.x) * 0.5f), y, fs, c);
    }

    void render(int sR, int sC) const override {
        DrawRectangle(GRID_X-4, GRID_Y-4, CELL*9+8, CELL*9+8, C_GRID_BG);

        for (int r = 0; r < 9; r++) {
            for (int c = 0; c < 9; c++) {
                int x = GRID_X + c * CELL;
                int y = GRID_Y + r * CELL;
                Rectangle cell = { (float)x, (float)y, (float)CELL, (float)CELL };

                Color bg = C_CELL_DEF;
                if (fixed[r][c])                        bg = C_CELL_FIX;
                if (hinted[r][c])                       bg = C_CELL_HINT;
                if (hasConflict(r,c))                   bg = C_CELL_ERR;
                if (r == sR && c == sC)                 bg = C_CELL_SEL;
                if (sR >= 0 && sC >= 0 &&
                    board[sR][sC] != 0 &&
                    board[r][c] == board[sR][sC] &&
                    !(r==sR && c==sC))                  bg = C_CELL_SAME;

                DrawRectangleRec(cell, bg);

                if (board[r][c] != 0) {
                    string ns = to_string(board[r][c]);
                    Color nc = fixed[r][c]  ? C_NUM_FIX :
                               hinted[r][c] ? ColorBrightness(WHITE, 0.1f) :
                               hasConflict(r,c) ? C_NUM_ERR : C_NUM_USR;
                    Vector2 ts = measureText(ns, 28);
                    drawText(ns, (int)(x + (CELL - ts.x) * 0.5f),
                                 (int)(y + (CELL - ts.y) * 0.5f), 28, nc);
                }
            }
        }

        for (int i = 0; i <= 9; i++) {
            float px = GRID_X + i * CELL;
            float py = GRID_Y + i * CELL;
            bool box = (i % 3 == 0);
            float thick = box ? 3.0f : 1.0f;
            Color lc    = box ? C_LINE_BOX : C_LINE;
            DrawLineEx({px, (float)GRID_Y}, {px, (float)(GRID_Y + CELL*9)}, thick, lc);
            DrawLineEx({(float)GRID_X, py}, {(float)(GRID_X + CELL*9), py}, thick, lc);
        }

        if (flashTimer > 0) {
            float alpha = flashTimer / 0.4f;
            DrawRectangle(GRID_X, GRID_Y, CELL*9, CELL*9,
                          Fade({200,0,0,200}, alpha));
        }
    }

    void drawStatusBar() {
        if (statusTimer <= 0) return;
        Color bg  = statusIsError ? Fade({200,50,50,255},  0.9f)
                                  : Fade({30,140,80,255},  0.9f);
        Color txt = WHITE;
        DrawRectangle(0, WIN_H - 40, WIN_W, 40, bg);
        centreText(statusMsg, WIN_H - 30, 18, txt);
    }

    void drawPanel() {
        drawText("SUDOKU", PANEL_X, 20, 40, C_ACCENT);

        drawText("Player: " + playerName, PANEL_X, 75, 18, WHITE);

        string diff = (difficulty==1)?"Easy":(difficulty==2)?"Medium":"Hard";
        drawText("Difficulty: " + diff, PANEL_X, 100, 18,
                 difficulty==1? C_BTN_EASY : difficulty==2? C_BTN_MED : C_BTN_HARD);
        drawText("Time:  " + timer.str(),          PANEL_X, 128, 20, WHITE);
        drawText("Mistakes: " + to_string(mistakes) + " / 10", PANEL_X, 153, 18,
                 mistakes >= 7 ? RED : WHITE);
        drawText("Hints left: " + to_string(hintsLeft), PANEL_X, 178, 18,
                 hintsLeft == 0 ? GRAY : C_WIN);

        Font f = fontLoaded ? fontMain : GetFontDefault();
        btnHint  .draw(f);
        btnSolve .draw(f);
        btnClear .draw(f);
        btnSave  .draw(f);   
        btnLoad  .draw(f);   
        btnScores.draw(f);
        btnMenu  .draw(f);

        drawText("Click cell, then press 1-9", PANEL_X, 620, 13, GRAY);
        drawText("Press DEL/0 to clear cell",  PANEL_X, 636, 13, GRAY);

        for (int r = 0; r < 9; r++) {
            string s(1, (char)('A'+r));
            drawText(s, GRID_X - 22, GRID_Y + r*CELL + (CELL-18)/2, 18, GRAY);
        }
        for (int c = 0; c < 9; c++) {
            string s = to_string(c+1);
            drawText(s, GRID_X + c*CELL + (CELL-12)/2, GRID_Y - 22, 16, GRAY);
        }
    }

    void drawMenu() {
        for (int i = 0; i < 20; i++) {
            DrawRectangle(0, i*35, WIN_W, 35,
                          ColorAlpha({20,(unsigned char)(15+i*3),(unsigned char)(40+i*5),255}, 0.5f));
        }

        centreText("SUDOKU", 80, 60, C_ACCENT);

        centreText("Enter Your Name:", 220, 22, WHITE);

        Rectangle nameBox = { (float)(WIN_W/2 - 160), 255, 320, 42 };
        DrawRectangleRoundedLines(nameBox, 0.2f, 6, Fade(C_ACCENT, 0.9f));
        drawText(playerName + (((int)(GetTime()*2)) % 2 == 0 ? "|" : " "),
                 (int)nameBox.x + 12, (int)nameBox.y + 8, 24, WHITE);

        centreText("Select Difficulty:", 330, 20, LIGHTGRAY);

        Font f = fontLoaded ? fontMain : GetFontDefault();
        btnEasy.draw(f, 20);
        btnMed .draw(f, 20);
        btnHard.draw(f, 20);

        Button bLoad;
        bLoad.rect  = {(float)(WIN_W/2-160), 425, 150, 44};
        bLoad.label = "Load Saved Game";
        bLoad.color = {60,120,60,255};
        bLoad.hover = {80,160,80,255};
        if (bLoad.isClicked()) loadGame();
        bLoad.draw(f, 17);

        Button bSc;
        bSc.rect  = {(float)(WIN_W/2+20), 425, 140, 44};
        bSc.label = "High Scores";
        bSc.color = C_BTN;
        bSc.hover = C_BTN_HOV;
        if (bSc.isClicked()) { loadScores(); screen = SCORES; }
        bSc.draw(f, 17);
    }

    void drawScores() {
        centreText("HIGH SCORES", 30, 36, C_ACCENT);

        int y = 90;
        drawText("Name",       100, y, 18, YELLOW);
        drawText("Difficulty", 300, y, 18, YELLOW);
        drawText("Time",       460, y, 18, YELLOW);
        drawText("Mistakes",   580, y, 18, YELLOW);
        DrawLine(80, y+24, WIN_W-80, y+24, GRAY);

        y += 35;
        int cnt = 0;
        for (auto& e : highScores) {
            if (cnt++ >= 10) break;
            Color c = (e.difficulty=="Easy")   ? C_BTN_EASY :
                      (e.difficulty=="Medium") ? C_BTN_MED   : C_BTN_HARD;
            drawText(e.name,        100, y, 18, WHITE);
            drawText(e.difficulty,  300, y, 18, c);
            int m = e.seconds/60, s = e.seconds%60;
            ostringstream ts;
            ts << setw(2)<<setfill('0')<<m<<":"<<setw(2)<<setfill('0')<<s;
            drawText(ts.str(),      460, y, 18, WHITE);
            drawText(to_string(e.mistakes), 580, y, 18, WHITE);
            y += 30;
        }
        if (highScores.empty())
            centreText("No scores yet!", 300, 22, GRAY);

        btnToMenu.draw(fontLoaded ? fontMain : GetFontDefault(), 20);
    }

    void drawOverlay() {
        bool win = (screen == WIN);
        Color oc = win ? Fade(C_WIN, 0.15f) : Fade(C_LOSE, 0.15f);
        DrawRectangle(0, 0, WIN_W, WIN_H, oc);

        string title    = win ? "PUZZLE SOLVED!" : "GAME OVER";
        string subtitle = win ? ("Time: " + timer.str() + "   Mistakes: " + to_string(mistakes))
                              : "10 mistakes reached!";
        Color  tc       = win ? C_WIN : C_LOSE;
        centreText(title,    WIN_H/2 - 80, 48, tc);
        centreText(subtitle, WIN_H/2 - 20, 22, WHITE);

        Font f = fontLoaded ? fontMain : GetFontDefault();
        btnPlayAgain.draw(f, 20);
        btnToMenu   .draw(f, 20);
    }

    void handlePlayInput() {
        
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            Vector2 mp = GetMousePosition();
            int cr = (int)((mp.y - GRID_Y) / CELL);
            int cc = (int)((mp.x - GRID_X) / CELL);
            if (cr >= 0 && cr < 9 && cc >= 0 && cc < 9) {
                selRow = cr; selCol = cc;
            }
        }

        if (selRow >= 0 && selCol >= 0 && !isFixed(selRow, selCol)) {
            int val = -1;
            for (int k = KEY_ONE; k <= KEY_NINE; k++)
                if (IsKeyPressed(k)) { val = k - KEY_ONE + 1; break; }
            for (int k = KEY_KP_1; k <= KEY_KP_9; k++)
                if (IsKeyPressed(k)) { val = k - KEY_KP_1 + 1; break; }
            if (IsKeyPressed(KEY_ZERO)      || IsKeyPressed(KEY_DELETE) ||
                IsKeyPressed(KEY_BACKSPACE) || IsKeyPressed(KEY_KP_0))
                val = 0;

            if (val >= 0) {
                try {
                    if (val == 0) {
                        setCell(selRow, selCol, 0);
                    } else {
                        if (val != solution[selRow][selCol]) {
                            mistakes++;
                            flashTimer = 0.4f;
                            gLogger.log("Wrong move at (" + to_string(selRow) +
                                        "," + to_string(selCol) +
                                        ") val=" + to_string(val) +
                                        " mistakes=" + to_string(mistakes));
                            if (mistakes >= 10) {
                                timer.stop();
                                screen = GAMEOVER;
                                gLogger.log("Game over for " + playerName);
                                return;
                            }
                        }
                        setCell(selRow, selCol, val);
                        if (isBoardComplete()) {
                            timer.stop();
                            solved = true;
                            saveScore();
                            loadScores();
                            screen   = WIN;
                            winAlpha = 0;
                            gLogger.log("Puzzle solved by " + playerName +
                                        " in " + timer.str() +
                                        " with " + to_string(mistakes) + " mistakes");
                            return;
                        }
                    }
                }
                catch (const InvalidCellValueException& e) {
                    
                    gLogger.log(string("Cell value exception: ") + e.what());
                }
            }
        }

        if (IsKeyPressed(KEY_UP)    && selRow > 0) selRow--;
        if (IsKeyPressed(KEY_DOWN)  && selRow < 8) selRow++;
        if (IsKeyPressed(KEY_LEFT)  && selCol > 0) selCol--;
        if (IsKeyPressed(KEY_RIGHT) && selCol < 8) selCol++;

        if (IsKeyPressed(KEY_S) && (IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL)))
            saveGame();

        if (btnHint  .isClicked()) doHint();
        if (btnSolve .isClicked()) doSolve();
        if (btnClear .isClicked()) doClear();
        if (btnSave  .isClicked()) saveGame();   
        if (btnLoad  .isClicked()) loadGame();   
        if (btnScores.isClicked()) { loadScores(); screen = SCORES; }
        if (btnMenu  .isClicked()) { timer.stop(); screen = MENU; }
    }

    void doHint() {
        if (hintsLeft <= 0) { showStatus("No hints left!", true); return; }
        vector<pair<int,int>> empty;
        for (int r=0; r<9; r++)
            for (int c=0; c<9; c++)
                if (!fixed[r][c] && !hinted[r][c] && board[r][c]==0)
                    empty.push_back({r,c});
        if (empty.empty()) { showStatus("No empty cells to hint!"); return; }
        int idx = rand() % empty.size();
        int r = empty[idx].first, c = empty[idx].second;
        board[r][c]  = solution[r][c];
        hinted[r][c] = true;
        hintsLeft--;
        gLogger.log("Hint used at (" + to_string(r) + "," + to_string(c) + ")");
        if (isBoardComplete()) {
            timer.stop(); solved = true;
            saveScore(); loadScores();
            screen = WIN;
        }
    }
    void doSolve() {
        autoSolve();
        timer.stop();
        screen   = WIN;
        solved   = false;
        winAlpha = 0;
        gLogger.log("Auto-solve used by " + playerName);
    }
    void doClear() {
        for (int r=0; r<9; r++)
            for (int c=0; c<9; c++)
                if (!fixed[r][c] && !hinted[r][c]) board[r][c] = 0;
        mistakes = 0;
        gLogger.log("Board cleared by " + playerName);
        showStatus("Board cleared.");
    }
    void startGame(int diff) {
        difficulty = diff;
        hintsLeft  = (diff==1)?5:(diff==2)?3:1;
        mistakes   = 0;
        selRow = selCol = -1;
        solved = false;
        timer.reset();
        generatePuzzle(diff);
        timer.start();
        screen = PLAYING;
        string d = (diff==1)?"Easy":(diff==2)?"Medium":"Hard";
        gLogger.log("New game started: " + playerName + " [" + d + "]");
    }

    void buildButtons() {
       
        btnEasy = {{ (float)(WIN_W/2-160), 365, 90, 44 }, "Easy",   C_BTN_EASY, ColorBrightness(C_BTN_EASY, 0.3f)};
        btnMed  = {{ (float)(WIN_W/2- 45), 365, 90, 44 }, "Medium", C_BTN_MED,  ColorBrightness(C_BTN_MED,  0.3f)};
        btnHard = {{ (float)(WIN_W/2+ 70), 365, 90, 44 }, "Hard",   C_BTN_HARD, ColorBrightness(C_BTN_HARD, 0.3f)};

        float bx = PANEL_X, bw = 270, bh = 38;
        btnHint   = {{ bx, 220, bw, bh }, "Hint (H)",       {50,140,100,255},{70,180,120,255}};
        btnSolve  = {{ bx, 265, bw, bh }, "Auto Solve",     {130,60,200,255},{160,90,230,255}};
        btnClear  = {{ bx, 310, bw, bh }, "Clear Board",    {180,100,20,255},{210,130,40,255}};
        btnSave   = {{ bx, 360, bw, bh }, "Save Game (Ctrl+S)", {40,100,160,255},{60,130,200,255}};  
        btnLoad   = {{ bx, 405, bw, bh }, "Load Game",      {40,80,140,255}, {60,110,180,255}};     
        btnScores = {{ bx, 455, bw, bh }, "High Scores",    C_BTN,           C_BTN_HOV};
        btnMenu   = {{ bx, 500, bw, bh }, "Main Menu",      {80,80,80,255},  {110,110,110,255}};

        
        btnPlayAgain = {{ (float)(WIN_W/2-140), (float)(WIN_H/2+40), 130, 44 }, "Play Again", C_BTN,         C_BTN_HOV};
        btnToMenu    = {{ (float)(WIN_W/2+  10),(float)(WIN_H/2+40), 130, 44 }, "Main Menu",  {80,80,80,255},{110,110,110,255}};
    }
public:
    SudokuGame() :
        screen(MENU), selRow(-1), selCol(-1),
        difficulty(1), mistakes(0), hintsLeft(3),
        solved(false),
        statusTimer(0), statusIsError(false),
        winAlpha(0), flashTimer(0), fontLoaded(false)
    {
        playerName = "Player";
    }
    void run() {
        srand((unsigned)time(nullptr));

        InitWindow(WIN_W, WIN_H, "Sudoku - OOP C++ Project");
        SetTargetFPS(60);

        try {
            fontMain   = LoadFontEx("resources/Montserrat-Bold.ttf", 40, nullptr, 0);
            fontLoaded = (fontMain.texture.id != 0);
            if (!fontLoaded)
                gLogger.log("Custom font not found – using default font");
        }
        catch (...) {
            fontLoaded = false;
            gLogger.log("Exception loading font – using default font");
        }

        buildButtons();

        while (!WindowShouldClose()) {
            float dt = GetFrameTime();
            if (flashTimer  > 0) flashTimer  -= dt;
            if (statusTimer > 0) statusTimer -= dt;

            if (screen == MENU) {
                int key = GetCharPressed();
                while (key > 0) {
                    if (key >= 32 && key <= 125 && playerName.size() < 20)
                        playerName += (char)key;
                    key = GetCharPressed();
                }
                if (IsKeyPressed(KEY_BACKSPACE) && !playerName.empty())
                    playerName.pop_back();

                if (btnEasy.isClicked()) startGame(1);
                if (btnMed .isClicked()) startGame(2);
                if (btnHard.isClicked()) startGame(3);
            }
            else if (screen == PLAYING) {
                handlePlayInput();
            }
            else if (screen == WIN || screen == GAMEOVER) {
                winAlpha = min(1.0f, winAlpha + dt * 2.0f);
                if (btnPlayAgain.isClicked()) screen = MENU;
                if (btnToMenu   .isClicked()) screen = MENU;
            }
            else if (screen == SCORES) {
                btnToMenu = {{ (float)(WIN_W/2-80), WIN_H-70, 160, 44 },
                               "Back", C_BTN, C_BTN_HOV};
                if (btnToMenu.isClicked()) screen = MENU;
            }

            BeginDrawing();
            ClearBackground(C_BG);

            if (screen == MENU) {
                drawMenu();
            }
            else if (screen == PLAYING) {
                render(selRow, selCol);
                drawPanel();
            }
            else if (screen == WIN || screen == GAMEOVER) {
                render(selRow, selCol);
                drawPanel();
                drawOverlay();
            }
            else if (screen == SCORES) {
                drawScores();
            }

            drawStatusBar();
            drawText("Sudoku  - C++ Raylib", 10, WIN_H-22, 13,
                     Fade(GRAY, 0.4f));

            EndDrawing();
        }

        if (fontLoaded) UnloadFont(fontMain);
        CloseWindow();
    }
};
int main() {
    try {
        SudokuGame game;
        game.run();
    }
    catch (const SudokuException& e) {
        TraceLog(LOG_ERROR, "SudokuException in main: %s", e.what());
        return 1;
    }
    catch (const std::exception& e) {
        TraceLog(LOG_ERROR, "std::exception in main: %s", e.what());
        return 2;
    }
    catch (...) {
        TraceLog(LOG_ERROR, "Unknown exception in main");
        return 3;
    }
    return 0;
}
