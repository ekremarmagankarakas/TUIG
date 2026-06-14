class Tuig < Formula
  desc "Terminal UI Games launcher with chess and Turkish draughts"
  homepage "https://github.com/ekremarmagankarakas/TUIG"
  url "https://github.com/ekremarmagankarakas/TUIG.git",
      using:    :git,
      tag:      "v0.1.0",
      revision: "8a91c8ba31513c4be28bc07bf2fc8a8741189bc9"
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
