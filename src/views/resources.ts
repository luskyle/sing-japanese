import type { ResourceCatalog } from "../services/resources";
import { escapeHtml as escape } from "../shared/html";

export function resourcesView(resources: ResourceCatalog | null, desktop: boolean): string {
  return `<div class="resource-layout">
    <section class="panel guide"><h2>发音录音</h2>
      <div class="resource-summary"><strong>${resources?.count ?? 71}</strong><span>条真人录音 · 平片假名共用<br>清音 46 / 浊音 20 / 半浊音 5</span></div>
      <p>Hakatanoshio117117 · Wikimedia Commons<br>PD-self（公有领域），原始 Ogg 录音。</p>
      <p class="muted">${resources ? `${(resources.bytes / 1024 / 1024).toFixed(2)} MB · 获取于 ${escape(resources.date)}` : "资源清单尚未读取。"}</p>
    </section>
    <section class="panel guide"><h2>数据与使用范围</h2>
      <h3>学习记录</h3><p>${desktop ? "保存在本机应用数据目录，更新应用不会主动删除。" : "保存在当前浏览器，清理网站数据会删除记录。"}桌面版与浏览器记录互不共享。</p>
      <h3>读音范围</h3><p>录音为单字发音，不包含连读、重音或歌唱变化。歌词注音采用常规词典读法，人名与特殊唱法可能不同。</p>
      <p class="muted">歌词仅在本次会话保留，不上传、不自动存盘。</p>
    </section>
    <section class="panel guide resource-details" aria-label="来源与许可">
      <details><summary>录音来源与许可（71 条）</summary><div id="resource-list"><ul class="resource-list">${resources?.recordings.map((item) =>
        `<li><b lang="ja">${escape(item.hiragana)} / ${escape(item.katakana)}</b> <span>${escape(item.id)} · ${escape(item.author)} · ${escape(item.license)}</span><small>${escape(item.sourcePage)}</small></li>`).join("") ?? ""}</ul></div></details>
      <details><summary>IPADIC 词典许可声明</summary><pre class="license-text" id="dictionary-license"></pre></details>
    </section>
  </div>`;
}
