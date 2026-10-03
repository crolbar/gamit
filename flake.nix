{
  inputs.nixpkgs.url = "github:NixOS/nixpkgs/nixpkgs-unstable";

  outputs = inputs: let
    system = "x86_64-linux";
    pkgs = import inputs.nixpkgs {inherit system;};
    cross = pkgs.pkgsCross.mingwW64;

    # static raylib with its bundled glfw (no glfw DLL needed)
    raylibStatic = cross.raylib.overrideAttrs (old: {
      cmakeFlags =
        (old.cmakeFlags or [])
        ++ [
          "-DBUILD_SHARED_LIBS=OFF"
          "-DUSE_EXTERNAL_GLFW=OFF"
        ];
    });

curlStatic = (cross.curl.override {
  websocketSupport = true;
  opensslSupport = false;
  zlibSupport = true;
  http2Support = false;
  http3Support = false;
  brotliSupport = false;
  zstdSupport = false;
  scpSupport = false;
  gssSupport = false;
  idnSupport = false;
  pslSupport = false;
}).overrideAttrs (old: {
  configureFlags =
    (builtins.filter (f: f != "--without-ssl") (old.configureFlags or []))
    ++ [
      "--disable-shared"
      "--enable-static"
      "--with-schannel"
    ];
});

  in {	
    devShells.${system}.default = cross.mkShell {
      nativeBuildInputs = with pkgs; [
       pkg-config
      ];
      buildInputs = [
	curlStatic 
	raylibStatic
      ];
    };
  };
}
