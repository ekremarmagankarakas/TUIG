#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

extern char** environ;

namespace {

struct Game {
  std::string name;
  std::string description;
  std::string binary;
};

const std::vector<Game>& Games() {
  static const std::vector<Game> games = {
      {"Chess", "Standard 8x8 chess with minimax engine", TUIG_CHESS_BIN},
      {"Checkers (Dama)", "Turkish-style draughts with minimax engine",
       TUIG_DAMA_BIN},
  };
  return games;
}

std::string Trim(std::string s) {
  auto not_space = [](unsigned char c) { return !std::isspace(c); };
  s.erase(s.begin(), std::find_if(s.begin(), s.end(), not_space));
  s.erase(std::find_if(s.rbegin(), s.rend(), not_space).base(), s.end());
  return s;
}

std::string ReadLine(std::string_view prompt) {
  std::cout << prompt << std::flush;
  std::string line;
  if (!std::getline(std::cin, line)) return "";
  return Trim(line);
}

bool ReadYesNo(std::string_view prompt, bool default_yes) {
  std::string line = ReadLine(prompt);
  if (line.empty()) return default_yes;
  char c = std::tolower(static_cast<unsigned char>(line[0]));
  return c == 'y';
}

int ReadIntOr(std::string_view prompt, int default_value, int min_value) {
  std::string line = ReadLine(prompt);
  if (line.empty()) return default_value;
  try {
    int v = std::stoi(line);
    if (v < min_value) return default_value;
    return v;
  } catch (...) {
    return default_value;
  }
}

// Returns the chosen 0-based index, or empty optional to quit (q/EOF).
// Loops on invalid input.
std::optional<size_t> ReadGameChoice(size_t game_count) {
  while (true) {
    std::string line = ReadLine("Choose game (number, q to quit): ");
    if (std::cin.eof()) return std::nullopt;
    if (line.empty() || line == "q" || line == "Q") return std::nullopt;
    try {
      size_t idx = std::stoul(line);
      if (idx >= 1 && idx <= game_count) return idx - 1;
    } catch (...) {
    }
    std::cout << "Invalid choice.\n";
  }
}

void PrintMenu() {
  std::cout << "\n=== TUIG: Terminal UI Games ===\n";
  const auto& games = Games();
  for (size_t i = 0; i < games.size(); ++i) {
    std::cout << "  " << (i + 1) << ". " << games[i].name << " — "
              << games[i].description << "\n";
  }
  std::cout << "  q. Quit\n";
}

int RunChild(const std::string& binary, std::vector<std::string> args) {
  std::vector<char*> argv;
  argv.reserve(args.size() + 2);
  argv.push_back(const_cast<char*>(binary.c_str()));
  for (auto& a : args) argv.push_back(a.data());
  argv.push_back(nullptr);

  pid_t pid = 0;
  int rc = posix_spawnp(&pid, binary.c_str(), nullptr, nullptr, argv.data(),
                        environ);
  if (rc != 0) {
    std::cerr << "Failed to launch " << binary << ": " << std::strerror(rc)
              << "\n";
    return -1;
  }
  int status = 0;
  if (waitpid(pid, &status, 0) < 0) {
    std::perror("waitpid");
    return -1;
  }
  if (WIFEXITED(status)) return WEXITSTATUS(status);
  return -1;
}

// Build argv for chesscli/damacli from interactive prompts.
// Both share the same flag vocabulary today.
std::vector<std::string> PromptGameConfig() {
  std::vector<std::string> args;

  bool unicode = ReadYesNo("Use Unicode board? [Y/n]: ", true);
  if (!unicode) args.emplace_back("--text");

  bool engine = ReadYesNo("Play against engine? [Y/n]: ", true);
  if (!engine) {
    args.emplace_back("--no-engine");
    return args;
  }

  int depth = ReadIntOr("Engine depth [3]: ", 3, 1);
  args.emplace_back("--engine");
  args.emplace_back(std::to_string(depth));

  bool play_black = ReadYesNo("Play as Black? [y/N]: ", false);
  if (play_black) args.emplace_back("--play-black");

  return args;
}

}  // namespace

int main() {
  while (true) {
    PrintMenu();
    auto choice = ReadGameChoice(Games().size());
    if (!choice) {
      std::cout << "Goodbye.\n";
      return 0;
    }
    const auto& game = Games()[*choice];
    std::cout << "\n--- Configure " << game.name << " ---\n";
    auto args = PromptGameConfig();
    std::cout << "\nLaunching " << game.name << "...\n\n";
    int rc = RunChild(game.binary, std::move(args));
    if (rc != 0) {
      std::cout << "\n(" << game.name << " exited with code " << rc << ")\n";
    }
  }
}
