# ClassCheck

A small Qt 6 desktop attendance app with a teacher login, student IDs, per-date attendance, and CSV export.

## Features

- First launch prompts you to create a teacher password; later launches require it.
- Register students by unique ID and name; remove a student and their records.
- Record each student as **Unmarked**, **Present**, **Absent**, **Late**, or **Excused** for the selected date.
- View a date-specific status summary and export that date to CSV.
- Stores data in a local SQLite database under the operating system's application-data folder.

## Build and run

Install Qt 6 with the **Widgets** and **SQL** modules, plus a C++17 compiler and CMake. Then from this repository directory:

```sh
cmake -S . -B build
cmake --build build
```

Run the resulting `classcheck` (Windows: `classcheck.exe`) executable from the build directory. If your Qt installation is not in the default CMake search path, configure `CMAKE_PREFIX_PATH` to the Qt installation's folder. The Qt SQLite driver (`QSQLITE`) must be available; Qt's standard desktop distributions include it.

This is a Qt desktop project, not a single-file Code::Blocks console project. Open the CMake project in an IDE configured for Qt 6, or build it with the commands above.

## Login note

The initial password is created interactively and is not hard-coded. The local password check uses a salted, iterated SHA-256 hash for this lightweight example. It is not intended for sensitive or production deployments; use a vetted password-hashing library and appropriate access controls for those situations. The database is local to the current user account and is not encrypted.
