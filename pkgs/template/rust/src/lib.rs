#![no_std]

use core::panic::PanicInfo;

const ZIRCAPP_ABI_V1: u32 = 1;
const ZIRCAPP_MODULE_EVENT_TOUCH_DOWN: u32 = 10;

#[repr(C)]
pub struct ZircAppEventV1 {
    pub event_type: u32,
    pub x: i32,
    pub y: i32,
    pub key: i32,
}

#[repr(C)]
pub struct ZircAppModuleV1 {
    pub abi_version: u32,
    pub struct_size: u32,
    pub name: *const u8,
    pub name_len: u32,
    pub init: Option<extern "C" fn()>,
    pub event: Option<extern "C" fn(*const ZircAppEventV1)>,
    pub stop: Option<extern "C" fn()>,
}

static NAME: &[u8] = b"Rust Example\0";

extern "C" fn on_event(event: *const ZircAppEventV1) {
    if event.is_null() {
        return;
    }
    let event = unsafe { &*event };
    if event.event_type == ZIRCAPP_MODULE_EVENT_TOUCH_DOWN {
        let _touch_position = (event.x, event.y);
    }
}

// The raw pointer refers only to the immutable NAME static.
unsafe impl Sync for ZircAppModuleV1 {}

static MODULE: ZircAppModuleV1 = ZircAppModuleV1 {
    abi_version: ZIRCAPP_ABI_V1,
    struct_size: core::mem::size_of::<ZircAppModuleV1>() as u32,
    name: NAME.as_ptr(),
    name_len: (NAME.len() - 1) as u32,
    init: None,
    event: Some(on_event),
    stop: None,
};

#[no_mangle]
pub extern "C" fn zircon_app_module_v1() -> *const ZircAppModuleV1 {
    &MODULE
}

#[panic_handler]
fn panic(_info: &PanicInfo) -> ! {
    loop {}
}
