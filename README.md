# TUIG — Terminal UI Games

A small launcher and collection of terminal-based games, built with C++20 and
[FTXUI](https://github.com/ArthurSonzogni/ftxui).

`tuig` is the launcher; each game is a standalone binary that owns its own
configuration. Pick a game from the menu and play, or configure it.

## Games

- **chesscli** — Standard 8x8 chess with a minimax engine.
- **damacli** — Turkish-style draughts (dama) with a minimax engine.

Each game runs on its own (`chesscli`, `damacli`) and reads its own config
file. The launcher is just a convenient entry point.

## Install (Homebrew)

```sh
brew tap ekremarmagankarakas/tuig https://github.com/ekremarmagankarakas/TUIG.git
brew trust ekremarmagankarakas/tuig
brew install tuig
```

The first two commands only need to be run once. After that, `brew upgrade
tuig` picks up new releases.

## Build (from source)

```sh
make build      # Release build
make debug      # Debug build with -O0 and ASan/UBSan
make test       # Configure with tests on, build, run ctest
make fresh      # Wipe build dir and do a clean Release build
```

The build uses CMake under the hood. New `.cc` files added to a game's `src/`
need a reconfigure (`make configure` or `make fresh`) because the per-game
`CMakeLists.txt` uses `file(GLOB)`.

## Run

```sh
make run        # Launches ./build/tuig
make chess      # Runs chesscli directly (ARGS="--help" etc.)
make dama       # Runs damacli directly
```

The launcher resolves game binaries in this order:

1. `$TUIG_GAMES_DIR/<binary>` if set
2. Next to the `tuig` executable (dev/build layout)
3. `PATH` (e.g. `brew install`)

## Configuration

Each game stores its own config (default: `$XDG_CONFIG_HOME/<game>/config.ini`,
falling back to `~/.config/<game>/config.ini`). To edit it interactively,
either:

- From the launcher, select a game and press `c` (or click *Configure*).
- From a shell, run `chesscli --configure` / `damacli --configure`.

Both routes invoke the same in-game configuration screen.

## Requirements

- C++20 compiler (clang++ or g++)
- CMake ≥ 3.20
- Make (for the convenience targets)

Build flags: `-std=c++20 -Wall -Wextra -Wpedantic -g`. Source follows the
[Google C++ Style Guide](https://google.github.io/styleguide/cppguide.html).

## Releasing (maintainer)

1. Commit and push the changes you want to ship on `main`.
2. Tag the release and push the tag:
   ```sh
   git tag v0.2.0 && git push origin v0.2.0
   ```
3. Grab the commit SHA the tag points at:
   ```sh
   git rev-parse v0.2.0^{commit}
   ```
4. In `Formula/tuig.rb`, update the two pinning lines under `url`:
   ```ruby
   tag:      "v0.2.0",
   revision: "<sha from step 3>"
   ```
5. Commit and push the formula change. Users get the new version with
   `brew update && brew upgrade tuig`.

## License

[MIT](LICENSE).
