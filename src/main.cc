#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>

#include <array>
#include <cctype>
#include <charconv>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <ftxui/component/component.hpp>
#include <ftxui/component/component_base.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>

extern char** environ;

namespace {

// Local namespace alias keeps lines short without leaking ftxui names into
// other translation units (this file uses an anonymous namespace).
namespace ft = ftxui;

// ---- Per-game config ----

struct GameConfig {
  bool engine_on = true;
  int depth_idx = 2;  // index into kDepthChoices
  bool play_black = false;
  bool unicode_view = true;
};

inline constexpr std::array<int, 6> kDepthChoices = {1, 2, 3, 4, 5, 6};

// ---- Config persistence (INI) ----

std::filesystem::path ConfigPath() {
  namespace fs = std::filesystem;
  if (const char* xdg = std::getenv("XDG_CONFIG_HOME"); xdg && *xdg) {
    return fs::path(xdg) / "tuig" / "config.ini";
  }
  if (const char* home = std::getenv("HOME"); home && *home) {
    return fs::path(home) / ".config" / "tuig" / "config.ini";
  }
  return fs::path(".tuig.ini");
}

std::string TrimCopy(std::string s) {
  auto is_ws = [](unsigned char c) { return std::isspace(c); };
  while (!s.empty() && is_ws(s.front())) {
    s.erase(0, 1);
  }
  while (!s.empty() && is_ws(s.back())) {
    s.pop_back();
  }
  return s;
}

bool ParseBool(const std::string& v, bool def) {
  std::string s;
  for (char c : v) {
    s.push_back(std::tolower(static_cast<unsigned char>(c)));
  }
  if (s == "true" || s == "1" || s == "yes" || s == "on") {
    return true;
  }
  if (s == "false" || s == "0" || s == "no" || s == "off") {
    return false;
  }
  return def;
}

int ParseInt(const std::string& v, int def) {
  int out = 0;
  const char* first = v.data();
  const char* last = v.data() + v.size();
  auto [ptr, ec] = std::from_chars(first, last, out);
  if (ec != std::errc{} || ptr != last) {
    return def;
  }
  return out;
}

using Ini = std::map<std::string, std::map<std::string, std::string>>;

Ini ReadIni(const std::filesystem::path& path) {
  Ini ini;
  std::ifstream f(path);
  if (!f) {
    return ini;
  }
  std::string line;
  std::string section;
  while (std::getline(f, line)) {
    line = TrimCopy(line);
    if (line.empty() || line[0] == '#' || line[0] == ';') {
      continue;
    }
    if (line.front() == '[' && line.back() == ']') {
      section = line.substr(1, line.size() - 2);
      continue;
    }
    auto eq = line.find('=');
    if (eq == std::string::npos) {
      continue;
    }
    std::string key = TrimCopy(line.substr(0, eq));
    std::string val = TrimCopy(line.substr(eq + 1));
    if (!section.empty() && !key.empty()) {
      ini[section][key] = val;
    }
  }
  return ini;
}

bool WriteIni(const std::filesystem::path& path, const Ini& ini) {
  std::error_code ec;
  std::filesystem::create_directories(path.parent_path(), ec);
  std::ofstream f(path, std::ios::trunc);
  if (!f) {
    return false;
  }
  f << "# TUIG configuration. Auto-generated; safe to hand-edit.\n";
  for (const auto& [section, kv] : ini) {
    f << "\n[" << section << "]\n";
    for (const auto& [k, v] : kv) {
      f << k << "=" << v << "\n";
    }
  }
  return f.good();
}

GameConfig FromIniSection(const std::map<std::string, std::string>& kv,
                          GameConfig def) {
  GameConfig c = def;
  if (auto it = kv.find("engine_on"); it != kv.end()) {
    c.engine_on = ParseBool(it->second, c.engine_on);
  }
  if (auto it = kv.find("depth_idx"); it != kv.end()) {
    c.depth_idx = ParseInt(it->second, c.depth_idx);
  }
  if (auto it = kv.find("play_black"); it != kv.end()) {
    c.play_black = ParseBool(it->second, c.play_black);
  }
  if (auto it = kv.find("unicode_view"); it != kv.end()) {
    c.unicode_view = ParseBool(it->second, c.unicode_view);
  }
  if (c.depth_idx < 0) {
    c.depth_idx = 0;
  }
  if (c.depth_idx >= static_cast<int>(kDepthChoices.size())) {
    c.depth_idx = static_cast<int>(kDepthChoices.size()) - 1;
  }
  return c;
}

std::map<std::string, std::string> ToIniSection(const GameConfig& c) {
  return {
      {"engine_on", c.engine_on ? "true" : "false"},
      {"depth_idx", std::to_string(c.depth_idx)},
      {"play_black", c.play_black ? "true" : "false"},
      {"unicode_view", c.unicode_view ? "true" : "false"},
  };
}

// ---- Game definitions ----

struct GameDef {
  std::string key;          // INI section + display
  std::string name;
  std::string description;
  std::string binary;       // executable name (PATH-resolved)
  GameConfig (*defaults)();
};

// Defaults mirror chesscli/damacli library defaults
// (chess::Config{} / dama::Config{}). Keep in sync if upstream changes.
GameConfig SharedDefaults() {
  GameConfig c;
  c.engine_on = true;
  c.depth_idx = 2;       // depth = 3
  c.play_black = false;  // engine plays Black
  c.unicode_view = true;
  return c;
}

std::vector<GameDef> BuildGames() {
  return {
      {"chess", "Chess", "Standard 8x8 chess with minimax engine", "chesscli",
       &SharedDefaults},
      {"dama", "Checkers (Dama)", "Turkish-style draughts with minimax engine",
       "damacli", &SharedDefaults},
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

// ---- Argv builder (chesscli/damacli share this flag vocabulary today) ----

std::vector<std::string> BuildArgv(const GameConfig& c) {
  std::vector<std::string> a;
  if (!c.unicode_view) {
    a.emplace_back("--text");
  }
  if (!c.engine_on) {
    a.emplace_back("--no-engine");
    return a;
  }
  a.emplace_back("--engine");
  a.emplace_back(std::to_string(kDepthChoices[c.depth_idx]));
  if (c.play_black) {
    a.emplace_back("--play-black");
  }
  return a;
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

enum class Page { kMenu, kConfig };

struct AppState {
  Page page = Page::kMenu;
  int game_idx = 0;

  std::map<std::string, GameConfig> configs;

  // Stable string buffers backing ftxui Menu / Radiobox components.
  // Populated once at startup; their addresses must outlive the components.
  std::vector<std::string> game_entries;
  std::vector<std::string> depth_labels;

  // Scratch (config editor binds here).
  bool edit_engine_on = true;
  int edit_depth_idx = 2;
  bool edit_play_black = false;
  bool edit_unicode_view = true;

  std::string flash;  // shown on menu after action
  bool flash_error = false;
};

void LoadConfigs(AppState* st, const std::vector<GameDef>& games) {
  Ini ini = ReadIni(ConfigPath());
  for (const auto& g : games) {
    GameConfig def = g.defaults();
    auto it = ini.find(g.key);
    st->configs[g.key] =
        it == ini.end() ? def : FromIniSection(it->second, def);
  }
}

bool SaveConfigs(const AppState& st) {
  Ini ini;
  for (const auto& [key, cfg] : st.configs) {
    ini[key] = ToIniSection(cfg);
  }
  return WriteIni(ConfigPath(), ini);
}

std::string Summarize(const GameConfig& c) {
  std::string v = c.unicode_view ? "Unicode" : "ASCII";
  if (!c.engine_on) {
    return v + " | solo (2-player)";
  }
  return v + " | vs engine depth " +
         std::to_string(kDepthChoices[c.depth_idx]) +
         (c.play_black ? " | you play Black" : " | you play White");
}

// ---- Page renderers ----

ft::Component MakeMenuPage(AppState* st, const std::vector<GameDef>& games,
                           std::function<void()> on_start,
                           std::function<void()> on_configure,
                           std::function<void()> on_quit) {
  ft::MenuOption menu_opt;
  menu_opt.on_enter = on_start;
  auto menu = ft::Menu(&st->game_entries, &st->game_idx, menu_opt);

  auto cfg_btn =
      ft::Button("Configure (c)", on_configure, ft::ButtonOption::Ascii());
  auto quit_btn = ft::Button("Quit", on_quit, ft::ButtonOption::Ascii());

  auto layout = ft::Container::Vertical({
      menu,
      ft::Container::Horizontal({cfg_btn, quit_btn}),
  });

  return ft::Renderer(layout, [st, &games, menu, cfg_btn, quit_btn] {
    const auto& g = games[st->game_idx];
    auto cfg_it = st->configs.find(g.key);
    std::string current = cfg_it == st->configs.end()
                              ? std::string("defaults")
                              : Summarize(cfg_it->second);
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
               ft::text("Current config: " + current) | ft::dim,
               flash,
               ft::separator(),
               ft::hbox(
                   {cfg_btn->Render(), ft::text(" "), quit_btn->Render()}),
               ft::filler(),
               ft::text("Esc or Ctrl-C to quit") | ft::dim | ft::center,
           }) |
           ft::border;
  });
}

ft::Component MakeConfigPage(AppState* st, const std::vector<GameDef>& games,
                             std::function<void()> on_save,
                             std::function<void()> on_cancel) {
  auto unicode_box = ft::Checkbox("Unicode board", &st->edit_unicode_view);
  auto engine_box = ft::Checkbox("Play against engine", &st->edit_engine_on);
  auto depth_choice =
      ft::Radiobox(&st->depth_labels, &st->edit_depth_idx);
  auto black_box = ft::Checkbox("Play as Black", &st->edit_play_black);

  auto save_btn = ft::Button("Save", on_save, ft::ButtonOption::Ascii());
  auto cancel_btn =
      ft::Button("Cancel", on_cancel, ft::ButtonOption::Ascii());

  auto layout = ft::Container::Vertical({
      unicode_box,
      engine_box,
      depth_choice,
      black_box,
      ft::Container::Horizontal({save_btn, cancel_btn}),
  });

  return ft::Renderer(layout, [st, &games, unicode_box, engine_box,
                               depth_choice, black_box, save_btn, cancel_btn] {
    const auto& g = games[st->game_idx];
    bool engine = st->edit_engine_on;
    ft::Element depth_view =
        engine ? depth_choice->Render()
               : ft::text("(disabled: solo mode)") | ft::dim;
    ft::Element black_view =
        engine ? black_box->Render()
               : ft::text("(disabled: solo mode)") | ft::dim;
    return ft::vbox({
               ft::text("Configure: " + g.name) | ft::bold | ft::center,
               ft::text("Saved to " + ConfigPath().string()) | ft::dim |
                   ft::center,
               ft::separator(),
               unicode_box->Render(),
               ft::text(""),
               engine_box->Render(),
               ft::text(""),
               ft::text("Engine strength:") | ft::dim,
               depth_view,
               ft::text(""),
               black_view,
               ft::separator(),
               ft::hbox({save_btn->Render(), ft::text(" "),
                         cancel_btn->Render()}),
               ft::filler(),
               ft::text("Tab navigate | Space toggle | Enter activate | Esc "
                        "cancel") |
                   ft::dim | ft::center,
           }) |
           ft::border;
  });
}

void PopulateStaticBuffers(AppState* st, const std::vector<GameDef>& games) {
  st->game_entries.clear();
  for (const auto& g : games) {
    st->game_entries.push_back(g.name + "  --  " + g.description);
  }
  st->depth_labels.clear();
  for (int d : kDepthChoices) {
    st->depth_labels.push_back("Depth " + std::to_string(d));
  }
}

}  // namespace

int main(int /*argc*/, char* argv[]) {
  auto screen = ft::ScreenInteractive::Fullscreen();
  AppState state;
  auto games = BuildGames();
  LoadConfigs(&state, games);
  PopulateStaticBuffers(&state, games);

  auto current_cfg = [&state, &games]() -> GameConfig& {
    return state.configs[games[state.game_idx].key];
  };

  auto copy_to_scratch = [&state, &current_cfg] {
    auto& c = current_cfg();
    state.edit_engine_on = c.engine_on;
    state.edit_depth_idx = c.depth_idx;
    state.edit_play_black = c.play_black;
    state.edit_unicode_view = c.unicode_view;
  };

  auto goto_config = [&state, &copy_to_scratch] {
    copy_to_scratch();
    state.flash.clear();
    state.flash_error = false;
    state.page = Page::kConfig;
  };
  auto save_config = [&state, &current_cfg] {
    auto& c = current_cfg();
    c.engine_on = state.edit_engine_on;
    c.depth_idx = state.edit_depth_idx;
    c.play_black = state.edit_play_black;
    c.unicode_view = state.edit_unicode_view;
    bool ok = SaveConfigs(state);
    state.flash =
        ok ? "Saved to " + ConfigPath().string() : "Failed to save config";
    state.flash_error = !ok;
    state.page = Page::kMenu;
  };
  auto cancel_config = [&state] { state.page = Page::kMenu; };

  auto start_game = [&state, &games, &screen, argv, &current_cfg] {
    const auto& g = games[state.game_idx];
    std::string bin = ResolveBinary(g.binary, argv[0]);
    auto args = BuildArgv(current_cfg());
    int rc = -1;
    screen.WithRestoredIO([&] { rc = RunChild(bin, args); })();
    if (rc < 0) {
      state.flash = "Could not launch '" + g.binary +
                    "'. Ensure it is on PATH " +
                    "(brew install) or set TUIG_GAMES_DIR.";
      state.flash_error = true;
    } else {
      state.flash.clear();
      state.flash_error = false;
    }
  };

  auto quit = [&screen] { screen.Exit(); };

  ft::Component menu_page =
      MakeMenuPage(&state, games, start_game, goto_config, quit);
  ft::Component config_page =
      MakeConfigPage(&state, games, save_config, cancel_config);

  int selected_page = 0;
  auto pages =
      ft::Container::Tab({menu_page, config_page}, &selected_page);

  auto root = ft::Renderer(pages, [&state, menu_page, config_page,
                                   &selected_page] {
    selected_page = static_cast<int>(state.page);
    return state.page == Page::kMenu ? menu_page->Render()
                                     : config_page->Render();
  });

  auto root_with_keys = ft::CatchEvent(
      root, [&state, &screen, &goto_config, &cancel_config](ft::Event e) {
        if (state.page == Page::kMenu) {
          if (e == ft::Event::Character('c') ||
              e == ft::Event::Character('C')) {
            goto_config();
            return true;
          }
        }
        if (e == ft::Event::Escape) {
          if (state.page == Page::kMenu) {
            screen.Exit();
            return true;
          }
          cancel_config();
          return true;
        }
        return false;
      });

  screen.Loop(root_with_keys);
  return 0;
}
