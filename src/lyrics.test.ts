import { beforeEach, expect, it, vi } from "vitest";

vi.mock("./storage", () => ({ desktop: true }));
vi.mock("@tauri-apps/api/core", () => ({ invoke: vi.fn() }));
import { invoke } from "@tauri-apps/api/core";

beforeEach(() => {
  vi.resetModules();
  vi.mocked(invoke).mockReset();
});

it("renders safe ruby and keeps original whitespace without adding annotations to kana", async () => {
  const lyrics = await import("./lyrics");
  lyrics.updateLyricsInput("学校\n <b>あ");
  vi.mocked(invoke).mockResolvedValue([
    { text: "学校", reading: "がっこう", unknown: false },
    { text: "\n <b>あ", reading: null, unknown: false },
  ]);
  await lyrics.annotateLyrics(vi.fn());
  expect(lyrics.lyricsView()).toContain("<ruby>学校<rt>がっこう</rt></ruby>");
  expect(lyrics.lyricsView()).toContain("\n &lt;b&gt;あ");
  lyrics.toggleLyricsReadings(false);
  expect(lyrics.lyricsView()).toContain("hide-readings");
});

it("rejects changed source text, surfaces failures and validates empty input", async () => {
  const lyrics = await import("./lyrics");
  const render = vi.fn();
  await lyrics.annotateLyrics(render);
  expect(invoke).not.toHaveBeenCalled();
  expect(lyrics.lyricsView()).toContain("请先输入");
  lyrics.updateLyricsInput("学校");
  vi.mocked(invoke).mockResolvedValue([{ text: "学", reading: "がく", unknown: false }]);
  await lyrics.annotateLyrics(render);
  expect(lyrics.lyricsView()).toContain("与原文不一致");
  vi.mocked(invoke).mockRejectedValue("词典错误");
  await lyrics.annotateLyrics(render);
  expect(lyrics.lyricsView()).toContain("词典错误");
});
