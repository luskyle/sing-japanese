import { invoke, isTauri } from "@tauri-apps/api/core";
import { emptyProgress, parseProgress, type Progress } from "../features/kana/progress";

const key = "sing-japanese:progress:v1";
export const desktop = isTauri();

export async function loadProgress(): Promise<Progress> {
  if (desktop) return parseProgress(await invoke<unknown>("load_progress"));
  const stored = localStorage.getItem(key);
  return stored === null ? emptyProgress() : parseProgress(JSON.parse(stored));
}

export async function saveProgress(progress: Progress): Promise<void> {
  if (desktop) await invoke("save_progress", { progress });
  else localStorage.setItem(key, JSON.stringify(progress));
}
