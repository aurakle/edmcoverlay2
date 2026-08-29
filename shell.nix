{ pkgs ? import <nixpkgs> { } }:

let
  libs = with pkgs; [
    libX11
    libXcursor
    libxcb
    libXi
    libxkbcommon
    libGL
    libxft
    libxcomposite
  ];
in pkgs.mkShell {
  name = "edmcoverlay2";

  buildInputs = libs ++ (with pkgs; [
    gcc
    gnumake
  ]);

  LD_LIBRARY_PATH = pkgs.lib.makeLibraryPath libs;
}
