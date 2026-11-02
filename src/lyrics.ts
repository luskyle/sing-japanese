import { invoke } from "@tauri-apps/api/core";
import { desktop } from "./storage";

interface Segment { text: string; reading: string | null; unknown: boolean }
let input = "";
let segments: Segment[] | null = null;
let annotatedInput = "";
let busy = false;
let error = "";
let showReading = true;
const escape = (text: string): string => text.replace(/[&<>"']/g, (char) =>
  ({ "&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;", "'": "&#39;" })[char] ?? char);

export function lyricsView(): string {
  const unknown = segments?.filter((segment) => segment.unknown).length ?? 0;
  return `<section class="panel guide">
    <h2>粘贴歌词，自动标注假名</h2>
    <p>使用本地日语词典标注常规读音，不需要模型、网络或人工修正。按词注音，不匹配原唱的特殊读法。</p>
    ${!desktop ? '<p class="notice">注音需要 Rust 本地词典，请打开桌面版使用；浏览器预览不提供注音服务。</p>' : ""}
    <label for="lyrics-input">日语文本（最多 20,000 字符）</label>
    <textarea id="lyrics-input" class="lyrics-input" lang="ja" placeholder="在这里粘贴日语歌词…" ${busy ? "disabled" : ""}>${escape(input)}</textarea>
    <div class="lyrics-actions"><button class="primary" id="annotate-lyrics" ${busy || !desktop ? "disabled" : ""}>${busy ? "正在本地分析…" : "自动注音"}</button>
    <label><input id="lyrics-readings" type="checkbox" ${showReading ? "checked" : ""}> 显示振假名</label></div>
    ${error ? `<p role="alert" class="error-notice">${escape(error)}</p>` : ""}
    ${segments ? `<p role="status">注音完成。${unknown ? `${unknown} 个汉字词未找到读音，已用虚线标出并保留原文。` : "原文与换行已保留。"}${input !== annotatedInput ? " 输入已更改，请重新注音。" : ""}</p>
    <div class="lyrics-output ${showReading ? "" : "hide-readings"}" lang="ja">${segments.map((segment) =>
      segment.reading ? `<ruby>${escape(segment.text)}<rt>${escape(segment.reading)}</rt></ruby>` :
        segment.unknown ? `<span class="unknown-word" title="本地词典未找到读音">${escape(segment.text)}</span>` : escape(segment.text)).join("")}</div>` : ""}
    <p class="muted">人名、造词及歧义词可能标错。输入和结果仅在本次应用会话中保留，不上传、不自动存盘。</p>
  </section>`;
}

export function updateLyricsInput(value: string): void { input = value; }
export function toggleLyricsReadings(value: boolean): void { showReading = value; }
export async function annotateLyrics(render: () => void): Promise<void> {
  if (busy) return;
  if (!input.trim()) {
    error = "请先输入日语文本。";
    render();
    return;
  }
  if ([...input].length > 20_000) {
    error = "一次最多注音 20,000 个字符，请分段处理。";
    render();
    return;
  }
  busy = true;
  error = "";
  const text = input;
  render();
  try {
    const result = await invoke<Segment[]>("annotate_lyrics", { text });
    if (!Array.isArray(result) || !result.every((segment) =>
      typeof segment.text === "string" && typeof segment.unknown === "boolean"
      && (segment.reading === null || typeof segment.reading === "string"))
      || result.map((segment) => segment.text).join("") !== text) {
      throw new Error("注音结果与原文不一致，已停止显示。");
    }
    segments = result;
    annotatedInput = text;
  } catch (cause) {
    error = `注音失败：${String(cause)}`;
  } finally {
    busy = false;
    render();
  }
}
