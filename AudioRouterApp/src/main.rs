#![allow(non_snake_case)]

use std::io::{Read, Write};

fn main() {
    let dev_name = "\\??\\ROOT#SAMPLE#0000#{c9b7d8ce-7a5f-4165-b0f9-ee1a683cfbd8}";

    println!("Opening device");
    let mut file = std::fs::OpenOptions::new()
        .read(true)
        .write(true)
        .create(false)
        .open(dev_name)
        .unwrap();

    let input = "Hello, world!";
    file.write_all(input.as_bytes()).unwrap();

    println!("Data written. Reading back...");

    let mut s = String::new();
    s.extend(std::iter::repeat(' ').take(5));
    file.read_exact(unsafe { s.as_bytes_mut() }).unwrap();

    println!("Read first 5 bytes: {}", s);

    file.read_to_string(&mut s).unwrap();
    println!("Read everything: {}", s);

    assert_eq!(s, input);
}
