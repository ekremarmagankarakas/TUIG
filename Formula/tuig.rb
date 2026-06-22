class Tuig < Formula
  desc "Terminal UI Games launcher with chess and Turkish draughts"
  homepage "https://github.com/ekremarmagankarakas/TUIG"
  url "https://github.com/ekremarmagankarakas/TUIG.git",
      using:    :git,
      tag:      "v0.2.3",
      revision: "8d29e904d216e502aaffb04b169cc1451fe40b5c"
  license "MIT"
  head "https://github.com/ekremarmagankarakas/TUIG.git", branch: "main"

  depends_on "cmake" => :build

  def install
    # chesscli, damacli, and vendor/ftxui are git submodules; fetch them
    # before configuring CMake (GitHub tarballs would omit submodule content).
    system "git", "submodule", "update", "--init", "--recursive"

    system "cmake", "-S", ".", "-B", "build",
                    "-DCMAKE_BUILD_TYPE=Release",
                    *std_cmake_args
    system "cmake", "--build", "build", "--parallel"

    bin.install "build/tuig",
                "build/chesscli/chesscli",
                "build/damacli/damacli"
  end

  test do
    system bin/"chesscli", "--help"
    system bin/"damacli",  "--help"
  end
end
