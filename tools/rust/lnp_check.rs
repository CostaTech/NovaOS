use std::env;
use std::fs;
use std::process;

fn read_u16_le(data: &[u8], at: usize) -> u16 {
    u16::from(data[at]) | (u16::from(data[at + 1]) << 8)
}

fn main() {
    let path = match env::args().nth(1) {
        Some(p) => p,
        None => {
            eprintln!("Usage: rustc tools/rust/lnp_check.rs -o /tmp/lnp_check && /tmp/lnp_check image.lnp");
            process::exit(1);
        }
    };

    let data = match fs::read(&path) {
        Ok(d) => d,
        Err(e) => {
            eprintln!("Cannot read {}: {}", path, e);
            process::exit(1);
        }
    };

    if data.len() < 8 || &data[0..4] != b"LNP1" {
        eprintln!("Not an LNP1 file");
        process::exit(1);
    }

    let width = read_u16_le(&data, 4);
    let height = read_u16_le(&data, 6);
    let expected = usize::from(width) * usize::from(height) * 3 + 8;

    println!("LNP1 image: {}x{}", width, height);
    println!("File bytes: {}", data.len());
    println!("Expected bytes: {}", expected);

    if data.len() != expected {
        eprintln!("Invalid pixel data size");
        process::exit(1);
    }
}
