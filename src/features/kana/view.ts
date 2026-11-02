import { kana, groupNames, pronunciationNote, type Group, type Kana, type Script } from "./data";
import { blankCard, cardKey, type Progress, type Question } from "./progress";
import { escapeHtml } from "../../shared/html";

export interface KanaViewState {
  progress: Progress;
  script: Script;
  group: Group;
  row: string;
  selected: Kana;
  items: Kana[];
  showRomaji: boolean;
  rate: number;
  question: Question | null;
  answer: string | null;
  audioReady: boolean;
  audioStatus: string;
  roundCorrect: number;
  roundTotal: number;
}

function filters({ script, group, row }: KanaViewState): string {
  const rows = [...new Set(kana.filter((item) => item.group === group).map((item) => item.row))];
  return `<div class="filters">
    <div class="segmented" aria-label="假名类型">
      <button data-script="hiragana" aria-pressed="${script === "hiragana"}">あ 平假名</button>
      <button data-script="katakana" aria-pressed="${script === "katakana"}">ア 片假名</button>
    </div>
    <label class="select-label">范围 <select id="group" aria-label="发音分组">${(Object.keys(groupNames) as Group[]).map((key) =>
      `<option value="${key}" ${group === key ? "selected" : ""}>${groupNames[key]}</option>`).join("")}</select></label>
    <label class="select-label">练习行 <select id="row" aria-label="练习行"><option value="all">全部行</option>${rows.map((name) =>
      `<option ${row === name ? "selected" : ""}>${name}</option>`).join("")}</select></label>
  </div>`;
}

function speedControl(rate: number): string {
  return `<label class="speed-label">播放速度 <select id="rate" aria-label="播放速度">
    <option value="1" ${rate === 1 ? "selected" : ""}>原速 1×</option>
    <option value="0.8" ${rate === 0.8 ? "selected" : ""}>慢速 0.8×</option>
  </select></label>`;
}

export function learnView(state: KanaViewState): string {
  const { items, progress, script, selected, showRomaji, rate, audioStatus } = state;
  const rows = [...new Set(items.map((item) => item.row))];
  const card = progress.cards[cardKey(script, selected.id)] ?? blankCard();
  return `${filters(state)}<div class="learning-layout">
    <section class="panel chart-panel" aria-label="假名表">
      <div class="panel-heading"><h2>假名表</h2><label class="toggle"><input id="romaji" type="checkbox" ${showRomaji ? "checked" : ""}> 罗马音提示</label></div>
      <div class="chart-head"><span></span><span>a</span><span>i</span><span>u</span><span>e</span><span>o</span></div>
      ${rows.map((name) => `<div class="kana-row"><span class="row-label">${name}</span>${[0, 1, 2, 3, 4].map((column) => {
        const item = items.find((item) => item.row === name && item.column === column);
        if (!item) return '<span class="empty-cell" aria-hidden="true">·</span>';
        const stats = progress.cards[cardKey(script, item.id)] ?? blankCard();
        return `<button class="kana-cell ${selected.id === item.id ? "selected" : ""} ${stats.streak >= 3 ? "familiar" : ""}" data-kana="${item.id}" aria-label="学习 ${item[script]}" aria-pressed="${selected.id === item.id}">
          <span lang="ja">${item[script]}</span>${showRomaji ? `<small>${item.romaji}</small>` : ""}
          ${stats.streak >= 3 ? '<i class="known-dot" title="连续答对至少 3 次"></i>' : ""}
        </button>`;
      }).join("")}</div>`).join("")}
      <p class="chart-footnote">点击听音 · 绿点：连续答对 ≥ 3 次</p>
    </section>
    <aside class="panel sound-card">
      <div class="big-kana" lang="ja">${selected[script]}</div>
      <div class="counterpart" lang="ja">${script === "hiragana" ? "片假名" : "平假名"} <strong>${script === "hiragana" ? selected.katakana : selected.hiragana}</strong></div>
      ${showRomaji ? `<div class="romaji-large">${selected.romaji}</div>` : ""}
      <button class="primary wide" id="replay">▶ 播放发音</button>
      ${speedControl(rate)}
      <p id="audio-status" class="audio-status" role="status">${escapeHtml(audioStatus)}</p>
      <div class="tip"><span>发音小提示</span><p>${pronunciationNote(selected)}</p></div>
      <div class="card-counts">完整听过 ${card.listens} 次 · 答对 ${card.correct} 次 · 答错 ${card.wrong} 次</div>
      <button class="text-button wide" id="go-quiz">去听辨练习 →</button>
    </aside>
  </div>`;
}

export function quizView(state: KanaViewState): string {
  const { items, script, question, answer, audioReady, rate, audioStatus, roundCorrect, roundTotal } = state;
  const distinct = new Set(items.map((item) => item.sound)).size;
  return `${filters(state)}<section class="panel quiz-panel">
    <div class="quiz-top"><span>本轮答对</span><strong>${roundCorrect} / ${roundTotal}</strong></div>
    <h2>听到的是哪一个${script === "hiragana" ? "平假名" : "片假名"}？</h2>
    ${question ? `
      <button class="listen-orb" id="replay" aria-label="重听题目">▶</button>
      ${speedControl(rate)}
      <p id="audio-status" class="audio-status" role="status">${escapeHtml(audioStatus || "点击播放，再选择答案")}</p>
      <div class="choices">${question.choices.map((item) => {
        const status = answer !== null ? item.id === question.target.id ? "correct" : item.id === answer ? "wrong" : "" : "";
        return `<button class="choice ${status}" data-answer="${item.id}" ${answer !== null || !audioReady ? "disabled" : ""} aria-label="选择 ${item[script]}"><span lang="ja">${item[script]}</span></button>`;
      }).join("")}</div>
      <div class="answer-feedback" role="status">${answer === null ? "" :
        `${answer === question.target.id ? "✓ 答对了" : "再记一次"}：${question.target[script]} · ${question.target.romaji}。${pronunciationNote(question.target)}`}</div>
      ${answer !== null ? '<button class="primary" id="next-question">下一题 →</button>' : ""}
    ` : `<img class="quiz-intro" src="/illustrations/quiz-headphones.svg" alt="" aria-hidden="true">
      <button class="primary" id="start-quiz" ${distinct < 2 ? "disabled" : ""}>开始听辨练习</button>
      ${distinct < 2 ? '<p class="warning">至少选择两个不同发音；请在上方选择更多行。</p>' : ""}`}
  </section>`;
}
