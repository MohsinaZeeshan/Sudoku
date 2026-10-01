# Sudoku Game - C++ Console Version

A console-based Sudoku game developed in C++ using Object-Oriented Programming concepts. This version is adapted from the original Raylib-based Sudoku project by replacing the graphical interface with a command-line interface while keeping the main game logic and features.

## Features

- Three difficulty levels:
  - Easy
  - Medium
  - Hard
- Sudoku puzzle generation using recursive backtracking
- Sudoku puzzle solving using recursive backtracking
- Puzzle uniqueness checking
- User input validation
- Mistake tracking with a maximum of 10 mistakes
- Hint system
- Auto Solve option
- Clear Board option
- Game timer
- Save and Load game functionality
- High score system
- Game logging
- Custom exception handling
- Object-Oriented Programming with inheritance
- Console-based menus and game interaction

## Difficulty Levels

| Difficulty | Cells Removed | Hints |
|------------|---------------|-------|
| Easy | 35 | 5 |
| Medium | 45 | 3 |
| Hard | 55 | 1 |

The player can make up to 10 mistakes before the game ends.

## OOP Structure

The project is divided into multiple classes to separate different responsibilities.

### SudokuException

Base custom exception class used for handling Sudoku-related errors.

### FileIOException

Handles file input/output errors.

### CorruptSaveException

Handles invalid or corrupted saved-game data.

### InvalidCellValueException

Handles invalid Sudoku cell values.

### InvalidScoreException

Handles invalid score information.

### GameLogger

Records important game events and errors in `game_log.txt`.

### GameTimer

Keeps track of the game duration.

### ScoreEntry

Stores information related to high scores.

### SudokuBoard

Responsible for the Sudoku board and core board operations.

### SudokuSolver

Inherits from `SudokuBoard` and provides Sudoku solving and puzzle-generation functionality using recursive backtracking.

### SudokuGame

Inherits from `SudokuSolver` and manages the overall game, including difficulty, hints, mistakes, saving, loading, scoring, and console interaction.

## Sudoku Solving

The project uses a recursive backtracking algorithm to solve Sudoku puzzles.

The general process is:

1. Find an empty cell.
2. Try values from 1 to 9.
3. Check whether the value is valid in the row, column, and 3x3 box.
4. Place the value if it is valid.
5. Recursively continue with the next empty cell.
6. Backtrack when a valid solution cannot be completed.

The same backtracking approach is also used during puzzle generation.

## Puzzle Uniqueness

The project checks the number of possible solutions for a generated puzzle. This helps ensure that the generated Sudoku puzzle has a unique solution instead of multiple possible solutions.

## Game Controls

The console version uses keyboard input and menus instead of Raylib mouse buttons and graphical screens.

Typical options include:

- Enter a value
- Request a hint
- Clear the board
- Auto Solve
- Save Game
- Load Game
- Return to menu
- Exit

## Save and Load

The game can save its current state to:

`game_save.txt`

Saved information can be loaded later to continue the game.

## High Scores

High score information is stored in:

`scores.txt`

The score system keeps track of completed games and their relevant score information.

## Logging

Game events and errors are recorded in:

`game_log.txt`

This provides a simple record of important actions and exceptions during gameplay.

## Technologies Used

- C++
- Object-Oriented Programming
- Recursion
- Backtracking
- File Handling
- Exception Handling
- Standard Template Library (STL)
- Console Input/Output

## Concepts Demonstrated

This project demonstrates several important C++ and Data Structures concepts:

- Classes and Objects
- Encapsulation
- Inheritance
- Polymorphism through class design
- Exception Handling
- Recursion
- Backtracking
- Vectors and Strings
- File Input/Output
- Random Number Generation
- Sorting and Score Management
- Modular Program Design

## How to Compile

Using a standard C++ compiler:

```bash
g++ sudoku_console.cpp -o sudoku
```

Run the program:

```bash
./sudoku
```

On Windows, the executable can be run as:

```bash
sudoku.exe
```

## Project Files

```text
Sudoku/
│
├── sudoku_console.cpp
├── game_save.txt
├── scores.txt
├── game_log.txt
└── README.md
```

The text files are created or updated by the program when required.

## Original Raylib Version

The original project used Raylib for its graphical interface, including menus, buttons, mouse interaction, fonts, and different game screens.

This console version removes the Raylib-specific graphical components and replaces them with text-based menus while preserving the main Sudoku logic, game functionality, and Object-Oriented structure.

## Learning Purpose

This project was developed to practice C++ Object-Oriented Programming, recursion, backtracking, exception handling, file handling, and problem-solving through a complete playable application.
