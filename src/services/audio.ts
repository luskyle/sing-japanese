export class PronunciationPlayer {
  private current: HTMLAudioElement | null = null;
  private generation = 0;
  private request: AbortController | null = null;
  private objectUrl: string | null = null;

  stop(): void {
    this.generation++;
    this.request?.abort();
    this.request = null;
    if (this.current) {
      this.current.pause();
      this.current.removeAttribute("src");
      this.current.load();
      this.current = null;
    }
    if (this.objectUrl) {
      URL.revokeObjectURL(this.objectUrl);
      this.objectUrl = null;
    }
  }

  async play(id: string, rate: number, onEnded: () => void, onError: (message: string) => void): Promise<boolean> {
    this.stop();
    const generation = this.generation;
    const audio = new Audio();
    const request = new AbortController();
    this.request = request;
    this.current = audio;
    audio.playbackRate = rate;
    audio.preservesPitch = true;
    audio.onended = () => {
      if (generation === this.generation) onEnded();
    };
    audio.onerror = () => {
      if (generation === this.generation) {
        onError(`无法解码 ${id} 的本地录音（媒体错误 ${audio.error?.code ?? "未知"}），请检查音频资源和系统音频组件。`);
      }
    };
    let stage: "load" | "play" = "load";
    try {
      const response = await fetch(`/audio/${id}.ogg`, { signal: request.signal });
      if (!response.ok) throw new Error(`本地录音 ${id}：HTTP ${response.status}`);
      const bytes = await response.arrayBuffer();
      if (generation !== this.generation) return false;
      // WebKit's media pipeline cannot reliably load Tauri's custom URI scheme.
      this.objectUrl = URL.createObjectURL(new Blob([bytes], { type: "audio/ogg" }));
      audio.src = this.objectUrl;
      stage = "play";
      await audio.play();
      return generation === this.generation;
    } catch (error) {
      if (generation !== this.generation) return false;
      const detail = error instanceof Error ? error.message : String(error);
      onError(stage === "load"
        ? `录音加载失败：${detail}。浏览器预览请确认本地服务仍在运行（npm run dev），再重试；桌面版请检查应用资源是否完整。`
        : `播放失败：${detail}。请重试并检查系统音频输出或播放权限。`);
      return false;
    }
  }
}
