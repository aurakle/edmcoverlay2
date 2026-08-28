{ pkgs ? import <nixpkgs> { } }:

let
  libs = with pkgs; [
    libX11
    libXcursor
    libxcb
    libXi
    libxkbcommon
    libGL
    libxcomposite
  ];
in pkgs.mkShell {
  name = "edmcoverlay2";

  buildInputs = libs ++ (with pkgs; [
  ]);

  LD_LIBRARY_PATH = pkgs.lib.makeLibraryPath libs;
}
