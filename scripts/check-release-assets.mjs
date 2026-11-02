import { mkdir, readFile, readdir, writeFile } from "node:fs/promises";
import { createHash } from "node:crypto";
import { resolve, join, basename } from "node:path";
import assert from "node:assert/strict";

const input = resolve(process.argv[2] ?? "downloaded-artifacts");
const directory = resolve(process.argv[3] ?? "release-assets");
async function collect(path) {
  const entries = await readdir(path, { withFileTypes: true });
  const nested = await Promise.all(entries.map((entry) =>
    entry.isDirectory() ? collect(join(path, entry.name)) : [join(path, entry.name)]));
  return nested.flat();
}
const files = (await collect(input)).sort();
const extensions = [".deb", ".AppImage", ".exe", ".dmg"];
assert.equal(files.length, extensions.length, "Expected exactly four platform installers");
for (const extension of extensions) {
  assert.equal(files.filter((file) => file.endsWith(extension)).length, 1, `Missing or duplicate installer: ${extension}`);
}
const checksums = [];
await mkdir(directory, { recursive: true });
assert.equal((await readdir(directory)).length, 0, "Output directory must be empty");
for (const path of files) {
  const file = basename(path);
  assert.ok(!/[\r\n]/.test(file), "Invalid asset filename");
  const contents = await readFile(path);
  assert.ok(contents.length > 1024, `Installer is unexpectedly small: ${file}`);
  checksums.push(`${createHash("sha256").update(contents).digest("hex")}  ${file}`);
  await writeFile(join(directory, file), contents, { flag: "wx" });
}
await writeFile(join(directory, "SHA256SUMS.txt"), `${checksums.join("\n")}\n`);
console.log(`Validated ${files.length} installers and wrote SHA256SUMS.txt`);
