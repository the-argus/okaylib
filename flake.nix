{
  description = "Dev environment for running tests of allo";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs?ref=nixos-unstable";
    flake-utils.url = "github:numtide/flake-utils";
  };

  outputs = {
    nixpkgs,
    flake-utils,
    ...
  }: let
    supportedSystems = let
      inherit (flake-utils.lib) system;
    in [
      system.aarch64-linux
      system.aarch64-darwin
      system.x86_64-linux
    ];
  in
    flake-utils.lib.eachSystem supportedSystems (system: let
      pkgs = import nixpkgs {inherit system;};
    in {
      devShell =
        pkgs.mkShell
        {
          packages =
            (with pkgs; [
              zig_0_15
              cmake

              # backtraces
              libbfd

              # code coverage (llvm-profdata)
              llvmPackages.bintools-unwrapped
            ])
            ++ pkgs.lib.optionals (system != flake-utils.lib.system.aarch64-darwin) (with pkgs; [
              gdb
              valgrind
            ]);

          shellHook = ''
            export LLVM_PROFILE_FILE="$(pwd)/%Nm-%b.profraw"
          '';
        };

      formatter = pkgs.alejandra;
    });
}
