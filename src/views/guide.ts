export function guideView(): string {
  return `<div class="guide-layout">
    <section class="steps" aria-label="练习步骤">
      <article class="panel"><b>01</b><h2>听与跟读</h2><p>从元音行开始，点击听音并跟读；切换平、片假名，认识对应字形。</p></article>
      <article class="panel"><b>02</b><h2>关闭提示</h2><p>关闭罗马音，回想读音后再播放核对，避免依赖中文谐音。</p></article>
      <article class="panel"><b>03</b><h2>听辨复习</h2><p>用听辨练习检查字音关联，隔天再测。连续答对不代表长期掌握。</p></article>
    </section>
    <section class="panel guide"><h2>发音区别</h2>
      <dl class="sound-differences"><div><dt lang="ja">きゃ / キャ · 拗音</dt><dd>小写 ゃ、ゅ、ょ 与前字组合。きゃ 通常一拍，きや 两拍。</dd></div>
      <div><dt lang="ja">きって / キッテ · 促音</dt><dd>小写 っ／ッ 占一拍，阻塞或延长后续辅音，不另读 tsu。</dd></div>
      <div><dt lang="ja">おばさん / おばあさん · 长音</dt><dd>元音长短影响词义。长音通常占两拍，片假名常用 ー 表示。</dd></div>
      <div><dt lang="ja">シ / ツ、ソ / ン · 易混字</dt><dd>结合声音比较笔画方向和位置，不只记表格顺序。</dd></div></dl>
      <p class="notice">拗音、促音、长音暂无专项音频；听辨分数不代表口语发音评分。</p>
    </section>
  </div>`;
}
