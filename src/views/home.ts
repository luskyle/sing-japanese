import { summarize, type Progress } from "../features/kana/progress";

export function homeView(progress: Progress): string {
  const { heard } = summarize(progress, "hiragana");
  return `<div class="home-layout">
    <section class="study-companion" aria-label="学习进度">
      <img class="companion-art" src="/illustrations/study-buddy.svg" alt="" aria-hidden="true">
      <div class="companion-progress"><span class="progress-glyph" lang="ja">あ</span><div><strong>平假名已听 ${heard} / 71</strong>
        <div class="learning-meter" role="meter" aria-label="平假名已听数量" aria-valuemin="0" aria-valuemax="71" aria-valuenow="${heard}"><span style="width: ${heard / 71 * 100}%"></span></div>
      </div></div>
    </section>
    <div class="home-tools">
      <section class="home-path" aria-label="学习与使用路径">
        <article class="feature-card kana-entry">
          <span class="feature-step">01 / 先学基础</span><h2>先认识假名</h2>
          <p>真人发音 · 平假名与片假名</p>
          <img class="feature-art" src="/illustrations/kana-notebook.svg" alt="" aria-hidden="true">
          <button class="feature-link" data-view="learn" aria-label="认识假名">开始学习 <span aria-hidden="true">›</span></button>
        </article>
        <article class="feature-card lyrics-entry">
          <span class="feature-step">02 / 核心功能</span><h2>歌词注音</h2>
          <p>给歌词中的汉字标上假名</p>
          <img class="feature-art" src="/illustrations/lyrics-music.svg" alt="" aria-hidden="true">
          <button class="feature-link" data-view="lyrics" aria-label="歌词注音">打开注音 <span aria-hidden="true">›</span></button>
        </article>
      </section>
      <section class="home-extras" aria-label="练习与帮助">
        <article class="feature-card quiz-entry"><h2>听辨练习</h2><p>听声音，选出对应假名</p>
          <img class="feature-art" src="/illustrations/quiz-headphones.svg" alt="" aria-hidden="true">
          <button class="feature-link" data-view="quiz" aria-label="听辨练习">去练习 <span aria-hidden="true">›</span></button></article>
        <article class="feature-card guide-entry"><h2>学习指南</h2><p>练习方法与发音区别</p>
          <img class="feature-art" src="/illustrations/guide-planet.svg" alt="" aria-hidden="true">
          <button class="feature-link" data-view="guide" aria-label="学习指南">查看指南 <span aria-hidden="true">›</span></button></article>
      </section>
      <section class="home-dock" aria-label="本地资源入口">
        <span class="dock-icon" aria-hidden="true">♫</span><span><strong>71 条本地真人录音</strong><small>音频来源与词典许可</small></span>
        <button class="text-button" data-view="resources" aria-label="本地资源">本地资源 <span aria-hidden="true">›</span></button>
      </section>
    </div>
  </div>`;
}
