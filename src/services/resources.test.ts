import { afterEach, expect, it, vi } from "vitest";
import manifest from "../../public/audio-manifest.json";
import { loadDictionaryLicense, loadResources } from "./resources";

afterEach(() => vi.unstubAllGlobals());

it("loads the bundled catalog without changing counts, dates or recording metadata", async () => {
  vi.stubGlobal("fetch", vi.fn().mockResolvedValue(new Response(JSON.stringify(manifest))));
  const catalog = await loadResources();
  expect(catalog.count).toBe(71);
  expect(catalog.bytes).toBe(manifest.recordings.reduce((sum, item) => sum + item.bytes, 0));
  expect(catalog.date).toBe(manifest.retrievedAt.slice(0, 10));
  expect(catalog.recordings).toEqual(manifest.recordings);
});

it("rejects incomplete or malformed catalog entries explicitly", async () => {
  for (const data of [
    { ...manifest, recordings: [] },
    { ...manifest, recordings: manifest.recordings.map((item) => ({ ...item, bytes: -1 })) },
    { ...manifest, retrievedAt: "invalid" },
  ]) {
    vi.stubGlobal("fetch", vi.fn().mockResolvedValue(new Response(JSON.stringify(data))));
    await expect(loadResources()).rejects.toThrow("本地资源清单不完整");
  }
});

it("surfaces missing resources and returns the complete dictionary notice unchanged", async () => {
  vi.stubGlobal("fetch", vi.fn().mockResolvedValue(new Response("", { status: 404 })));
  await expect(loadResources()).rejects.toThrow("HTTP 404");
  await expect(loadDictionaryLicense()).rejects.toThrow("HTTP 404");
  vi.stubGlobal("fetch", vi.fn().mockResolvedValue(new Response("Original license\nAll rights reserved.\n")));
  expect(await loadDictionaryLicense()).toBe("Original license\nAll rights reserved.\n");
});
