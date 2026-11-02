import { spawn } from "node:child_process";
import { mkdtemp, readFile, rm } from "node:fs/promises";
import { tmpdir } from "node:os";
import { resolve, join } from "node:path";
import assert from "node:assert/strict";

const data = await mkdtemp(join(tmpdir(), "sing-japanese-smoke-"));
const port = Number(process.env.TAURI_TEST_PORT ?? 4444);
const base = `http://127.0.0.1:${port}`;
const args = ["--port", String(port), "--native-port", String(port + 1)];
if (process.env.WEBKIT_WEBDRIVER) args.push("--native-driver", resolve(process.env.WEBKIT_WEBDRIVER));
const driver = spawn(process.env.TAURI_DRIVER ?? "tauri-driver", args, {
  env: { ...process.env, XDG_DATA_HOME: data },
  stdio: ["ignore", "pipe", "pipe"],
});
let driverError;
driver.on("error", (error) => { driverError = error; });
driver.stderr.on("data", (chunk) => process.stderr.write(chunk));
driver.stdout.on("data", (chunk) => process.stdout.write(chunk));
const exited = new Promise((resolve) => driver.once("exit", resolve));
let session;
async function request(path, body, method = "POST") {
  const response = await fetch(`${base}${path}`, {
    method,
    headers: { "Content-Type": "application/json" },
    ...(body === undefined ? {} : { body: JSON.stringify(body) }),
    signal: AbortSignal.timeout(30000),
  });
  const result = await response.json();
  if (!response.ok || result.value?.error) throw new Error(JSON.stringify(result));
  return result.value;
}
async function waitFor(test, label) {
  const deadline = Date.now() + 20000;
  while (Date.now() < deadline) {
    if (driverError) throw driverError;
    if (driver.exitCode !== null) throw new Error(`tauri-driver exited: ${driver.exitCode}`);
    if (await test()) return;
    await new Promise((resolve) => setTimeout(resolve, 200));
  }
  const details = session ? await execute("return document.querySelector('#error-notice')?.textContent + ' | ' + document.querySelector('#audio-status')?.textContent") : "";
  throw new Error(`Timed out: ${label}. ${details}`);
}
const execute = (script) => request(`/session/${session}/execute/sync`, { script, args: [] });
async function click(selector) {
  const element = await request(`/session/${session}/element`, { using: "css selector", value: selector });
  const id = element["element-6066-11e4-a52e-4f735466cecf"];
  await request(`/session/${session}/element/${id}/click`, {});
}
async function open() {
  const created = await request("/session", {
    capabilities: {
      alwaysMatch: {
        "tauri:options": { application: resolve("src-tauri/target/release/sing-japanese") },
      },
    },
  });
  session = created.sessionId;
  await waitFor(async () => execute("return document.querySelectorAll('[data-kana]').length === 46"), "native chart");
}

try {
  await waitFor(async () => {
    try {
      const response = await fetch(`${base}/status`, { signal: AbortSignal.timeout(500) });
      return response.ok;
    } catch (error) {
      if (error instanceof TypeError || error.name === "TimeoutError") return false;
      throw error;
    }
  }, "WebDriver readiness");
  await open();
  assert.equal(await execute("return document.querySelector('.local-badge').textContent.includes('离线桌面版')"), true);
  assert.equal(await execute("return document.querySelector('#error-notice').hidden"), true,
    await execute("return document.querySelector('#error-notice').textContent"));
  await click('[data-kana="a"]');
  await waitFor(async () => execute("return document.querySelector('#heard-count').textContent === '1'"), "native Ogg playback and Rust persistence");
  await waitFor(async () => execute("return document.querySelector('#storage-status').textContent === '进度已保存在本机'"), "atomic save");
  const path = join(data, "com.luskyle.sing-japanese/progress.json");
  const saved = JSON.parse(await readFile(path, "utf8"));
  assert.equal(saved.cards["hiragana:a"].listens, 1);
  await request(`/session/${session}`, undefined, "DELETE");
  session = undefined;
  await open();
  assert.equal(await execute("return document.querySelector('#heard-count').textContent"), "1");
  await click('[data-view="quiz"]');
  await click("#start-quiz");
  await waitFor(async () => execute("return [...document.querySelectorAll('[data-answer]')].some(button => !button.disabled)"), "native quiz playback");
  await click("[data-answer]");
  await waitFor(async () => execute("return document.querySelector('#storage-status').textContent === '进度已保存在本机'"), "native quiz save");
  const afterQuiz = JSON.parse(await readFile(path, "utf8"));
  const attempts = Object.values(afterQuiz.cards).reduce((sum, card) => sum + card.correct + card.wrong, 0);
  assert.equal(attempts, 1);
  console.log("Native desktop verified: bundled UI, Ogg playback, Rust IPC, atomic save, restart recovery and quiz.");
} finally {
  try {
    if (session) await request(`/session/${session}`, undefined, "DELETE");
  } finally {
    if (driver.pid && driver.exitCode === null) {
      driver.kill("SIGTERM");
      await exited;
    }
    await rm(data, { recursive: true });
  }
}
