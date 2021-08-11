#[repr(C)]
pub struct State {
    timer: f64,
    lag: f64,
    click_x: f32,
    click_y: f32,
    delta_x: f32,
    delta_y: f32,
    mode: i32,
}

#[no_mangle]
pub extern "C" fn update(state: State) {}
