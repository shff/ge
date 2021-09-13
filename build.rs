fn main() {
    let target = std::env::var("TARGET").unwrap();
    if target.contains("darwin") {
        copy_asset("post.metal");
        copy_asset("quad.metal");
        cc::Build::new()
            .flag("-fmodules")
            .flag("-O3")
            .flag("-Wall")
            .flag("-Werror")
            .flag("-pedantic")
            .flag("-Wno-unused-parameter")
            .flag("-mmacosx-version-min=10.10")
            .file("src/native/macos.m")
            .compile("native.a");
    } else if target.contains("x86_64-apple-ios") || target.contains("aarch64-apple-ios-sim") {
        cc::Build::new()
            .flag("-fmodules")
            .flag("-O3")
            .flag("-Wall")
            .flag("-Werror")
            .flag("-pedantic")
            .flag("-Wno-unused-parameter")
            .flag("-mios-simulator-version-min=13.0")
            .file("src/native/ios.m")
            .compile("native.a");
    } else if target.contains("aarch64-apple-ios") {
        cc::Build::new()
            .flag("-fmodules")
            .flag("-O3")
            .flag("-Wall")
            .flag("-Werror")
            .flag("-pedantic")
            .flag("-Wno-unused-parameter")
            .file("src/native/ios.m")
            .compile("native.a");
    } else if target.contains("windows") {
        cc::Build::new()
            .flag("-Wall")
            .file("src/native/win32.c")
            .compile("native.a");
        println!("cargo:rustc-link-lib=user32");
        println!("cargo:rustc-link-lib=d3d11");
        println!("cargo:rustc-link-lib=dxguid");
        println!("cargo:rustc-link-lib=dsound");
        println!("cargo:rustc-link-lib=xinput");
    } else if target.contains("android") {
        cc::Build::new()
            .flag("-O3")
            .flag("-Wall")
            .flag("-Werror")
            .file("src/native/android.c")
            .compile("native.a");
    } else if target.contains("linux") {
        cc::Build::new()
            .flag("-O3")
            .flag("-Wall")
            .flag("-Werror")
            .flag("-Wl,-s")
            .flag("-Wno-unused-parameter")
            .flag("-Wno-unused-but-set-variable")
            .file("src/native/x11.c")
            .compile("native.a");
        println!("cargo:rustc-link-lib=X11");
        println!("cargo:rustc-link-lib=EGL");
        println!("cargo:rustc-link-lib=GL");
        println!("cargo:rustc-link-lib=asound");
    }
}

fn copy_asset(asset: &str) {
    println!("cargo:rerun-if-changed=src/assets/{}", asset);

    let manifest_dir_string = std::env::var("CARGO_MANIFEST_DIR").unwrap();
    let build_type = std::env::var("PROFILE").unwrap();
    let path = std::path::Path::new(&manifest_dir_string)
        .join("target")
        .join(build_type);
    let output_dir = std::path::PathBuf::from(path);

    let input_path = std::path::Path::new(&std::env::var("CARGO_MANIFEST_DIR").unwrap())
        .join("src/assets")
        .join(asset);
    let output_path = std::path::Path::new(&output_dir).join(asset);
    std::fs::copy(input_path, output_path).unwrap();
}
