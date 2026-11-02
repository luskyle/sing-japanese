#![cfg_attr(not(debug_assertions), windows_subsystem = "windows")]

mod progress;

use progress::{load, save, Progress};
use std::sync::Mutex;
use tauri::Manager;

struct StorageLock(Mutex<()>);

#[tauri::command]
fn load_progress(
    app: tauri::AppHandle,
    lock: tauri::State<StorageLock>,
) -> Result<Progress, String> {
    let _guard = lock.0.lock().map_err(|error| error.to_string())?;
    let path = app
        .path()
        .app_data_dir()
        .map_err(|error| error.to_string())?;
    load(&path.join("progress.json"))
}

#[tauri::command]
fn save_progress(
    app: tauri::AppHandle,
    lock: tauri::State<StorageLock>,
    progress: Progress,
) -> Result<(), String> {
    let _guard = lock.0.lock().map_err(|error| error.to_string())?;
    let path = app
        .path()
        .app_data_dir()
        .map_err(|error| error.to_string())?;
    save(&path.join("progress.json"), &progress)
}

fn main() {
    tauri::Builder::default()
        .manage(StorageLock(Mutex::new(())))
        .invoke_handler(tauri::generate_handler![load_progress, save_progress])
        .run(tauri::generate_context!())
        .expect("无法启动假名小调桌面应用");
}
