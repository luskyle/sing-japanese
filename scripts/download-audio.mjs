import { mkdir, readFile, writeFile, rename } from "node:fs/promises";
import { createHash } from "node:crypto";
import { fileURLToPath } from "node:url";
import { resolve } from "node:path";
import { execFile } from "node:child_process";
import { promisify } from "node:util";
import ts from "typescript";

const root = fileURLToPath(new URL("../", import.meta.url));
const source = await readFile(resolve(root, "src/features/kana/data.ts"), "utf8");
const { outputText } = ts.transpileModule(source, {
  compilerOptions: { module: ts.ModuleKind.ES2022, target: ts.ScriptTarget.ES2022 },
});
const { kana } = await import(`data:text/javascript;base64,${Buffer.from(outputText).toString("base64")}`);
const output = resolve(root, "public/audio");
await mkdir(output, { recursive: true });

const exec = promisify(execFile);
async function get(url) {
  const { stdout } = await exec("curl", [
    "--fail", "--location", "--silent", "--show-error", "--retry", "2", "--max-time", "90",
    "--user-agent", "SingJapanese/0.1 (https://github.com/luskyle/sing-japanese)",
    url.toString(),
  ], {
    encoding: "buffer", maxBuffer: 10 * 1024 * 1024,
  });
  return stdout;
}

const clean = (text) => text.replace(/<[^>]*>/g, "").trim();
const manifest = { version: 1, retrievedAt: new Date().toISOString(), recordings: [] };
for (let offset = 0; offset < kana.length; offset += 20) {
  const batch = kana.slice(offset, offset + 20);
  const api = new URL("https://commons.wikimedia.org/w/api.php");
  api.search = new URLSearchParams({
    action: "query", titles: batch.map((item) => `File:${item.source}`).join("|"),
    prop: "imageinfo", iiprop: "url|extmetadata", format: "json",
  }).toString();
  const metadata = JSON.parse((await get(api)).toString("utf8"));
  if (metadata.error) throw new Error(JSON.stringify(metadata.error));
  const pages = Object.values(metadata.query.pages);
  for (const item of batch) {
    const page = pages.find((page) => page.title === `File:${item.source}`);
    const info = page?.imageinfo?.[0];
    if (!info) throw new Error(`No recording metadata for ${item.source}`);
    const license = info.extmetadata.LicenseShortName?.value;
    if (license !== "Public domain") throw new Error(`Unapproved license for ${item.id}: ${license}`);
    const audio = await get(info.url);
    if (audio.subarray(0, 4).toString() !== "OggS") throw new Error(`Invalid Ogg file: ${item.id}`);
    const filename = `${item.id}.ogg`;
    await writeFile(resolve(output, `${filename}.tmp`), audio);
    await rename(resolve(output, `${filename}.tmp`), resolve(output, filename));
    manifest.recordings.push({
      id: item.id, hiragana: item.hiragana, katakana: item.katakana,
      file: `audio/${filename}`, sourceTitle: page.title,
      sourcePage: info.descriptionurl, downloadUrl: info.url,
      author: clean(info.extmetadata.Artist.value),
      license, licenseUrl: info.extmetadata.LicenseUrl?.value ?? "https://commons.wikimedia.org/wiki/Template:PD-self",
      description: clean(info.extmetadata.ImageDescription?.value ?? ""),
      modified: false, bytes: audio.length,
      sha256: createHash("sha256").update(audio).digest("hex"),
    });
    console.log(`${manifest.recordings.length}/${kana.length} ${item.hiragana} ${filename}`);
    await new Promise((resolve) => setTimeout(resolve, 200));
  }
}
await writeFile(resolve(root, "public/audio-manifest.json"), `${JSON.stringify(manifest, null, 2)}\n`);
console.log("Downloaded audio and provenance manifest.");
