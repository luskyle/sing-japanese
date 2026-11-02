import { readFile, readdir } from "node:fs/promises";
import { createHash } from "node:crypto";
import { spawnSync } from "node:child_process";
import { fileURLToPath } from "node:url";
import { resolve } from "node:path";
import assert from "node:assert/strict";

const root = fileURLToPath(new URL("../", import.meta.url));
const manifest = JSON.parse(await readFile(resolve(root, "public/audio-manifest.json"), "utf8"));
assert.equal(manifest.recordings.length, 71);
assert.equal(new Set(manifest.recordings.map((item) => item.id)).size, 71);
assert.equal((await readdir(resolve(root, "public/audio"))).filter((file) => file.endsWith(".ogg")).length, 71);
for (const item of manifest.recordings) {
  const path = resolve(root, "public", item.file);
  const bytes = await readFile(path);
  assert.equal(bytes.subarray(0, 4).toString(), "OggS", item.id);
  assert.equal(bytes.length, item.bytes, item.id);
  assert.equal(createHash("sha256").update(bytes).digest("hex"), item.sha256, item.id);
  assert.equal(item.license, "Public domain", item.id);
  assert.ok(item.sourcePage.startsWith("https://commons.wikimedia.org/wiki/"), item.id);
  const result = spawnSync("ffprobe", ["-v", "error", "-show_entries", "format=duration", "-of", "csv=p=0", path], { encoding: "utf8" });
  if (result.error) throw result.error;
  assert.equal(result.status, 0, `${item.id}: ${result.stderr}`);
  const duration = Number(result.stdout.trim());
  assert.ok(duration > 0.1 && duration < 10, `${item.id}: duration ${duration}`);
}
console.log("Verified 71 local Ogg recordings: SHA-256, license, source, decode and duration.");
