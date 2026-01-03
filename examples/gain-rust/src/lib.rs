//! Simple Gain Plugin - CLASP DSP Example (Rust)
//!
//! This demonstrates the minimal CLASP DSP ABI for an audio effect.
//!
//! Build:
//!   rustup target add wasm32-wasi
//!   cargo build --target wasm32-wasi --release
//!   cp target/wasm32-wasi/release/gain_rust.wasm dsp.wasm

#![no_std]

use core::slice;

// Constants
const NUM_CHANNELS: usize = 2;
const MAX_BLOCK_SIZE: usize = 4096;

// Global state (safe in WASM single-threaded environment)
static mut SAMPLE_RATE: f32 = 44100.0;
static mut MAX_BLOCK: usize = 512;
static mut GAIN: f32 = 1.0;

// Audio buffers
static mut INPUT_BUFFERS: [[f32; MAX_BLOCK_SIZE]; NUM_CHANNELS] = [[0.0; MAX_BLOCK_SIZE]; 2];
static mut OUTPUT_BUFFERS: [[f32; MAX_BLOCK_SIZE]; NUM_CHANNELS] = [[0.0; MAX_BLOCK_SIZE]; 2];

// State structure for save/restore
#[repr(C)]
struct PluginState {
    gain: f32,
}

static mut STATE: PluginState = PluginState { gain: 1.0 };

//-----------------------------------------------------------------------------
// CLASP DSP ABI Implementation
//-----------------------------------------------------------------------------

#[no_mangle]
pub extern "C" fn dsp_init(sample_rate: f32, max_block_size: i32) {
    unsafe {
        SAMPLE_RATE = sample_rate;
        MAX_BLOCK = (max_block_size as usize).min(MAX_BLOCK_SIZE);
        GAIN = 1.0;
    }
}

#[no_mangle]
pub extern "C" fn dsp_reset() {
    // Reset any internal state (none for this simple plugin)
}

#[no_mangle]
pub extern "C" fn dsp_process(block_size: i32) {
    let size = block_size as usize;

    unsafe {
        for ch in 0..NUM_CHANNELS {
            for i in 0..size {
                OUTPUT_BUFFERS[ch][i] = INPUT_BUFFERS[ch][i] * GAIN;
            }
        }
    }
}

#[no_mangle]
pub extern "C" fn dsp_set_param(param_id: i32, value: f32) {
    unsafe {
        match param_id {
            0 => GAIN = value,
            _ => {}
        }
    }
}

#[no_mangle]
pub extern "C" fn dsp_get_param(param_id: i32) -> f32 {
    unsafe {
        match param_id {
            0 => GAIN,
            _ => 0.0,
        }
    }
}

#[no_mangle]
pub extern "C" fn dsp_get_input_buffer(channel: i32) -> *mut f32 {
    if channel < 0 || channel >= NUM_CHANNELS as i32 {
        return core::ptr::null_mut();
    }
    unsafe { INPUT_BUFFERS[channel as usize].as_mut_ptr() }
}

#[no_mangle]
pub extern "C" fn dsp_get_output_buffer(channel: i32) -> *mut f32 {
    if channel < 0 || channel >= NUM_CHANNELS as i32 {
        return core::ptr::null_mut();
    }
    unsafe { OUTPUT_BUFFERS[channel as usize].as_mut_ptr() }
}

#[no_mangle]
pub extern "C" fn dsp_get_state_size() -> i32 {
    core::mem::size_of::<PluginState>() as i32
}

#[no_mangle]
pub extern "C" fn dsp_get_state(out: *mut u8) {
    unsafe {
        STATE.gain = GAIN;
        let state_bytes = slice::from_raw_parts(
            &STATE as *const PluginState as *const u8,
            core::mem::size_of::<PluginState>(),
        );
        core::ptr::copy_nonoverlapping(state_bytes.as_ptr(), out, state_bytes.len());
    }
}

#[no_mangle]
pub extern "C" fn dsp_set_state(input: *const u8) {
    unsafe {
        let state = &*(input as *const PluginState);
        GAIN = state.gain;
    }
}

// Panic handler for no_std
#[panic_handler]
fn panic(_info: &core::panic::PanicInfo) -> ! {
    loop {}
}
