import { expect, test } from "@playwright/test";

test("offline chart, real pronunciation playback and persistent script-specific progress", async ({ page }) => {
  const foreignRequests: string[] = [];
  page.on("request", (request) => {
    if (new URL(request.url()).origin !== "http://127.0.0.1:1420") foreignRequests.push(request.url());
  });
  await page.goto("/");
  await page.getByRole("button", { name: "认识假名", exact: true }).click();
  await expect(page.getByRole("heading", { name: "认识假名", exact: true })).toBeVisible();
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
  await expect(page.getByRole("heading", { name: "本地资源", exact: true })).toBeVisible();
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
  await expect(page.getByRole("heading", { name: "学习与工具", exact: true })).toBeVisible();
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

test("home fits the viewport without scrolling or clipping its content", async ({ page }) => {
  await page.goto("/");
  await expect(page.getByRole("button", { name: "认识假名", exact: true })).toBeEnabled();
  for (const viewport of [
    { width: 1200, height: 850 },
    { width: 820, height: 600 },
    { width: 820, height: 650 },
    { width: 1440, height: 900 },
    { width: 1920, height: 1080 },
    { width: 390, height: 844 },
    { width: 360, height: 740 },
  ]) {
    await page.setViewportSize(viewport);
    await page.evaluate(() => document.fonts.ready);
    const layout = await page.evaluate(() => {
      const elements = [...document.querySelectorAll<HTMLElement>(".home-screen button, .home-screen h1, .feature-card h2, .feature-card p, .feature-art, .companion-art, .companion-progress, .home-dock")];
      return {
        width: document.documentElement.scrollWidth,
        height: document.documentElement.scrollHeight,
        clipped: elements.filter((element) => {
          const box = element.getBoundingClientRect();
          return box.top < 0 || box.left < 0 || box.bottom > innerHeight || box.right > innerWidth
            || element.scrollWidth > element.clientWidth + 1 || element.scrollHeight > element.clientHeight + 1;
        }).map((element) => element.className || element.tagName),
        overflow: getComputedStyle(document.querySelector(".home-screen")!).overflow,
      };
    });
    expect(layout, JSON.stringify(viewport)).toMatchObject({
      width: viewport.width, height: viewport.height, clipped: [], overflow: "visible",
    });
    for (const name of ["认识假名", "听辨练习", "歌词注音", "学习指南", "本地资源"]) {
      await expect(page.getByRole("button", { name, exact: true })).toBeInViewport({ ratio: 1 });
    }
  }
});

test("all tools share the pastel workspace and keep useful controls accessible on narrow screens", async ({ page }) => {
  await page.goto("/");
  for (const name of ["认识假名", "听辨练习", "歌词注音", "学习指南", "本地资源"]) {
    await page.getByRole("button", { name, exact: true }).click();
    await expect(page.locator("h1")).toHaveText(name);
    await expect(page.locator(".chapter, .title-stamp, footer")).toHaveCount(0);
    await expect(page.locator("html")).toHaveCSS("color-scheme", "light");
    await expect(page.locator(".panel").first()).toHaveCSS("border-radius", "20px");
    for (const viewport of [
      { width: 1200, height: 850 }, { width: 820, height: 650 }, { width: 390, height: 844 },
    ]) {
      await page.setViewportSize(viewport);
      expect(await page.evaluate(() => document.documentElement.scrollWidth <= innerWidth), `${name}: ${viewport.width}`).toBe(true);
      await expect(page.getByRole("button", { name: "返回首页", exact: false })).toBeVisible();
      if (viewport.width === 1200) {
        expect(await page.evaluate(() => document.documentElement.scrollHeight <= innerHeight), `${name}: default window`).toBe(true);
      }
    }
    if (name === "歌词注音") {
      await expect(page.getByRole("region", { name: "歌词原文", exact: true })).toBeVisible();
      await expect(page.getByRole("region", { name: "注音结果", exact: true })).toBeVisible();
      await expect(page.getByLabel("显示振假名")).toBeChecked();
      await page.getByLabel("日语文本（最多 20,000 字符）").fill("学校へ行く\nあいうえお");
      await page.getByLabel("显示振假名").uncheck();
      await expect(page.getByLabel("显示振假名")).not.toBeChecked();
      await expect(page.locator("#lyrics-input")).toHaveValue("学校へ行く\nあいうえお");
      await expect(page.getByText("请使用桌面版注音；浏览器仅供界面预览。", { exact: true })).toBeVisible();
    }
    if (name === "本地资源") {
      await page.getByText("IPADIC 词典许可声明", { exact: true }).click();
      await expect(page.locator("#dictionary-license")).toContainText("Lindera Morphological Analyzer");
    }
    await page.getByRole("button", { name: "返回首页", exact: false }).click();
  }
});

test("home illustrations load locally and each colorful card opens its tool", async ({ page }) => {
  const foreignRequests: string[] = [];
  page.on("request", (request) => {
    if (new URL(request.url()).origin !== "http://127.0.0.1:1420") foreignRequests.push(request.url());
  });
  await page.goto("/");
  await expect(page.locator(".companion-art, .feature-art")).toHaveCount(5);
  await expect.poll(() => page.locator(".companion-art, .feature-art").evaluateAll((images) =>
    images.every((image) => image instanceof HTMLImageElement && image.complete && image.naturalWidth > 0))).toBe(true);
  for (const [card, name] of [
    [".kana-entry", "认识假名"], [".lyrics-entry", "歌词注音"],
    [".quiz-entry", "听辨练习"], [".guide-entry", "学习指南"],
  ]) {
    await page.locator(`${card} button`).click();
    await expect(page.locator("h1")).toHaveText(name);
    await page.getByRole("button", { name: "返回首页", exact: false }).click();
  }
  expect(foreignRequests).toEqual([]);
});
