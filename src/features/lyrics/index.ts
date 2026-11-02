import { invoke } from "@tauri-apps/api/core";
import { desktop } from "../../services/storage";
import { escapeHtml as escape } from "../../shared/html";

interface Segment { text: string; reading: string | null; unknown: boolean }
let input = "";
let segments: Segment[] | null = null;
let annotatedInput = "";
let busy = false;
let error = "";
let showReading = true;

export function lyricsView(): string {
  const unknown = segments?.filter((segment) => segment.unknown).length ?? 0;
  return `<div class="lyrics-layout">
    <section class="panel lyrics-editor" aria-label="歌词原文">
    <div class="panel-heading"><label for="lyrics-input">歌词原文</label><span class="input-limit">最多 20,000 字符</span></div>
    ${!desktop ? '<p class="notice">请使用桌面版注音；浏览器仅供界面预览。</p>' : ""}
    <textarea id="lyrics-input" class="lyrics-input" lang="ja" aria-label="日语文本（最多 20,000 字符）" placeholder="粘贴日语歌词…" ${busy ? "disabled" : ""}>${escape(input)}</textarea>
    <div class="lyrics-actions"><button class="primary" id="annotate-lyrics" ${busy || !desktop ? "disabled" : ""}>${busy ? "正在本地分析…" : "自动注音"}</button>
    </div>
    ${error ? `<p role="alert" class="error-notice">${escape(error)}</p>` : ""}
    <p class="muted">常规词典读音，人名及特殊唱法可能不同。歌词仅本次会话保留。</p>
    </section>
    <section class="panel lyrics-result" aria-label="注音结果">
    <div class="panel-heading"><h2>注音结果</h2><label class="toggle"><input id="lyrics-readings" type="checkbox" ${showReading ? "checked" : ""}> 显示振假名</label></div>
    ${segments ? `<p class="result-status" role="status">${unknown ? `${unknown} 个汉字词未找到读音，已用虚线标出。` : "注音完成"}${input !== annotatedInput ? " · 输入已更改，请重新注音。" : ""}</p>
    <div class="lyrics-output ${showReading ? "" : "hide-readings"}" lang="ja">${segments.map((segment) =>
      segment.reading ? `<ruby>${escape(segment.text)}<rt>${escape(segment.reading)}</rt></ruby>` :
        segment.unknown ? `<span class="unknown-word" title="本地词典未找到读音">${escape(segment.text)}</span>` : escape(segment.text)).join("")}</div>` : '<div class="lyrics-empty">注音结果将显示在这里</div>'}
    </section>
  </div>`;
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
