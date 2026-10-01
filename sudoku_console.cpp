#include <iostream>
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
#include <cctype>
#include <chrono>
#include <limits>

using namespace std;

class SudokuException : public exception {
protected:
    string message;
public:
    explicit SudokuException(const string& msg) : message(msg) {}
    const char* what() const noexcept override {
        return message.c_str();
    }
};

class FileIOException : public SudokuException {
    string filename;
    string operation;
public:
    FileIOException(const string& file, const string& op, const string& detail = "")
        : SudokuException("File " + op + " error on '" + file + "'" +
                          (detail.empty() ? "" : ": " + detail)),
          filename(file), operation(op) {}

    const string& getFilename() const { return filename; }
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
                          " at cell (" + to_string(r + 1) + "," +
                          to_string(c + 1) + ")"),
          row(r), col(c), value(v) {}

    int getRow() const { return row; }
    int getCol() const { return col; }
    int getValue() const { return value; }
};

class InvalidScoreException : public SudokuException {
public:
    explicit InvalidScoreException(const string& detail)
        : SudokuException("Invalid score entry: " + detail) {}
};

class GameLogger {
    string logPath;
    ofstream logFile;
    bool enabled;

    string timestamp() const {
        time_t now = time(nullptr);
        tm* t = localtime(&now);
        char buf[32];
        strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", t);
        return string(buf);
    }

public:
    explicit GameLogger(const string& path = "game_log.txt")
        : logPath(path), enabled(false) {
        try {
            logFile.open(path, ios::app);
            if (!logFile.is_open())
                throw FileIOException(path, "open", strerror(errno));

            enabled = true;
            log("=== Session started ===");
        }
        catch (const FileIOException&) {
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
        catch (const FileIOException&) {
            enabled = false;
        }
    }

    bool isEnabled() const {
        return enabled;
    }
};

static GameLogger gLogger;

class GameTimer {
    chrono::steady_clock::time_point startTime;
    int savedElapsed;
    bool running;

public:
    GameTimer() : savedElapsed(0), running(false) {}

    void start() {
        startTime = chrono::steady_clock::now();
        running = true;
    }

    void stop() {
        if (running) {
            savedElapsed = elapsed();
            running = false;
        }
    }

    void reset() {
        savedElapsed = 0;
        running = false;
    }

    void setElapsed(int seconds) {
        savedElapsed = seconds;
        running = false;
    }

    int elapsed() const {
        if (!running)
            return savedElapsed;

        auto now = chrono::steady_clock::now();
        auto current = chrono::duration_cast<chrono::seconds>(
            now - startTime).count();

        return savedElapsed + static_cast<int>(current);
    }

    string str() const {
        int s = elapsed();
        int m = s / 60;
        s %= 60;

        ostringstream oss;
        oss << setw(2) << setfill('0') << m << ":"
            << setw(2) << setfill('0') << s;

        return oss.str();
    }
};

struct ScoreEntry {
    string name;
    string difficulty;
    int seconds;
    int mistakes;
};

class SudokuBoard {
protected:
    int board[9][9];
    int solution[9][9];
    bool fixed[9][9];
    bool hinted[9][9];

public:
    SudokuBoard() {
        for (int r = 0; r < 9; r++) {
            for (int c = 0; c < 9; c++) {
                board[r][c] = 0;
                solution[r][c] = 0;
                fixed[r][c] = false;
                hinted[r][c] = false;
            }
        }
    }

    int getCell(int r, int c) const {
        return board[r][c];
    }

    int getSolution(int r, int c) const {
        return solution[r][c];
    }

    bool isFixed(int r, int c) const {
        return fixed[r][c];
    }

    bool isHinted(int r, int c) const {
        return hinted[r][c];
    }

    bool setCell(int r, int c, int v) {
        if (r < 0 || r >= 9 || c < 0 || c >= 9)
            throw InvalidCellValueException(r, c, v);

        if (v < 0 || v > 9)
            throw InvalidCellValueException(r, c, v);

        if (fixed[r][c] || hinted[r][c])
            return false;

        board[r][c] = v;
        return true;
    }

    bool isValidPlacement(int r, int c, int v, int g[9][9]) const {
        if (v < 1 || v > 9)
            return false;

        for (int j = 0; j < 9; j++)
            if (j != c && g[r][j] == v)
                return false;

        for (int i = 0; i < 9; i++)
            if (i != r && g[i][c] == v)
                return false;

        int br = (r / 3) * 3;
        int bc = (c / 3) * 3;

        for (int i = br; i < br + 3; i++) {
            for (int j = bc; j < bc + 3; j++) {
                if ((i != r || j != c) && g[i][j] == v)
                    return false;
            }
        }

        return true;
    }

    bool hasConflict(int r, int c) const {
        int v = board[r][c];

        if (v == 0)
            return false;

        return !isValidPlacement(
            r, c, v,
            const_cast<int (*)[9]>(board)
        );
    }

    bool isBoardComplete() const {
        for (int r = 0; r < 9; r++) {
            for (int c = 0; c < 9; c++) {
                if (board[r][c] == 0 || hasConflict(r, c))
                    return false;
            }
        }

        return true;
    }

    virtual void displayBoard() const {}
};

class SudokuSolver : public SudokuBoard {
public:
    bool solve(int g[9][9]) {
        for (int r = 0; r < 9; r++) {
            for (int c = 0; c < 9; c++) {
                if (g[r][c] == 0) {
                    int nums[9] = {1,2,3,4,5,6,7,8,9};

                    for (int k = 8; k > 0; k--) {
                        int j = rand() % (k + 1);
                        swap(nums[k], nums[j]);
                    }

                    for (int i = 0; i < 9; i++) {
                        if (isValidPlacement(r, c, nums[i], g)) {
                            g[r][c] = nums[i];

                            if (solve(g))
                                return true;

                            g[r][c] = 0;
                        }
                    }

                    return false;
                }
            }
        }

        return true;
    }

    int countSolutions(int g[9][9], int cap = 2) {
        for (int r = 0; r < 9; r++) {
            for (int c = 0; c < 9; c++) {
                if (g[r][c] == 0) {
                    int cnt = 0;

                    for (int v = 1; v <= 9 && cnt < cap; v++) {
                        if (isValidPlacement(r, c, v, g)) {
                            g[r][c] = v;
                            cnt += countSolutions(g, cap - cnt);
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
            for (int c = 0; c < 9; c++)
                solution[r][c] = 0;

        solve(solution);
    }

    void generatePuzzle(int diff) {
        generateSolution();

        for (int r = 0; r < 9; r++) {
            for (int c = 0; c < 9; c++) {
                board[r][c] = solution[r][c];
                fixed[r][c] = true;
                hinted[r][c] = false;
            }
        }

        int toRemove = (diff == 1) ? 35 :
                       (diff == 2) ? 45 : 55;

        vector<pair<int, int>> positions;

        for (int r = 0; r < 9; r++)
            for (int c = 0; c < 9; c++)
                positions.push_back({r, c});

        static mt19937 rng(
            static_cast<unsigned>(time(nullptr))
        );

        shuffle(positions.begin(), positions.end(), rng);

        int removed = 0;

        for (auto& p : positions) {
            if (removed >= toRemove)
                break;

            int r = p.first;
            int c = p.second;

            int backup = board[r][c];

            board[r][c] = 0;
            fixed[r][c] = false;

            int temp[9][9];

            for (int i = 0; i < 9; i++)
                for (int j = 0; j < 9; j++)
                    temp[i][j] = board[i][j];

            if (countSolutions(temp) != 1) {
                board[r][c] = backup;
                fixed[r][c] = true;
            }
            else {
                removed++;
            }
        }
    }

    void autoSolve() {
        for (int r = 0; r < 9; r++)
            for (int c = 0; c < 9; c++)
                board[r][c] = solution[r][c];
    }
};

class SudokuGame : public SudokuSolver {
    enum Screen {
        MENU,
        PLAYING,
        WIN,
        GAMEOVER,
        SCORES
    };

    Screen screen;

    int selRow;
    int selCol;
    int difficulty;
    int mistakes;
    int hintsLeft;

    GameTimer timer;

    string playerName;
    bool solved;

    vector<ScoreEntry> highScores;

    void clearInput() {
        cin.clear();
        cin.ignore(numeric_limits<streamsize>::max(), '\n');
    }

    string difficultyName() const {
        if (difficulty == 1) return "Easy";
        if (difficulty == 2) return "Medium";
        return "Hard";
    }

    void pause() {
        cout << "\nPress Enter to continue...";
        cin.ignore(numeric_limits<streamsize>::max(), '\n');
    }

    void displayBoard() const override {
        cout << "\n        1 2 3   4 5 6   7 8 9\n";
        cout << "      +-------+-------+-------+\n";

        for (int r = 0; r < 9; r++) {
            cout << "    " << char('A' + r) << " | ";

            for (int c = 0; c < 9; c++) {
                if (board[r][c] == 0)
                    cout << ".";
                else
                    cout << board[r][c];

                cout << " ";

                if (c == 2 || c == 5)
                    cout << "| ";
            }

            cout << "|\n";

            if (r == 2 || r == 5 || r == 8)
                cout << "      +-------+-------+-------+\n";
        }
    }

    void showGameInfo() const {
        cout << "\nPlayer: " << playerName
             << " | Difficulty: " << difficultyName()
             << " | Time: " << timer.str()
             << " | Mistakes: " << mistakes << "/10"
             << " | Hints: " << hintsLeft << "\n";
    }

    void loadScores() {
        highScores.clear();

        try {
            ifstream f("scores.txt");

            if (!f.is_open())
                throw FileIOException(
                    "scores.txt", "open", strerror(errno)
                );

            string line;
            int lineNumber = 0;

            while (getline(f, line)) {
                lineNumber++;

                if (line.empty())
                    continue;

                try {
                    istringstream ss(line);
                    ScoreEntry e;
                    string timeText, mistakeText;

                    if (!getline(ss, e.name, ',') ||
                        !getline(ss, e.difficulty, ',') ||
                        !getline(ss, timeText, ',') ||
                        !getline(ss, mistakeText, ',')) {
                        throw CorruptSaveException(
                            "missing fields", lineNumber
                        );
                    }

                    for (char ch : timeText) {
                        if (!isdigit(static_cast<unsigned char>(ch)))
                            throw InvalidScoreException(
                                "non-numeric time '" + timeText + "'"
                            );
                    }

                    for (char ch : mistakeText) {
                        if (!isdigit(static_cast<unsigned char>(ch)))
                            throw InvalidScoreException(
                                "non-numeric mistakes '" + mistakeText + "'"
                            );
                    }

                    e.seconds = stoi(timeText);
                    e.mistakes = stoi(mistakeText);

                    if (e.seconds < 0 || e.seconds > 99999)
                        throw InvalidScoreException(
                            "time out of range"
                        );

                    if (e.mistakes < 0 || e.mistakes > 10)
                        throw InvalidScoreException(
                            "mistakes out of range"
                        );

                    highScores.push_back(e);
                }
                catch (const InvalidScoreException& e) {
                    gLogger.log(
                        string("Score load warning: ") + e.what()
                    );
                }
                catch (const CorruptSaveException& e) {
                    gLogger.log(
                        string("Score load warning: ") + e.what()
                    );
                }
            }

            f.close();

            sort(
                highScores.begin(),
                highScores.end(),
                [](const ScoreEntry& a, const ScoreEntry& b) {
                    return a.seconds < b.seconds;
                }
            );

            gLogger.log(
                "Scores loaded (" +
                to_string(highScores.size()) +
                " entries)"
            );
        }
        catch (const FileIOException& e) {
            gLogger.log(
                string("loadScores: ") + e.what()
            );
        }
        catch (const exception& e) {
            gLogger.log(
                string("loadScores unexpected error: ") + e.what()
            );
        }
    }

    void saveScore() {
        try {
            ofstream f("scores.txt", ios::app);

            if (!f.is_open())
                throw FileIOException(
                    "scores.txt", "write", strerror(errno)
                );

            f << playerName << ","
              << difficultyName() << ","
              << timer.elapsed() << ","
              << mistakes << "\n";

            if (!f.good())
                throw FileIOException(
                    "scores.txt", "write", "stream error"
                );

            f.close();

            gLogger.log(
                "Score saved for " +
                playerName + " [" +
                difficultyName() + "]"
            );
        }
        catch (const FileIOException& e) {
            gLogger.log(
                string("saveScore error: ") + e.what()
            );
        }
    }

    void saveGame() {
        try {
            ofstream f("game_save.txt");

            if (!f.is_open())
                throw FileIOException(
                    "game_save.txt", "open", strerror(errno)
                );

            f << playerName << "\n";
            f << difficulty << "\n";
            f << timer.elapsed() << "\n";
            f << mistakes << "\n";
            f << hintsLeft << "\n";

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
                    f << static_cast<int>(fixed[r][c]);
                }
                f << "\n";
            }

            for (int r = 0; r < 9; r++) {
                for (int c = 0; c < 9; c++) {
                    if (c) f << " ";
                    f << static_cast<int>(hinted[r][c]);
                }
                f << "\n";
            }

            if (!f.good())
                throw FileIOException(
                    "game_save.txt", "write", "stream error"
                );

            f.close();

            gLogger.log(
                "Game saved for player: " + playerName
            );

            cout << "\nGame saved successfully.\n";
        }
        catch (const FileIOException& e) {
            gLogger.log(
                string("saveGame error: ") + e.what()
            );

            cout << "\nError: " << e.what() << "\n";
        }
    }

    void loadGame() {
        try {
            ifstream f("game_save.txt");

            if (!f.is_open())
                throw FileIOException(
                    "game_save.txt",
                    "open",
                    "No saved game found."
                );

            int lineNumber = 0;

            auto nextLine = [&]() -> string {
                string line;

                if (!getline(f, line))
                    throw CorruptSaveException(
                        "unexpected end of file",
                        lineNumber
                    );

                lineNumber++;
                return line;
            };

            auto readInt = [&](const string& fieldName) -> int {
                string s = nextLine();

                if (s.empty())
                    throw CorruptSaveException(
                        "empty value for " + fieldName,
                        lineNumber
                    );

                for (char ch : s) {
                    if (!isdigit(static_cast<unsigned char>(ch)) &&
                        ch != '-') {
                        throw CorruptSaveException(
                            "non-integer value for " +
                            fieldName + " = '" + s + "'",
                            lineNumber
                        );
                    }
                }

                return stoi(s);
            };

            string name = nextLine();

            if (name.empty())
                throw CorruptSaveException(
                    "player name is empty",
                    lineNumber
                );

            int diff = readInt("difficulty");

            if (diff < 1 || diff > 3)
                throw CorruptSaveException(
                    "difficulty out of range",
                    lineNumber
                );

            int elapsed = readInt("elapsed");

            if (elapsed < 0)
                throw CorruptSaveException(
                    "negative elapsed time",
                    lineNumber
                );

            int errs = readInt("mistakes");

            if (errs < 0 || errs > 10)
                throw CorruptSaveException(
                    "mistakes out of range",
                    lineNumber
                );

            int hints = readInt("hintsLeft");

            if (hints < 0)
                throw CorruptSaveException(
                    "negative hintsLeft",
                    lineNumber
                );

            auto readGrid = [&](int grid[9][9],
                                const string& gridName) {
                for (int r = 0; r < 9; r++) {
                    string line = nextLine();
                    istringstream ss(line);

                    for (int c = 0; c < 9; c++) {
                        int v;

                        if (!(ss >> v))
                            throw CorruptSaveException(
                                gridName + " row " +
                                to_string(r) +
                                ": missing value at column " +
                                to_string(c),
                                lineNumber
                            );

                        if (v < 0 || v > 9)
                            throw CorruptSaveException(
                                gridName +
                                " contains value outside 0-9",
                                lineNumber
                            );

                        grid[r][c] = v;
                    }
                }
            };

            int tempBoard[9][9];
            int tempSolution[9][9];
            int tempFixed[9][9];
            int tempHinted[9][9];

            readGrid(tempBoard, "board");
            readGrid(tempSolution, "solution");
            readGrid(tempFixed, "fixed");
            readGrid(tempHinted, "hinted");

            f.close();

            playerName = name;
            difficulty = diff;
            mistakes = errs;
            hintsLeft = hints;
            selRow = -1;
            selCol = -1;
            solved = false;

            for (int r = 0; r < 9; r++) {
                for (int c = 0; c < 9; c++) {
                    board[r][c] = tempBoard[r][c];
                    solution[r][c] = tempSolution[r][c];
                    fixed[r][c] = static_cast<bool>(tempFixed[r][c]);
                    hinted[r][c] = static_cast<bool>(tempHinted[r][c]);
                }
            }

            timer.reset();
            timer.setElapsed(elapsed);
            timer.start();

            screen = PLAYING;

            gLogger.log(
                "Game loaded for player: " + playerName
            );

            cout << "\nGame loaded successfully.\n";
        }
        catch (const FileIOException& e) {
            gLogger.log(
                string("loadGame: ") + e.what()
            );

            cout << "\nNo saved game found.\n";
        }
        catch (const CorruptSaveException& e) {
            gLogger.log(
                string("loadGame: ") + e.what()
            );

            cout << "\nSave file is corrupt and cannot be loaded.\n";
        }
        catch (const exception& e) {
            gLogger.log(
                string("loadGame unexpected error: ") +
                e.what()
            );

            cout << "\nUnexpected load error.\n";
        }
    }

    void doHint() {
        if (hintsLeft <= 0) {
            cout << "\nNo hints left.\n";
            return;
        }

        vector<pair<int, int>> empty;

        for (int r = 0; r < 9; r++) {
            for (int c = 0; c < 9; c++) {
                if (!fixed[r][c] &&
                    !hinted[r][c] &&
                    board[r][c] == 0) {
                    empty.push_back({r, c});
                }
            }
        }

        if (empty.empty()) {
            cout << "\nNo empty cells available for a hint.\n";
            return;
        }

        int index = rand() % empty.size();

        int r = empty[index].first;
        int c = empty[index].second;

        board[r][c] = solution[r][c];
        hinted[r][c] = true;
        hintsLeft--;

        cout << "\nHint placed at "
             << char('A' + r) << c + 1
             << ". Value: " << board[r][c] << "\n";

        gLogger.log(
            "Hint used at (" +
            to_string(r) + "," +
            to_string(c) + ")"
        );

        if (isBoardComplete()) {
            finishWin();
        }
    }

    void doSolve() {
        autoSolve();

        timer.stop();
        screen = WIN;
        solved = false;

        gLogger.log(
            "Auto-solve used by " + playerName
        );

        cout << "\nPuzzle automatically solved.\n";
    }

    void doClear() {
        for (int r = 0; r < 9; r++) {
            for (int c = 0; c < 9; c++) {
                if (!fixed[r][c] && !hinted[r][c])
                    board[r][c] = 0;
            }
        }

        mistakes = 0;

        gLogger.log(
            "Board cleared by " + playerName
        );

        cout << "\nBoard cleared.\n";
    }

    void finishWin() {
        timer.stop();
        solved = true;

        saveScore();
        loadScores();

        screen = WIN;

        gLogger.log(
            "Puzzle solved by " + playerName +
            " in " + timer.str() +
            " with " + to_string(mistakes) +
            " mistakes"
        );
    }

    bool enterMove() {
        string cell;
        int value;

        cout << "\nEnter cell (example A1) or 0 to cancel: ";
        cin >> cell;

        if (cell == "0")
            return false;

        if (cell.size() != 2) {
            cout << "Invalid cell format. Use A1 to I9.\n";
            clearInput();
            return false;
        }

        char rowChar = static_cast<char>(
            toupper(static_cast<unsigned char>(cell[0]))
        );

        char colChar = cell[1];

        if (rowChar < 'A' || rowChar > 'I' ||
            colChar < '1' || colChar > '9') {
            cout << "Invalid cell. Use A1 to I9.\n";
            return false;
        }

        selRow = rowChar - 'A';
        selCol = colChar - '1';

        if (fixed[selRow][selCol]) {
            cout << "This is a fixed cell and cannot be changed.\n";
            return false;
        }

        if (hinted[selRow][selCol]) {
            cout << "This cell was filled by a hint and cannot be changed.\n";
            return false;
        }

        cout << "Enter value (1-9, or 0 to clear): ";
        cin >> value;

        if (!cin || value < 0 || value > 9) {
            cout << "Invalid value. Enter a number from 0 to 9.\n";
            clearInput();
            return false;
        }

        try {
            if (value == 0) {
                setCell(selRow, selCol, 0);
                return true;
            }

            if (value != solution[selRow][selCol]) {
                mistakes++;

                cout << "\nIncorrect value.\n";
                cout << "Mistakes: " << mistakes << "/10\n";

                gLogger.log(
                    "Wrong move at (" +
                    to_string(selRow) + "," +
                    to_string(selCol) +
                    ") val=" + to_string(value) +
                    " mistakes=" + to_string(mistakes)
                );

                if (mistakes >= 10) {
                    timer.stop();
                    screen = GAMEOVER;

                    gLogger.log(
                        "Game over for " + playerName
                    );

                    return false;
                }
            }

            setCell(selRow, selCol, value);

            if (hasConflict(selRow, selCol)) {
                cout << "Warning: this value conflicts with another value.\n";
            }

            if (isBoardComplete()) {
                finishWin();
            }

            return true;
        }
        catch (const InvalidCellValueException& e) {
            cout << "Error: " << e.what() << "\n";

            gLogger.log(
                string("Cell value exception: ") +
                e.what()
            );
        }

        return false;
    }

    void startGame(int diff) {
        difficulty = diff;
        hintsLeft = (diff == 1) ? 5 :
                    (diff == 2) ? 3 : 1;

        mistakes = 0;
        selRow = -1;
        selCol = -1;
        solved = false;

        timer.reset();
        generatePuzzle(diff);
        timer.start();

        screen = PLAYING;

        gLogger.log(
            "New game started: " +
            playerName + " [" +
            difficultyName() + "]"
        );
    }

    void showScores() {
        loadScores();

        cout << "\n============================================\n";
        cout << "                 HIGH SCORES\n";
        cout << "============================================\n";

        cout << left
             << setw(20) << "Name"
             << setw(12) << "Difficulty"
             << setw(10) << "Time"
             << setw(10) << "Mistakes" << "\n";

        cout << "--------------------------------------------\n";

        int count = 0;

        for (const auto& e : highScores) {
            if (count++ >= 10)
                break;

            int minutes = e.seconds / 60;
            int seconds = e.seconds % 60;

            cout << left
                 << setw(20) << e.name
                 << setw(12) << e.difficulty
                 << setfill('0') << setw(2) << minutes
                 << ":"
                 << setw(2) << seconds
                 << setfill(' ')
                 << setw(5) << ""
                 << setw(10) << e.mistakes
                 << "\n";
        }

        if (highScores.empty())
            cout << "No scores yet.\n";

        cout << "============================================\n";
    }

    void playGame() {
        while (screen == PLAYING) {
            system("cls");

            cout << "============================================\n";
            cout << "                 SUDOKU GAME\n";
            cout << "============================================\n";

            showGameInfo();
            displayBoard();

            cout << "\nOptions:\n";
            cout << "1. Enter / Change Number\n";
            cout << "2. Use Hint\n";
            cout << "3. Auto Solve\n";
            cout << "4. Clear Board\n";
            cout << "5. Save Game\n";
            cout << "6. Load Game\n";
            cout << "7. High Scores\n";
            cout << "8. Main Menu\n";
            cout << "9. Show Solution\n";
            cout << "0. Exit Game\n";

            cout << "\nEnter choice: ";

            int choice;
            cin >> choice;

            if (!cin) {
                clearInput();
                cout << "Invalid choice.\n";
                pause();
                continue;
            }

            switch (choice) {
            case 1:
                enterMove();
                if (screen == PLAYING)
                    pause();
                break;

            case 2:
                doHint();
                if (screen == PLAYING)
                    pause();
                break;

            case 3:
                doSolve();
                pause();
                break;

            case 4:
                doClear();
                pause();
                break;

            case 5:
                saveGame();
                pause();
                break;

            case 6:
                loadGame();
                pause();
                break;

            case 7:
                showScores();
                pause();
                break;

            case 8:
                timer.stop();
                screen = MENU;
                break;

            case 9:
                cout << "\nSolution:\n";

                for (int r = 0; r < 9; r++) {
                    for (int c = 0; c < 9; c++) {
                        cout << solution[r][c] << " ";

                        if (c == 2 || c == 5)
                            cout << "| ";
                    }

                    cout << "\n";

                    if (r == 2 || r == 5)
                        cout << "------+-------+------\n";
                }

                pause();
                break;

            case 0:
                timer.stop();
                screen = MENU;
                return;

            default:
                cout << "Invalid choice.\n";
                pause();
            }
        }
    }

    void showEndScreen() {
        cout << "\n============================================\n";

        if (screen == WIN) {
            cout << "              PUZZLE SOLVED!\n";
            cout << "============================================\n";
            cout << "Player: " << playerName << "\n";
            cout << "Difficulty: " << difficultyName() << "\n";
            cout << "Time: " << timer.str() << "\n";
            cout << "Mistakes: " << mistakes << "\n";
        }
        else {
            cout << "                GAME OVER\n";
            cout << "============================================\n";
            cout << "You reached 10 mistakes.\n";
            cout << "The puzzle was not completed.\n";
        }

        cout << "\n1. Play Again\n";
        cout << "2. Main Menu\n";
        cout << "Enter choice: ";

        int choice;
        cin >> choice;

        if (choice == 1)
            screen = MENU;
        else
            screen = MENU;
    }

    void menu() {
        while (screen == MENU) {
            system("cls");

            cout << "============================================\n";
            cout << "                  SUDOKU\n";
            cout << "           C++ Console OOP Project\n";
            cout << "============================================\n";

            cout << "\nPlayer Name: " << playerName << "\n";

            cout << "\n1. Start Easy Game\n";
            cout << "2. Start Medium Game\n";
            cout << "3. Start Hard Game\n";
            cout << "4. Load Saved Game\n";
            cout << "5. High Scores\n";
            cout << "6. Change Player Name\n";
            cout << "0. Exit\n";

            cout << "\nEnter choice: ";

            int choice;
            cin >> choice;

            if (!cin) {
                clearInput();
                cout << "Invalid choice.\n";
                pause();
                continue;
            }

            switch (choice) {
            case 1:
                startGame(1);
                break;

            case 2:
                startGame(2);
                break;

            case 3:
                startGame(3);
                break;

            case 4:
                loadGame();
                pause();
                break;

            case 5:
                showScores();
                pause();
                break;

            case 6:
                cout << "\nEnter player name: ";
                cin >> ws;
                getline(cin, playerName);

                if (playerName.empty())
                    playerName = "Player";

                break;

            case 0:
                return;

            default:
                cout << "Invalid choice.\n";
                pause();
            }

            if (screen == PLAYING)
                playGame();

            if (screen == WIN || screen == GAMEOVER) {
                showEndScreen();
            }
        }
    }

public:
    SudokuGame()
        : screen(MENU),
          selRow(-1),
          selCol(-1),
          difficulty(1),
          mistakes(0),
          hintsLeft(3),
          solved(false) {
        playerName = "Player";
    }

    void run() {
        srand(static_cast<unsigned>(time(nullptr)));

        try {
            menu();
        }
        catch (const SudokuException& e) {
            cerr << "\nSudoku error: " << e.what() << "\n";
        }
        catch (const exception& e) {
            cerr << "\nStandard error: " << e.what() << "\n";
        }
        catch (...) {
            cerr << "\nUnknown error occurred.\n";
        }
    }
};

int main() {
    try {
        SudokuGame game;
        game.run();
    }
    catch (const SudokuException& e) {
        cerr << "SudokuException in main: "
             << e.what() << "\n";
        return 1;
    }
    catch (const exception& e) {
        cerr << "std::exception in main: "
             << e.what() << "\n";
        return 2;
    }
    catch (...) {
        cerr << "Unknown exception in main.\n";
        return 3;
    }

    return 0;
}
