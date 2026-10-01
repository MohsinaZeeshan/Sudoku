# Sudoku Game - C++

A complete Sudoku game developed in C++ using Object-Oriented Programming concepts. The project combines puzzle generation, solving, validation, scoring, file handling, and game management into an interactive Sudoku application.

## Features

* Three difficulty levels:

  * Easy
  * Medium
  * Hard
* Sudoku puzzle generation using recursive backtracking
* Sudoku puzzle solving using recursive backtracking
* Puzzle uniqueness checking
* User input validation
* Mistake tracking with a maximum of 10 mistakes
* Hint system
* Auto Solve option
* Clear Board option
* Game timer
* Save and Load game functionality
* High score system
* Game logging
* Custom exception handling
* Object-Oriented Programming with inheritance
* Interactive game interface

## Difficulty Levels

| Difficulty | Cells Removed | Hints |
| ---------- | ------------- | ----- |
| Easy       | 35            | 5     |
| Medium     | 45            | 3     |
| Hard       | 55            | 1     |

The player can make up to 10 mistakes before the game ends.

## OOP Structure

The project is organized into multiple classes, with each class handling a specific part of the game.

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

Inherits from `SudokuSolver` and manages the overall game, including difficulty levels, hints, mistakes, saving, loading, scoring, and user interaction.

## Sudoku Solving

The project uses a recursive backtracking algorithm to solve Sudoku puzzles.

The general process is:

1. Find an empty cell.
2. Try values from 1 to 9.
3. Check whether the value is valid in the row, column, and 3x3 box.
4. Place the value if it is valid.
5. Recursively continue with the next empty cell.
6. Backtrack when a valid solution cannot be completed.

The backtracking technique is also used during puzzle generation.

## Puzzle Uniqueness

The project checks the number of possible solutions for a generated puzzle to ensure that the puzzle has a unique solution.

## Game Features

Players can interact with the Sudoku board and use different game functions, including:

* Entering values
* Requesting hints
* Clearing entered values
* Automatically solving the puzzle
* Saving the current game
* Loading a saved game
* Tracking mistakes
* Viewing high scores
* Exiting the game

## Save and Load

The game supports saving and loading game progress.

Saved game data is stored in:

`game_save.txt`

This allows a player to continue a game at a later time.

## High Scores

High score information is stored in:

`scores.txt`

The scoring system records information from completed games.

## Logging

Game events and errors are recorded in:

`game_log.txt`

The logger helps keep track of important actions and exceptions during gameplay.

## Technologies Used

* C++
* Object-Oriented Programming
* Recursion
* Backtracking
* File Handling
* Exception Handling
* Standard Template Library (STL)
* Random Number Generation
* User Input/Output

## Concepts Demonstrated

This project demonstrates several important C++ and programming concepts:

* Classes and Objects
* Encapsulation
* Inheritance
* Exception Handling
* Recursion
* Backtracking
* Vectors and Strings
* File Input/Output
* Random Number Generation
* Sorting and Score Management
* Modular Program Design

## How to Compile

Using a standard C++ compiler:

```bash
g++ sudoku.cpp -o sudoku
```

Run the program:

```bash
./sudoku
```

On Windows:

```bash
sudoku.exe
```

If the project uses a graphical framework such as Raylib, configure the required library and build settings before compiling.

## Project Structure

```text
Sudoku/
│
├── sudoku.cpp
├── game_save.txt
├── scores.txt
├── game_log.txt
└── README.md
```

The text files are created or updated by the program when required.

## Learning Purpose

This project was developed to practice C++ Object-Oriented Programming, recursion, backtracking, exception handling, file handling, and problem-solving by implementing a complete Sudoku game with multiple features and game-management functionality.
