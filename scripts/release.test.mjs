import { test } from "node:test";
import assert from "node:assert/strict";
import { mkdtemp, mkdir, writeFile, readFile, rm } from "node:fs/promises";
import { execFileSync, spawnSync } from "node:child_process";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { fileURLToPath } from "node:url";
import { createHash } from "node:crypto";

const root = fileURLToPath(new URL("../", import.meta.url));

test("release accepts the exact version tag and rejects branch or mismatched tags", async () => {
  const pkg = JSON.parse(await readFile(join(root, "package.json"), "utf8"));
  for (const [ref, expected] of [[`refs/tags/v${pkg.version}`, 0], ["refs/heads/main", 1], ["refs/tags/v99.0.0", 1]]) {
    const result = spawnSync(process.execPath, ["scripts/check-release.mjs"], {
      cwd: root, env: { ...process.env, GITHUB_REF: ref }, encoding: "utf8",
    });
    assert.equal(result.status, expected, result.stderr);
  }
});

test("nested platform artifacts are flattened, checked and hashed; missing assets fail", async () => {
  const temp = await mkdtemp(join(tmpdir(), "kana-release-"));
  try {
    const input = join(temp, "input");
    const output = join(temp, "output");
    const content = Buffer.alloc(2048, 42);
    for (const extension of ["deb", "AppImage", "exe", "dmg"]) {
      await mkdir(join(input, extension), { recursive: true });
      await writeFile(join(input, extension, `Sing Japanese.${extension}`), content);
    }
    execFileSync(process.execPath, ["scripts/check-release-assets.mjs", input, output], { cwd: root });
    const checksums = await readFile(join(output, "SHA256SUMS.txt"), "utf8");
    assert.equal(checksums.trim().split("\n").length, 4);
    assert.ok(checksums.includes(`${createHash("sha256").update(content).digest("hex")}  Sing.Japanese.dmg`));
    assert.deepEqual(await readFile(join(output, "Sing.Japanese.exe")), content);
    await rm(join(input, "dmg", "Sing Japanese.dmg"));
    const result = spawnSync(process.execPath, ["scripts/check-release-assets.mjs", input, join(temp, "incomplete")], { cwd: root });
    assert.equal(result.status, 1);
    assert.match(result.stderr.toString(), /Expected exactly four/);
  } finally {
    await rm(temp, { recursive: true });
  }
});
