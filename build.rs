fn main() {
    let target = std::env::var("TARGET").unwrap();
    if target.contains("darwin") {
        copy_asset("shaders.metal");
        cc::Build::new()
            .cpp(true)
            .flag("-fmodules")
            .flag("-fcxx-modules")
            .flag("--std=c++17")
            .flag("-O3")
            .flag("-Wall")
            .flag("-Werror")
            .flag("-pedantic")
            .flag("-Wno-unused-parameter")
            .flag("-mmacosx-version-min=10.10")
            .file("src/native/macos.mm")
            .compile("native.a");
        println!("cargo:rerun-if-changed=src/native/macos.mm");
    } else if target.contains("x86_64-apple-ios") || target.contains("aarch64-apple-ios-sim") {
        copy_asset("shaders.metal");
        cc::Build::new()
            .cpp(true)
            .flag("-fmodules")
            .flag("-fcxx-modules")
            .flag("--std=c++17")
            .flag("-O3")
            .flag("-Wall")
            .flag("-Werror")
            .flag("-pedantic")
            .flag("-Wno-unused-parameter")
            .flag("-mios-simulator-version-min=13.0")
            .file("src/native/ios.mm")
            .compile("native.a");
        println!("cargo:rerun-if-changed=src/native/ios.mm");
    } else if target.contains("aarch64-apple-ios") {
        copy_asset("shaders.metal");
        cc::Build::new()
            .cpp(true)
            .flag("-fmodules")
            .flag("-fcxx-modules")
            .flag("--std=c++17")
            .flag("-O3")
            .flag("-Wall")
            .flag("-Werror")
            .flag("-pedantic")
            .flag("-Wno-unused-parameter")
            .file("src/native/ios.mm")
            .compile("native.a");
        println!("cargo:rerun-if-changed=src/native/ios.mm");
    } else if target.contains("windows") {
        cc::Build::new()
            .cpp(true)
            .flag("-Wall")
            .flag("/std:c++20")
            .file("src/native/win32.cpp")
            .compile("native.a");
        println!("cargo:rustc-link-lib=user32");
        println!("cargo:rustc-link-lib=d3d11");
        println!("cargo:rustc-link-lib=d3dcompiler");
        println!("cargo:rustc-link-lib=dxguid");
        println!("cargo:rustc-link-lib=dsound");
        println!("cargo:rustc-link-lib=xinput");
    } else if target.contains("android") {
        let ndk_root = std::env::var("ANDROID_NDK_HOME").unwrap();
        let glue_path = "/sources/android/native_app_glue/android_native_app_glue.c";
        cc::Build::new()
            .cpp(true)
            .flag("-O3")
            .flag("-Wall")
            .flag("-Werror")
            .flag("-Wno-unused-parameter")
            .flag(format!("-I{}/sources/android/native_app_glue", ndk_root).as_str())
            .file(format!("{}{}", ndk_root, glue_path).as_str())
            .file("src/native/android.cpp")
            .compile("native.a");
        println!("cargo:rerun-if-changed=src/native/android.cpp");
    } else if target.contains("linux") {
        cc::Build::new()
            .cpp(true)
            .flag("--std=c++17")
            .flag("-O3")
            .flag("-Wall")
            .flag("-Werror")
            .flag("-Wl,-s")
            .flag("-Wno-unused-parameter")
            .flag("-Wno-unused-but-set-variable")
            .file("src/native/x11.cpp")
            .compile("native.a");
        println!("cargo:rustc-link-lib=X11");
        println!("cargo:rustc-link-lib=EGL");
        println!("cargo:rustc-link-lib=GL");
        println!("cargo:rustc-link-lib=GLEW");
        println!("cargo:rustc-link-lib=asound");
        println!("cargo:rerun-if-changed=src/native/x11.cpp");
    }
}

fn copy_asset(asset: &str) {
    println!("cargo:rerun-if-changed=src/assets/{}", asset);
    let input_path = std::path::Path::new(&std::env::var("CARGO_MANIFEST_DIR").unwrap())
        .join("src/assets")
        .join(asset);
    let output_path = std::path::Path::new(&std::env::var("OUT_DIR").unwrap())
        .ancestors()
        .nth(3)
        .unwrap()
        .join(asset);
    std::fs::copy(input_path, output_path).unwrap();
}
