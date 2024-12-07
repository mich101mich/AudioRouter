#![no_std]
#![allow(unused)]

use core::time::Duration;

use wdk::println;

#[cfg(not(test))]
extern crate wdk_panic; // TODO: add handler that logs first

#[cfg(not(test))]
use wdk_alloc::WdkAllocator;

#[cfg(not(test))]
#[global_allocator]
static GLOBAL_ALLOCATOR: WdkAllocator = WdkAllocator;

/// The result type used by the driver. Ok(...) means success, Err(NTSTATUS) means failure.
/// Note that `Err(STATUS_SUCCESS)` should not be used, as it is a contradiction.
pub type NtResult<T = ()> = Result<T, wdk_sys::NTSTATUS>;

pub use widestring::utf16str as unicode_str;
pub use widestring::Utf16Str as UnicodeStr;
pub use widestring::Utf16String as UnicodeString;

fn status_result(status: wdk_sys::NTSTATUS) -> NtResult {
    if wdk::nt_success(status) {
        Ok(())
    } else {
        Err(status)
    }
}

fn as_unicode_str(s: &wdk_sys::UNICODE_STRING) -> &UnicodeStr {
    // SAFETY: The only time this code will use a `UNICODE_STRING` is when it is passed from the
    // kernel, and if we can't trust the kernel to give us a valid `UNICODE_STRING`, we have bigger
    // problems.
    unsafe {
        let slice = core::slice::from_raw_parts(s.Buffer, s.Length as usize / 2);
        widestring::Utf16Str::from_slice_unchecked(slice)
    }
}
fn ptr_as_unicode_str<'a>(s: wdk_sys::PCUNICODE_STRING) -> &'a UnicodeStr {
    if s.is_null() {
        unicode_str!("")
    } else {
        // SAFETY: We just checked that `s` is not null. Any non-null pointers should be valid, see the comment above.
        unsafe { as_unicode_str(&*s) }
    }
}

fn sleep(time: Duration) -> NtResult {
    let time = time.as_nanos() as i128 / 100;
    let mut interval = wdk_sys::LARGE_INTEGER {
        QuadPart: -time as _,
    };
    let status = unsafe {
        wdk_sys::ntddk::KeDelayExecutionThread(
            wdk_sys::_MODE::KernelMode as _,
            0,
            &mut interval as *mut _,
        )
    };
    status_result(status)
}

struct Driver {
    inner: wdk_sys::PDRIVER_OBJECT,
}
impl core::ops::Deref for Driver {
    type Target = wdk_sys::DRIVER_OBJECT;
    fn deref(&self) -> &Self::Target {
        unsafe { &*self.inner }
    }
}

/// The "main" function of the driver.
#[export_name = "DriverEntry"]
pub unsafe extern "system" fn raw_driver_entry(
    driver: wdk_sys::PDRIVER_OBJECT,
    registry_path: wdk_sys::PCUNICODE_STRING,
) -> wdk_sys::NTSTATUS {
    let driver = Driver { inner: driver };
    let registry_path = ptr_as_unicode_str(registry_path);
    match driver_entry(driver, registry_path) {
        Ok(()) => wdk_sys::STATUS_SUCCESS,
        Err(e) => e,
    }
}

fn driver_entry(driver: Driver, registry_path: &UnicodeStr) -> NtResult {
    let name = as_unicode_str(&driver.DriverName);

    println!("Hello, world from driver_entry of driver {}", name);
    println!("Registry path: {}", registry_path);

    Ok(())
}
