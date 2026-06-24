# Contributing

Thank you for considering a contribution to Where Is That?

I am building an open-source Win32 C++ desktop app. I am not very experienced with C++ and Win32 architecture, so volunteer code review is especially welcome. Thoughtful feedback, even without a code change, is a real contribution.

## Helpful Feedback Areas

Areas where feedback would be especially helpful:

- Win32, WTL, ATL, and message-loop architecture.
- C++20 correctness, ownership, lifetime, and error handling.
- SQLite schema design, query performance, and transaction safety.
- Threading, background scanning, cancellation, and UI responsiveness.
- Native Windows UX conventions and accessibility.
- Build, packaging, release workflow, and MSVC project structure.
- Tests, smoke-test coverage, and ways to make regressions easier to catch.
- Security, path handling, malformed input, and read-only/protected catalog behavior.

If something looks fragile, confusing, overcomplicated, or unlike normal Win32/C++ practice, please say so. Clear explanations and suggested alternatives are very helpful.

## Before Contributing

Please read the project specification in [`docs/spec/`](docs/spec/) before proposing architecture or behavior changes. It contains the canonical product, storage, UI, coding, and acceptance decisions for the project.

This project intentionally uses:

- C++20
- Win32 API
- WTL/ATL
- SQLite C API
- MSVC and MSBuild project files

It intentionally does not use .NET, WPF, C#, Qt, Electron, Python, CMake, vcpkg, or a cross-platform runtime for the app itself.

## Development Setup

Requirements and build instructions are documented in the [`README.md`](README.md).

The usual local build path is:

1. Open `whereisthat.sln` in Visual Studio.
2. Select `Release` + `x64`.
3. Build Solution.
4. Run `x64/Release/WhereIsThat.exe`.

From a Developer Command Prompt for Visual Studio, MSBuild can also be used:

```text
msbuild whereisthat.sln /p:Configuration=Release /p:Platform=x64 /m
```

## Pull Requests

Small, focused pull requests are easiest to review. Please keep unrelated formatting, refactoring, dependency, and behavior changes separate when possible.

For code changes, please include:

- A short description of the problem being solved.
- A summary of the approach.
- Any relevant notes about user-visible behavior or compatibility.
- The build or tests you ran.

If a change touches catalog storage, scanning, Save/Discard behavior, threading, or app startup, please call that out in the pull request description.

## Code Style

Follow the existing style in nearby code. Prefer straightforward Win32/C++ code over broad rewrites or new abstractions unless the change clearly reduces complexity.

Keep the current technology choices intact unless an issue or discussion has explicitly agreed to a different direction.

## Issues and Discussions

Bug reports, design questions, architecture review, and implementation suggestions are all welcome.

When reporting a bug, please include:

- Windows version.
- App version or commit.
- Steps to reproduce.
- Expected behavior.
- Actual behavior.
- Any relevant catalog/source details, without sharing private file names if that matters to you.

Thanks for helping make this project better.
