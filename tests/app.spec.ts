import { expect, test } from "@playwright/test";

test("offline chart, real pronunciation playback and persistent script-specific progress", async ({ page }) => {
  const foreignRequests: string[] = [];
  page.on("request", (request) => {
    if (new URL(request.url()).origin !== "http://127.0.0.1:1420") foreignRequests.push(request.url());
  });
  await page.goto("/");
  await page.getByRole("button", { name: "认识假名", exact: true }).click();
  await expect(page.getByRole("heading", { name: "让每一个字，都有声音。" })).toBeVisible();
  await expect(page.locator("[data-kana]")).toHaveCount(46);
  await page.getByRole("button", { name: "学习 あ", exact: true }).click();
  await expect(page.locator("#heard-count")).toHaveText("1", { timeout: 15000 });
  await expect(page.locator("#audio-status")).toContainText("播放完成");
  await page.reload();
  await page.getByRole("button", { name: "认识假名", exact: true }).click();
  await expect(page.locator("#heard-count")).toHaveText("1");
  await page.getByRole("button", { name: "ア 片假名", exact: true }).click();
  await expect(page.getByRole("button", { name: "学习 ア", exact: true })).toBeVisible();
  await expect(page.locator("#heard-count")).toHaveText("0");
  await page.getByLabel("罗马音提示").uncheck();
  await expect(page.locator(".kana-cell small")).toHaveCount(0);
  await page.getByLabel("发音分组").selectOption("semi");
  await expect(page.locator("[data-kana]")).toHaveCount(5);
  expect(foreignRequests).toEqual([]);
});

test("all bundled recordings are decoded by the actual browser audio engine", async ({ page }) => {
  await page.goto("/");
  const result = await page.evaluate(async () => {
    const manifest = await (await fetch("/audio-manifest.json")).json();
    const context = new AudioContext();
    const durations: number[] = [];
    for (const entry of manifest.recordings) {
      const response = await fetch(`/${entry.file}`);
      if (!response.ok) throw new Error(`Missing ${entry.id}`);
      const buffer = await context.decodeAudioData(await response.arrayBuffer());
      durations.push(buffer.duration);
    }
    await context.close();
    return durations;
  });
  expect(result).toHaveLength(71);
  expect(result.every((duration) => duration > 0.1 && duration < 10)).toBe(true);
});

test("quiz hides the target, records first attempts and restores progress after reload", async ({ page }) => {
  let audioId = "";
  page.on("request", (request) => {
    const match = /\/audio\/([a-z]+)\.ogg/.exec(request.url());
    if (match) audioId = match[1];
  });
  await page.goto("/");
  await page.getByRole("button", { name: "听辨练习", exact: true }).click();
  await page.getByLabel("练习行", { exact: true }).selectOption("元音");
  await page.getByRole("button", { name: "开始听辨练习", exact: true }).click();
  await expect(page.locator("[data-answer]").first()).toBeEnabled();
  await expect(page.locator(".choices small")).toHaveCount(0);
  expect(audioId).toBeTruthy();
  await page.locator(`[data-answer="${audioId}"]`).click();
  await expect(page.locator(".answer-feedback")).toContainText("答对了");
  await expect(page.locator("#accuracy-count")).toHaveText("100%");
  await expect(page.locator("#storage-status")).toHaveText("进度已保存在本机");
  await page.getByRole("button", { name: "下一题", exact: false }).click();
  await expect(page.locator("[data-answer]").first()).toBeEnabled();
  await page.locator(`[data-answer]:not([data-answer="${audioId}"])`).first().click();
  await expect(page.locator(".answer-feedback")).toContainText("再记一次");
  await expect(page.locator("#accuracy-count")).toHaveText("50%");
  await expect(page.locator("#storage-status")).toHaveText("进度已保存在本机");
  await page.reload();
  await page.getByRole("button", { name: "认识假名", exact: true }).click();
  await expect(page.locator("#accuracy-count")).toHaveText("50%");
});

test("unusable quiz pools are explained instead of producing invalid questions", async ({ page }) => {
  await page.goto("/");
  await page.getByRole("button", { name: "听辨练习", exact: true }).click();
  await page.getByLabel("练习行", { exact: true }).selectOption("拨音");
  await expect(page.getByRole("button", { name: "开始听辨练习", exact: true })).toBeDisabled();
  await expect(page.locator(".warning")).toContainText("至少选择两个不同发音");
});

test("corrupt progress is reported and never overwritten by an empty record", async ({ page }) => {
  await page.goto("/");
  await page.evaluate(() => localStorage.setItem("sing-japanese:progress:v1", "broken"));
  await page.reload();
  await expect(page.getByRole("alert")).toContainText("为保护原记录");
  await expect(page.getByRole("button", { name: "认识假名", exact: true })).toBeDisabled();
  expect(await page.evaluate(() => localStorage.getItem("sing-japanese:progress:v1"))).toBe("broken");
  await page.evaluate(() => localStorage.removeItem("sing-japanese:progress:v1"));
  await page.getByRole("button", { name: "重试读取", exact: true }).click();
  await expect(page.getByRole("alert")).toBeHidden();
  await expect(page.getByRole("button", { name: "认识假名", exact: true })).toBeEnabled();
});

test("missing audio is explicit and prevents guessing until playback recovers", async ({ page }) => {
  await page.route("**/audio/*.ogg", (route) => route.abort());
  await page.goto("/");
  await page.getByRole("button", { name: "听辨练习", exact: true }).click();
  await page.getByRole("button", { name: "开始听辨练习", exact: true }).click();
  await expect(page.getByRole("alert")).toContainText("录音加载失败");
  await expect(page.getByRole("alert")).toContainText("本地服务仍在运行");
  await expect(page.locator("[data-answer]").first()).toBeDisabled();
  await page.unroute("**/audio/*.ogg");
  await page.getByRole("button", { name: "重听题目", exact: true }).click();
  await expect(page.locator("[data-answer]").first()).toBeEnabled();
  await expect(page.getByRole("alert")).toBeHidden();
});

test("resource attribution and narrow layouts remain usable", async ({ page }) => {
  await page.goto("/");
  await page.getByRole("button", { name: "本地资源", exact: true }).click();
  await expect(page.getByRole("heading", { name: "声音留在本地，学习也一样" })).toBeVisible();
  await page.locator("summary").first().click();
  await expect(page.locator(".resource-list li")).toHaveCount(71);
  await expect(page.locator(".resource-list").first()).toContainText("Hakatanoshio117117");
  await page.setViewportSize({ width: 820, height: 650 });
  await page.getByRole("button", { name: "返回首页", exact: false }).click();
  await page.getByRole("button", { name: "认识假名", exact: true }).click();
  expect(await page.evaluate(() => document.documentElement.scrollWidth <= innerWidth)).toBe(true);
  await page.setViewportSize({ width: 390, height: 844 });
  expect(await page.evaluate(() => document.documentElement.scrollWidth <= innerWidth)).toBe(true);
});

test("home cards guide learning before lyrics without sidebar tabs and retain lyric input", async ({ page }) => {
  await page.goto("/");
  await expect(page.getByRole("heading", { name: "先认识假名，再读懂歌词。" })).toBeVisible();
  await expect(page.locator(".sidebar, [role=tab], nav")).toHaveCount(0);
  const cards = page.locator(".home-path article");
  await expect(cards).toHaveCount(2);
  await expect(cards.nth(0)).toContainText("先认识假名");
  await expect(cards.nth(1)).toContainText("核心功能");
  await page.getByRole("button", { name: "歌词注音", exact: true }).click();
  await page.getByLabel("日语文本（最多 20,000 字符）").fill("学校へ行く");
  await expect(page.locator("#annotate-lyrics")).toBeDisabled();
  await page.getByRole("button", { name: "返回首页", exact: false }).click();
  await page.getByRole("button", { name: "歌词注音", exact: true }).click();
  await expect(page.locator("#lyrics-input")).toHaveValue("学校へ行く");
  await page.setViewportSize({ width: 390, height: 844 });
  await page.getByRole("button", { name: "返回首页", exact: false }).click();
  expect(await page.evaluate(() => document.documentElement.scrollWidth <= innerWidth)).toBe(true);
});
