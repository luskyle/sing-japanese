import { readFile } from "node:fs/promises";
import assert from "node:assert/strict";

const pkg = JSON.parse(await readFile(new URL("../package.json", import.meta.url), "utf8"));
const config = JSON.parse(await readFile(new URL("../src-tauri/tauri.conf.json", import.meta.url), "utf8"));
const cargo = await readFile(new URL("../src-tauri/Cargo.toml", import.meta.url), "utf8");
const cargoVersion = /^\[package\][\s\S]*?^version = "([^"]+)"/m.exec(cargo)?.[1];
const cargoLock = await readFile(new URL("../src-tauri/Cargo.lock", import.meta.url), "utf8");
const cargoLockVersion = /\[\[package\]\]\nname = "sing-japanese"\nversion = "([^"]+)"/.exec(cargoLock)?.[1];
const lock = JSON.parse(await readFile(new URL("../package-lock.json", import.meta.url), "utf8"));
assert.match(pkg.version, /^\d+\.\d+\.\d+(?:-[0-9A-Za-z]+(?:[.-][0-9A-Za-z]+)*)?$/);
assert.equal(config.version, pkg.version, "Tauri version must match package.json");
assert.equal(cargoVersion, pkg.version, "Cargo version must match package.json");
assert.equal(cargoLockVersion, pkg.version, "Cargo lockfile version must match package.json");
assert.equal(lock.version, pkg.version, "npm lockfile version must match package.json");
assert.equal(lock.packages[""].version, pkg.version, "npm lockfile root version must match package.json");
assert.equal(process.env.GITHUB_REF, `refs/tags/v${pkg.version}`, "Release must run from the exact version tag, not a branch");
console.log(`Validated release v${pkg.version}`);
