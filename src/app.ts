import { kana, type Group, type Kana, type Script } from "./features/kana/data";
import { blankCard, cardKey, emptyProgress, makeQuestion, record, summarize, type Progress, type Question } from "./features/kana/progress";
import { learnView, quizView, type KanaViewState } from "./features/kana/view";
import { lyricsView, updateLyricsInput, toggleLyricsReadings, annotateLyrics } from "./features/lyrics";
import { desktop, loadProgress, saveProgress } from "./services/storage";
import { PronunciationPlayer } from "./services/audio";
import { loadResources, loadDictionaryLicense, type ResourceCatalog } from "./services/resources";
import { homeView } from "./views/home";
import { guideView } from "./views/guide";
import { resourcesView } from "./views/resources";
import { escapeHtml } from "./shared/html";

type View = "home" | "learn" | "quiz" | "guide" | "resources" | "lyrics";
const root = document.querySelector<HTMLDivElement>("#app");
if (!root) throw new Error("缺少应用挂载节点");
const app = root;
const player = new PronunciationPlayer();
let progress = emptyProgress();
let script: Script = "hiragana";
let group: Group = "basic";
let row = "all";
let view: View = "home";
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
let resources: ResourceCatalog | null = null;
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
  if (status) status.textContent = loadFailed ? "记录读取失败" : storageFailed ? "保存失败" : pendingSaves ? "正在保存…" : "进度已保存在本机";
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
  if (text) text.textContent = errorMessage;
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

function navigate(next: View): void {
  resetQuestion();
  view = next;
  render();
  window.scrollTo({ top: 0 });
  document.querySelector<HTMLElement>("h1")?.focus({ preventScroll: true });
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
  const titles: Record<View, string> = {
    home: "学习与工具", learn: "认识假名", quiz: "听辨练习",
    guide: "学习指南", resources: "本地资源", lyrics: "歌词注音",
  };
  const state: KanaViewState = {
    progress, script, group, row, selected, items: pool(), showRomaji, rate,
    question, answer, audioReady, audioStatus, roundCorrect, roundTotal,
  };
  const content = view === "home" ? homeView(progress) : view === "learn" ? learnView(state)
    : view === "quiz" ? quizView(state) : view === "lyrics" ? lyricsView()
      : view === "guide" ? guideView() : resourcesView(resources, desktop);
  app.classList.toggle("home-screen", view === "home");
  app.innerHTML = `
    <div class="app-bar">
      <a class="brand" href="#" aria-label="假名小调首页"><span class="brand-mark" lang="ja">あ</span><span>假名小调</span></a>
      <div class="local-badge"><span></span>${desktop ? "离线桌面版" : "浏览器预览版"}${view === "home" || view === "learn" || view === "quiz" ? '<small id="storage-status"></small>' : ""}</div>
    </div>
    <main>
      <div class="page-title"><h1 tabindex="-1">${titles[view]}</h1>${view === "home" ? "" : '<button class="text-button back-button" data-view="home">← 返回首页</button>'}</div>
      <div id="error-notice" class="error-notice" role="alert" ${errorMessage ? "" : "hidden"}><span>${escapeHtml(errorMessage)}</span><button id="retry-storage" ${loadFailed || storageFailed ? "" : "hidden"}>${loadFailed ? "重试读取" : "重试保存"}</button></div>
      ${view === "learn" || view === "quiz" ? `<div class="stats-bar" aria-label="${scriptName()}学习统计">
        <div><strong id="heard-count">${stats.heard}</strong><span>/ 71 听过的${scriptName()}</span></div>
        <div><strong id="familiar-count">${stats.familiar}</strong><span>连续答对 ≥ 3 次</span></div>
        <div><strong id="accuracy-count">${stats.accuracy === null ? "—" : `${stats.accuracy}%`}</strong><span>听辨正确率</span></div>
      </div>` : ""}
      <fieldset class="workspace" ${loadFailed ? "disabled" : ""}>${content}</fieldset>
    </main>`;
  updateStatus();
  if (focusSelector) document.querySelector<HTMLElement>(focusSelector)?.focus({ preventScroll: true });
  if (view === "resources") void showDictionaryLicense();
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
    navigate("home");
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
  if (button.id === "annotate-lyrics") {
    void annotateLyrics(render);
    return;
  }
  if (button.dataset.view) navigate(button.dataset.view as View);
  else if (button.dataset.script) {
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
  } else if (button.id === "go-quiz") navigate("quiz");
  else if (button.id === "start-quiz" || button.id === "next-question") nextQuestion();
  else if (button.dataset.answer && question && answer === null && audioReady) {
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

app.addEventListener("input", (event) => {
  if (event.target instanceof HTMLTextAreaElement && event.target.id === "lyrics-input") updateLyricsInput(event.target.value);
});

app.addEventListener("change", (event) => {
  const target = event.target;
  if (!(target instanceof HTMLSelectElement || target instanceof HTMLInputElement)) return;
  if (target.id === "lyrics-readings" && target instanceof HTMLInputElement) {
    toggleLyricsReadings(target.checked);
    render();
    return;
  }
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

async function showDictionaryLicense(): Promise<void> {
  try {
    const text = await loadDictionaryLicense();
    const license = document.querySelector("#dictionary-license");
    if (license) license.textContent = text;
  } catch (error) {
    reportError(`无法读取词典许可声明：${String(error)}`, "resources");
  }
}

export async function initialize(): Promise<void> {
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
    resources = await loadResources();
    errors.resources = "";
  } catch (error) {
    errors.resources = `资源错误：${String(error)}`;
  }
  render();
}
