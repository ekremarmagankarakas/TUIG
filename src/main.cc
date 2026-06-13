#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <ftxui/component/component.hpp>
#include <ftxui/component/component_base.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

extern char** environ;

namespace {

// Local namespace alias keeps lines short without leaking ftxui names into
// other translation units (this file uses an anonymous namespace).
namespace ft = ftxui;

// ---- Game definitions ----
//
// Each game owns its own configuration entirely. The launcher only knows how
// to find the binary and invoke it; the game reads/writes its own config and
// renders its own `--configure` screen.

struct GameDef {
  std::string name;
  std::string description;
  std::string binary;  // executable name (PATH-resolved)
};

std::vector<GameDef> BuildGames() {
  return {
      {"Chess", "Standard 8x8 chess with minimax engine", "chesscli"},
      {"Checkers (Dama)", "Turkish-style draughts with minimax engine",
       "damacli"},
  };
}

// ---- Binary resolution ----

// Look for `binary` next to `base` or under `<base>/<binary>/<binary>`
// (cmake build layout).
std::optional<std::string> FindNear(std::string_view binary,
                                    const std::filesystem::path& base) {
  namespace fs = std::filesystem;
  std::error_code ec;
  auto p1 = base / binary;
  if (fs::is_regular_file(p1, ec)) {
    return p1.string();
  }
  auto p2 = base / binary / binary;
  if (fs::is_regular_file(p2, ec)) {
    return p2.string();
  }
  return std::nullopt;
}

// Resolution order:
//   1. $TUIG_GAMES_DIR/<binary>      (explicit override)
//   2. Same dir / subdir of argv[0]  (dev/build layout)
//   3. PATH                          (homebrew / installed)
std::string ResolveBinary(std::string_view name, const char* argv0) {
  namespace fs = std::filesystem;
  if (const char* dir = std::getenv("TUIG_GAMES_DIR"); dir && *dir) {
    if (auto r = FindNear(name, fs::path(dir))) {
      return *r;
    }
  }
  if (argv0 && *argv0) {
    fs::path argv0p(argv0);
    std::error_code ec;
    auto abs = fs::weakly_canonical(argv0p, ec);
    if (!ec) {
      argv0p = abs;
    }
    if (auto r = FindNear(name, argv0p.parent_path())) {
      return *r;
    }
  }
  return std::string(name);  // bare name -> posix_spawnp searches PATH
}

// ---- Spawn child, wait, return exit code (or -1 on failure to launch) ----

int RunChild(const std::string& binary, const std::vector<std::string>& args) {
  std::vector<char*> argv;
  argv.reserve(args.size() + 2);
  argv.push_back(const_cast<char*>(binary.c_str()));
  for (const auto& a : args) {
    argv.push_back(const_cast<char*>(a.c_str()));
  }
  argv.push_back(nullptr);

  pid_t pid = 0;
  int rc = posix_spawnp(&pid, binary.c_str(), nullptr, nullptr, argv.data(),
                        environ);
  if (rc != 0) {
    std::fprintf(stderr, "Failed to launch %s: %s\n", binary.c_str(),
                 std::strerror(rc));
    return -1;
  }
  int status = 0;
  if (waitpid(pid, &status, 0) < 0) {
    return -1;
  }
  if (WIFEXITED(status)) {
    return WEXITSTATUS(status);
  }
  return -1;
}

// ---- App state ----

struct AppState {
  int game_idx = 0;

  // Stable string buffer backing the ftxui Menu component.
  // Populated once at startup; addresses must outlive the component.
  std::vector<std::string> game_entries;

  std::string flash;  // shown on menu after action
  bool flash_error = false;
};

void PopulateStaticBuffers(AppState* st, const std::vector<GameDef>& games) {
  st->game_entries.clear();
  for (const auto& g : games) {
    st->game_entries.push_back(g.name + "  --  " + g.description);
  }
}

// ---- Page renderer ----

ft::Component MakeMenuPage(AppState* st, std::function<void()> on_play,
                           std::function<void()> on_configure,
                           std::function<void()> on_quit) {
  ft::MenuOption menu_opt;
  menu_opt.on_enter = on_play;
  auto menu = ft::Menu(&st->game_entries, &st->game_idx, menu_opt);

  auto play_btn =
      ft::Button("Play (Enter)", on_play, ft::ButtonOption::Ascii());
  auto cfg_btn =
      ft::Button("Configure (c)", on_configure, ft::ButtonOption::Ascii());
  auto quit_btn = ft::Button("Quit", on_quit, ft::ButtonOption::Ascii());

  auto layout = ft::Container::Vertical({
      menu,
      ft::Container::Horizontal({play_btn, cfg_btn, quit_btn}),
  });

  return ft::Renderer(layout, [st, menu, play_btn, cfg_btn, quit_btn] {
    ft::Element flash = ft::text("");
    if (!st->flash.empty()) {
      flash = ft::text(st->flash) |
              (st->flash_error ? ft::color(ft::Color::Red)
                               : ft::color(ft::Color::Green)) |
              ft::center;
    }
    return ft::vbox({
               ft::text("TUIG -- Terminal UI Games") | ft::bold | ft::center,
               ft::separator(),
               ft::text("Select a game (up/down), Enter to play, c to "
                        "configure") |
                   ft::dim,
               menu->Render() | ft::frame |
                   ft::size(ft::HEIGHT, ft::LESS_THAN, 10),
               ft::separator(),
               flash,
               ft::separator(),
               ft::hbox({play_btn->Render(), ft::text(" "), cfg_btn->Render(),
                         ft::text(" "), quit_btn->Render()}),
               ft::filler(),
               ft::text("Esc or Ctrl-C to quit") | ft::dim | ft::center,
           }) |
           ft::border;
  });
}

}  // namespace

int main(int /*argc*/, char* argv[]) {
  auto screen = ft::ScreenInteractive::Fullscreen();
  AppState state;
  auto games = BuildGames();
  PopulateStaticBuffers(&state, games);

  auto spawn = [&state, &games, &screen, argv](std::vector<std::string> args) {
    const auto& g = games[state.game_idx];
    std::string bin = ResolveBinary(g.binary, argv[0]);
    int rc = -1;
    screen.WithRestoredIO([&] { rc = RunChild(bin, std::move(args)); })();
    if (rc < 0) {
      state.flash = "Could not launch '" + g.binary +
                    "'. Ensure it is on PATH (brew install) or set "
                    "TUIG_GAMES_DIR.";
      state.flash_error = true;
    } else {
      state.flash.clear();
      state.flash_error = false;
    }
  };

  auto play_game = [&spawn] { spawn({}); };
  auto configure_game = [&spawn] { spawn({"--configure"}); };
  auto quit = [&screen] { screen.Exit(); };

  ft::Component menu_page =
      MakeMenuPage(&state, play_game, configure_game, quit);

  auto root_with_keys =
      ft::CatchEvent(menu_page, [&configure_game, &screen](ft::Event e) {
        if (e == ft::Event::Character('c') || e == ft::Event::Character('C')) {
          configure_game();
          return true;
        }
        if (e == ft::Event::Escape) {
          screen.Exit();
          return true;
        }
        return false;
      });

  screen.Loop(root_with_keys);
  return 0;
}
