fn main() {
    println!("cargo:rustc-link-search=../");
    println!("cargo:rerun-if-changed=wrapper.h");
    println!("cargo:rerun-if-changed=../xxhash.h");

    let bindings = bindgen::Builder::default()
        .header("wrapper.h")
        .parse_callbacks(Box::new(bindgen::CargoCallbacks::new()))
        .blocklist_type("__uint128_t")
        .generate()
        .expect("Unable to generate bindings");

    bindings.write_to_file("src/bindings.rs")
        .expect("Could not write bindings");
}
