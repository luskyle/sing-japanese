import "./style.css";
import { kana, groupNames, pronunciationNote, type Group, type Kana, type Script } from "./kana";
import { blankCard, cardKey, emptyProgress, makeQuestion, record, summarize, type Progress, type Question } from "./learning";
import { desktop, loadProgress, saveProgress } from "./storage";
import { PronunciationPlayer } from "./audio";

type View = "learn" | "quiz" | "guide" | "resources";
const root = document.querySelector<HTMLDivElement>("#app");
if (!root) throw new Error("缺少应用挂载节点");
const app = root;
const player = new PronunciationPlayer();
let progress = emptyProgress();
let script: Script = "hiragana";
let group: Group = "basic";
let row = "all";
let view: View = "learn";
let selected = kana[0];
let showRomaji = true;
let rate = 1;
let question: Question | null = null;
let answer: string | null = null;
let audioReady = false;
let audioStatus = "";
const errors = { audio: "", storage: "", resources: "" };
let loadFailed = false;
let storageFailed = false;
let pendingSaves = 0;
let saveQueue = Promise.resolve();
let roundCorrect = 0;
let roundTotal = 0;
let resources: { count: number; bytes: number; date: string } | null = null;

const escape = (text: string): string => text.replace(/[&<>"']/g, (char) =>
  ({ "&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;", "'": "&#39;" })[char] ?? char);
const glyph = (item: Kana): string => item[script];
const pool = (): Kana[] => kana.filter((item) => item.group === group && (row === "all" || item.row === row));
const scriptName = (): string => script === "hiragana" ? "平假名" : "片假名";

function save(snapshot: Progress): void {
  pendingSaves++;
  updateStatus();
  saveQueue = saveQueue.then(async () => {
    try {
      await saveProgress(snapshot);
      storageFailed = false;
      reportError("", "storage");
    } catch (error) {
      storageFailed = true;
      reportError(`学习记录保存失败：${String(error)}。本次进度暂留内存，请点击“重试保存”，成功前不要关闭应用。`, "storage");
    } finally {
      pendingSaves--;
      updateStatus();
    }
  });
}

function updateStatus(): void {
  const status = document.querySelector<HTMLElement>("#storage-status");
  if (status) {
    status.textContent = loadFailed ? "记录读取失败" : storageFailed ? "保存失败" : pendingSaves ? "正在保存…" : "进度已保存在本机";
  }
}

function updateStats(): void {
  const stats = summarize(progress, script);
  const heard = document.querySelector("#heard-count");
  const familiar = document.querySelector("#familiar-count");
  const accuracy = document.querySelector("#accuracy-count");
  if (heard) heard.textContent = String(stats.heard);
  if (familiar) familiar.textContent = String(stats.familiar);
  if (accuracy) accuracy.textContent = stats.accuracy === null ? "—" : `${stats.accuracy}%`;
  const counts = document.querySelector(".card-counts");
  const card = progress.cards[cardKey(script, selected.id)] ?? blankCard();
  if (counts) counts.textContent = `完整听过 ${card.listens} 次 · 答对 ${card.correct} 次 · 答错 ${card.wrong} 次`;
  updateStatus();
}

function reportError(message: string, kind: keyof typeof errors = "audio"): void {
  errors[kind] = message;
  const errorMessage = Object.values(errors).filter(Boolean).join(" ");
  const notice = document.querySelector<HTMLElement>("#error-notice");
  if (!notice) return;
  notice.hidden = !errorMessage;
  const text = notice.querySelector("span");
  if (text) text.textContent = message;
  const retry = notice.querySelector<HTMLButtonElement>("#retry-storage");
  if (retry) retry.hidden = !(loadFailed || storageFailed);
}

function resetAudio(): void {
  player.stop();
  audioReady = false;
  audioStatus = "";
}

function resetQuestion(): void {
  resetAudio();
  question = null;
  answer = null;
  roundCorrect = 0;
  roundTotal = 0;
}

function mark(item: Kana, event: "listen" | "correct" | "wrong"): void {
  if (loadFailed) return;
  progress = record(progress, script, item.id, event);
  save(progress);
  updateStats();
}

async function play(item: Kana, quiz: boolean): Promise<void> {
  if (loadFailed) return;
  const activeScript = script;
  audioStatus = "正在播放本地录音…";
  const label = document.querySelector("#audio-status");
  if (label) label.textContent = audioStatus;
  const ready = await player.play(item.id, rate, () => {
    if (activeScript !== script) return;
    mark(item, "listen");
    audioStatus = "播放完成，可以再听一遍。";
    const status = document.querySelector("#audio-status");
    if (status) status.textContent = audioStatus;
  }, (message) => {
    audioReady = false;
    audioStatus = "播放未完成，请重试。";
    const status = document.querySelector("#audio-status");
    if (status) status.textContent = audioStatus;
    reportError(message);
    if (quiz) document.querySelectorAll<HTMLButtonElement>("[data-answer]").forEach((button) => { button.disabled = true; });
  });
  if (ready) reportError("", "audio");
  if (ready && quiz && question?.target.id === item.id && view === "quiz") {
    audioReady = true;
    document.querySelectorAll<HTMLButtonElement>("[data-answer]").forEach((button) => { button.disabled = answer !== null; });
  }
}

function filters(): string {
  const rows = [...new Set(kana.filter((item) => item.group === group).map((item) => item.row))];
  return `
    <div class="filters">
      <div class="segmented" aria-label="假名类型">
        <button data-script="hiragana" aria-pressed="${script === "hiragana"}">あ 平假名</button>
        <button data-script="katakana" aria-pressed="${script === "katakana"}">ア 片假名</button>
      </div>
      <label class="select-label">范围
        <select id="group" aria-label="发音分组">${(Object.keys(groupNames) as Group[]).map((key) =>
          `<option value="${key}" ${group === key ? "selected" : ""}>${groupNames[key]}</option>`).join("")}</select>
      </label>
      <label class="select-label">练习行
        <select id="row" aria-label="练习行"><option value="all">全部行</option>${rows.map((name) =>
          `<option ${row === name ? "selected" : ""}>${name}</option>`).join("")}</select>
      </label>
    </div>`;
}

function learn(): string {
  const items = pool();
  const rows = [...new Set(items.map((item) => item.row))];
  const card = progress.cards[cardKey(script, selected.id)] ?? blankCard();
  return `
    ${filters()}
    <div class="learning-layout">
      <section class="panel chart-panel" aria-label="假名表">
        <div class="panel-heading"><h2>从一个声音开始</h2><label class="toggle"><input id="romaji" type="checkbox" ${showRomaji ? "checked" : ""}> 罗马音提示</label></div>
        <div class="chart-head"><span></span><span>a</span><span>i</span><span>u</span><span>e</span><span>o</span></div>
        ${rows.map((name) => `<div class="kana-row"><span class="row-label">${name}</span>${[0, 1, 2, 3, 4].map((column) => {
          const item = items.find((item) => item.row === name && item.column === column);
          if (!item) return '<span class="empty-cell" aria-hidden="true">·</span>';
          const stats = progress.cards[cardKey(script, item.id)] ?? blankCard();
          return `<button class="kana-cell ${selected.id === item.id ? "selected" : ""} ${stats.streak >= 3 ? "familiar" : ""}" data-kana="${item.id}" aria-label="学习 ${glyph(item)}" aria-pressed="${selected.id === item.id}">
            <span lang="ja">${glyph(item)}</span>${showRomaji ? `<small>${item.romaji}</small>` : ""}
            ${stats.streak >= 3 ? '<i class="known-dot" title="连续答对至少 3 次"></i>' : ""}
          </button>`;
        }).join("")}</div>`).join("")}
        <p class="chart-footnote">点击假名听真人发音 · 绿色圆点表示该字连续答对至少 3 次，并非长期掌握证明</p>
      </section>
      <aside class="panel sound-card">
        <div class="eyebrow">LISTEN & REPEAT</div>
        <div class="big-kana" lang="ja">${glyph(selected)}</div>
        <div class="counterpart" lang="ja">${script === "hiragana" ? "片假名" : "平假名"} <strong>${script === "hiragana" ? selected.katakana : selected.hiragana}</strong></div>
        ${showRomaji ? `<div class="romaji-large">${selected.romaji}</div>` : '<div class="romaji-large muted">试着自己回想</div>'}
        <button class="primary wide" id="replay">▶ 播放发音</button>
        ${speedControl()}
        <p id="audio-status" class="audio-status" role="status">${audioStatus || "音频已随应用保存，无需联网"}</p>
        <div class="tip"><span>发音小提示</span><p>${pronunciationNote(selected)}</p></div>
        <div class="card-counts">完整听过 ${card.listens} 次 · 答对 ${card.correct} 次 · 答错 ${card.wrong} 次</div>
        <button class="text-button wide" id="go-quiz">去听辨练习 →</button>
      </aside>
    </div>`;
}

function speedControl(): string {
  return `<label class="speed-label">播放速度 <select id="rate" aria-label="播放速度">
    <option value="1" ${rate === 1 ? "selected" : ""}>原速 1×</option>
    <option value="0.8" ${rate === 0.8 ? "selected" : ""}>慢速 0.8×</option>
  </select></label>`;
}

function quiz(): string {
  const distinct = new Set(pool().map((item) => item.sound)).size;
  return `${filters()}
    <section class="panel quiz-panel">
      <div class="quiz-top"><span class="eyebrow">LISTEN FIRST</span><span>本轮 ${roundCorrect} / ${roundTotal} 答对</span></div>
      <h2>听到的是哪一个${scriptName()}？</h2>
      <p class="muted">先听声音，再认字形。不显示罗马音，也不把同音字放在一起考。</p>
      ${question ? `
        <button class="listen-orb" id="replay" aria-label="重听题目">▶</button>
        ${speedControl()}
        <p id="audio-status" class="audio-status" role="status">${audioStatus || "点击播放，再选择答案"}</p>
        <div class="choices">${question.choices.map((item) => {
          const state = answer !== null ? item.id === question?.target.id ? "correct" : item.id === answer ? "wrong" : "" : "";
          return `<button class="choice ${state}" data-answer="${item.id}" ${answer !== null || !audioReady ? "disabled" : ""} aria-label="选择 ${glyph(item)}"><span lang="ja">${glyph(item)}</span></button>`;
        }).join("")}</div>
        <div class="answer-feedback" role="status">${answer === null ? "听不清可以反复播放，不必猜。" :
          `${answer === question.target.id ? "✓ 答对了" : "再记一次"}：${glyph(question.target)} · ${question.target.romaji}。${pronunciationNote(question.target)}`}</div>
        ${answer !== null ? '<button class="primary" id="next-question">下一题 →</button>' : ""}
      ` : `
        <div class="quiz-intro" lang="ja">あ <span>♪</span> ア</div>
        <p>当前范围：${groupNames[group]} / ${row === "all" ? "全部行" : row}</p>
        <button class="primary" id="start-quiz" ${distinct < 2 ? "disabled" : ""}>开始听辨练习</button>
        ${distinct < 2 ? '<p class="warning">至少选择两个不同发音；请在上方选择更多行。</p>' : ""}
      `}
      <p class="quiz-footer">未熟悉和容易出错的字会更常出现。切换范围或字形会开始新一轮。</p>
    </section>`;
}

function guide(): string {
  return `<section class="panel guide">
    <div class="eyebrow">A SMALL BEGINNING</div><h2>先听清，再记牢</h2>
    <p>假名不是中文谐音，也不是英语音标。先从元音行的 5 个字开始，每天少量练习。</p>
    <div class="steps"><div><b>01</b><h3>听与跟读</h3><p>点击假名听原速录音，再跟读。每个声音都可以在平、片假名之间找到对应字形。</p></div>
    <div><b>02</b><h3>撤掉提示</h3><p>关闭罗马音，尝试直接回想读音，再播放核对。不要一直依赖字母或中文谐音。</p></div>
    <div><b>03</b><h3>隔天再辨认</h3><p>用听辨练习检验字音关联。今天答对不等于永久记住，明天再测一次。</p></div></div>
    <h3>先了解这些区别</h3>
    <dl><dt lang="ja">きゃ / キャ · 拗音</dt><dd>由较小的 ゃ、ゅ、ょ 等与前一个假名组合，例如 きゃ 通常是一拍，而 きや 是两拍。</dd>
    <dt lang="ja">きって / キッテ · 促音</dt><dd>小写 っ／ッ 占一拍，通常表现为后续辅音的阻塞或延长，不是另外读一个 tsu。</dd>
    <dt lang="ja">おばさん / おばあさん · 长音</dt><dd>元音时长会影响词义。长音通常占两拍；片假名常用 ー 表示长音。</dd>
    <dt lang="ja">シ / ツ、ソ / ン · 易混字</dt><dd>比较笔画方向和位置，再结合声音反复辨认，不要只按五十音表的位置记忆。</dd></dl>
    <p class="notice">当前仅提供 71 个基础假名的孤立发音。以上拗音、促音、长音是文字说明，尚未配备专项示范音频。听辨分数不代表口语发音评分。</p>
  </section>`;
}

function resourceView(): string {
  return `<section class="panel guide">
    <div class="eyebrow">OFFLINE BY DESIGN</div><h2>声音留在本地，学习也一样</h2>
    <div class="resource-summary"><strong>${resources?.count ?? 71}</strong><span>真人录音 · 平片假名共用<br>清音 46 / 浊音 20 / 半浊音 5</span></div>
    <p>录音者：<strong>Hakatanoshio117117</strong>。来源为 Wikimedia Commons，由录音者以 PD-self（公有领域）发布。保留原始 Ogg 文件，没有剪辑、合成或修改音高。</p>
    <p>${resources ? `本地音频 ${(resources.bytes / 1024 / 1024).toFixed(2)} MB · 获取日期 ${resources.date}` : "资源清单尚未读取。"}</p>
    <p>每条录音的来源、许可、文件大小及 SHA-256 校验值都已保存在应用内的资源清单中。无需在线语音服务，没有账号和数据上传。</p>
    <h3>你的学习记录</h3><p>${desktop ? "桌面版通过 Rust 保存到系统应用数据目录中的 progress.json，使用临时文件原子替换。更新应用不会主动删除记录。" : "你正在浏览器预览版中，记录保存在此浏览器的 localStorage；与桌面版记录互不相通，清理网站数据会删除预览版记录。"}</p>
    <h3>范围与限制</h3><p>单个假名读音并不能涵盖连读、音高重音、方言、助词变读及歌唱发音。录音响度可能略有差异；播放失败时应用会明确提示，不会用合成声音静默替代。</p>
    <details><summary>查看逐条来源（离线清单）</summary><div id="resource-list"></div></details>
  </section>`;
}

function render(): void {
  const errorMessage = Object.values(errors).filter(Boolean).join(" ");
  const focused = document.activeElement;
  let focusSelector = "";
  if (focused instanceof HTMLElement) {
    if (focused.id) focusSelector = `#${CSS.escape(focused.id)}`;
    else {
      const key = ["view", "script", "kana"].find((key) => focused.dataset[key]);
      if (key) focusSelector = `[data-${key}="${CSS.escape(focused.dataset[key]!)}"]`;
    }
  }
  const stats = summarize(progress, script);
  const titles: Record<View, [string, string, string]> = {
    learn: ["认识假名", "让每一个字，都有声音。", "从听见，到认出。今天先学一小行。"],
    quiz: ["听辨练习", "让耳朵，带你找到答案。", "离开表格顺序，看看你真正记住了多少。"],
    guide: ["学习指南", "不急着背完，先建立连接。", "一点发音常识，一条更可靠的学习路径。"],
    resources: ["本地资源", "好的学习，不依赖网络。", "来源透明，声音与进度都留在你的设备。"],
  };
  app.innerHTML = `
    <aside class="sidebar">
      <a class="brand" href="#" aria-label="假名小调首页"><span class="brand-mark" lang="ja">あ</span><div>假名小调<small>SING JAPANESE</small></div></a>
      <div class="sidebar-label">你的日语第一步</div>
      <nav aria-label="主导航">${(["learn", "quiz", "guide", "resources"] as View[]).map((name, index) =>
        `<button data-view="${name}" class="${view === name ? "active" : ""}" ${view === name ? 'aria-current="page"' : ""}><span class="nav-icon" aria-hidden="true">${["あ", "♫", "≡", "▤"][index]}</span>${titles[name][0]}</button>`).join("")}</nav>
      <div class="sidebar-note"><span class="leaf">♪</span><p>先把声音记住，<br>再让歌声发生。</p><small>第一章 / 假名与声音</small></div>
      <div class="local-badge"><span></span>${desktop ? "离线桌面版" : "浏览器预览版"}<small id="storage-status"></small></div>
    </aside>
    <main>
      <header><span class="breadcrumb">起点 / ${titles[view][0]}</span><span class="chapter">CHAPTER 01 <i></i> KANA</span></header>
      <div class="page-title"><div><span class="eyebrow">A LITTLE EVERY DAY</span><h1>${titles[view][1]}</h1><p>${titles[view][2]}</p></div><div class="title-stamp" lang="ja">音<span>おと</span></div></div>
      <div id="error-notice" class="error-notice" role="alert" ${errorMessage ? "" : "hidden"}><span>${escape(errorMessage)}</span><button id="retry-storage" ${loadFailed || storageFailed ? "" : "hidden"}>${loadFailed ? "重试读取" : "重试保存"}</button></div>
      <div class="stats-bar" aria-label="${scriptName()}学习统计">
        <div><strong id="heard-count">${stats.heard}</strong><span>/ 71 听过的${scriptName()}</span></div>
        <div><strong id="familiar-count">${stats.familiar}</strong><span>连续答对 ≥ 3 次</span></div>
        <div><strong id="accuracy-count">${stats.accuracy === null ? "—" : `${stats.accuracy}%`}</strong><span>累计听辨正确率</span></div>
      </div>
      <fieldset class="workspace" ${loadFailed ? "disabled" : ""}>${view === "learn" ? learn() : view === "quiz" ? quiz() : view === "guide" ? guide() : resourceView()}</fieldset>
      <footer>小步前进，声音会慢慢变熟悉。<span>71 条本地真人录音 · 无需登录</span></footer>
    </main>`;
  updateStatus();
  if (focusSelector) document.querySelector<HTMLElement>(focusSelector)?.focus({ preventScroll: true });
  if (view === "resources") void showResourceList();
}

function nextQuestion(): void {
  resetAudio();
  answer = null;
  question = makeQuestion(pool(), progress, script, question?.target.id);
  render();
  void play(question.target, true);
}

app.addEventListener("click", (event) => {
  if (!(event.target instanceof Element)) return;
  const button = event.target.closest<HTMLElement>("button, .brand");
  if (!button || button instanceof HTMLButtonElement && button.disabled) return;
  if (button.classList.contains("brand")) {
    event.preventDefault();
    resetQuestion();
    view = "learn";
    render();
    return;
  }
  if (button.id === "retry-storage") {
    if (loadFailed) void initialize();
    else {
      errors.storage = "";
      render();
      save(progress);
    }
    return;
  }
  if (button.dataset.view) {
    resetQuestion();
    view = button.dataset.view as View;
    render();
  } else if (button.dataset.script) {
    resetQuestion();
    script = button.dataset.script as Script;
    render();
  } else if (button.dataset.kana) {
    const item = kana.find((item) => item.id === button.dataset.kana);
    if (!item) return;
    selected = item;
    resetAudio();
    render();
    void play(item, false);
  } else if (button.id === "replay") {
    void play(view === "quiz" && question ? question.target : selected, view === "quiz");
  } else if (button.id === "go-quiz") {
    resetQuestion();
    view = "quiz";
    render();
  } else if (button.id === "start-quiz" || button.id === "next-question") {
    nextQuestion();
  } else if (button.dataset.answer && question && answer === null && audioReady) {
    resetAudio();
    answer = button.dataset.answer;
    const correct = answer === question.target.id;
    roundTotal++;
    if (correct) roundCorrect++;
    mark(question.target, correct ? "correct" : "wrong");
    render();
    document.querySelector<HTMLButtonElement>("#next-question")?.focus({ preventScroll: true });
  }
});

app.addEventListener("change", (event) => {
  const target = event.target;
  if (!(target instanceof HTMLSelectElement || target instanceof HTMLInputElement)) return;
  if (target.id === "rate") {
    rate = Number(target.value);
    resetAudio();
    if (view === "quiz" && question) {
      render();
      void play(question.target, true);
    }
    return;
  }
  if (target.id === "romaji" && target instanceof HTMLInputElement) showRomaji = target.checked;
  if (target.id === "group") {
    group = target.value as Group;
    row = "all";
    selected = pool()[0];
    resetQuestion();
  }
  if (target.id === "row") {
    row = target.value;
    selected = pool()[0];
    resetQuestion();
  }
  render();
});

interface ManifestEntry { id: string; hiragana: string; katakana: string; sourcePage: string; author: string; license: string }
let manifestEntries: ManifestEntry[] = [];
async function loadResources(): Promise<void> {
  const response = await fetch("/audio-manifest.json");
  if (!response.ok) throw new Error(`资源清单读取失败：HTTP ${response.status}`);
  const manifest = await response.json();
  if (manifest.version !== 1 || !Array.isArray(manifest.recordings) || manifest.recordings.length !== kana.length) {
    throw new Error("本地资源清单不完整，请执行 npm run resources:verify。");
  }
  manifestEntries = manifest.recordings;
  resources = {
    count: manifest.recordings.length,
    bytes: manifest.recordings.reduce((sum: number, item: { bytes: number }) => sum + item.bytes, 0),
    date: manifest.retrievedAt.slice(0, 10),
  };
}

async function showResourceList(): Promise<void> {
  const container = document.querySelector("#resource-list");
  if (!container) return;
  container.innerHTML = `<ul class="resource-list">${manifestEntries.map((item) =>
    `<li><b lang="ja">${escape(item.hiragana)} / ${escape(item.katakana)}</b> <span>${escape(item.id)} · ${escape(item.author)} · ${escape(item.license)}</span><small>${escape(item.sourcePage)}</small></li>`).join("")}</ul>`;
}

async function initialize(): Promise<void> {
  app.innerHTML = '<div class="loading" role="status">正在读取本地学习记录…</div>';
  try {
    progress = await loadProgress();
    loadFailed = false;
    errors.storage = "";
  } catch (error) {
    loadFailed = true;
    errors.storage = `无法读取学习记录：${String(error)}。为保护原记录，已暂停练习；请检查应用数据目录或浏览器存储后重试。`;
  }
  try {
    await loadResources();
    errors.resources = "";
  } catch (error) {
    errors.resources = `资源错误：${String(error)}`;
  }
  render();
}

void initialize();
