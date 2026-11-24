// Ergo Studio in the browser (spec/feature/tool/shell.md): Tela's web renderer (tela.js) on
// WS /shell/ws, plus the remote-play extension point.
//
// Remote play: the play tab declares a video element whose stream key is "play:<target>" while
// the user watches. This script then opens WebRTC through the play plugin's own signaling socket
// (/play/ws, unchanged) and attaches the stream under that key. Pointer input over the picture,
// keys while the picture has the focus, and the tab's operation buttons (elements whose id ends
// in "/key-<name>") go to the game over the same socket (the plugin takes input from its viewer
// connection only).
(function () {
  'use strict';
  const canvas = document.getElementById('screen');
  const note = document.getElementById('note');
  const wsBase = (location.protocol === 'https:' ? 'wss:' : 'ws:') + '//' + location.host;
  const shell = { socket: null, view: null, retry: 0 };
  const thumbnails = new Map();
  const thumbnailStream = 'visus:/asset_import/api/thumbnail?name=';

  function syncThumbnails(frame) {
    const shown = new Set();
    const canvasBox = canvas.getBoundingClientRect();
    const stageBox = canvas.parentElement.getBoundingClientRect();
    for (const e of frame.elements) {
      if (e.kind !== 'video' || typeof e.stream !== 'string' ||
          !e.stream.startsWith(thumbnailStream) || e.clip[2] <= 0 || e.clip[3] <= 0) continue;
      shown.add(e.id);
      let wrapper = thumbnails.get(e.id);
      if (!wrapper) {
        wrapper = document.createElement('div');
        Object.assign(wrapper.style, { position: 'absolute', overflow: 'hidden', pointerEvents: 'none' });
        const img = document.createElement('img');
        Object.assign(img.style, { position: 'absolute', objectFit: 'contain' });
        wrapper.appendChild(img);
        canvas.parentElement.appendChild(wrapper);
        thumbnails.set(e.id, wrapper);
      }
      const img = wrapper.firstChild;
      const url = e.stream.slice('visus:'.length);
      if (img.getAttribute('src') !== url) img.src = url;
      img.alt = e.label || '';
      Object.assign(wrapper.style, {
        left: `${canvasBox.left - stageBox.left + e.clip[0]}px`,
        top: `${canvasBox.top - stageBox.top + e.clip[1]}px`,
        width: `${e.clip[2]}px`, height: `${e.clip[3]}px`,
      });
      Object.assign(img.style, {
        left: `${e.bounds[0] - e.clip[0]}px`, top: `${e.bounds[1] - e.clip[1]}px`,
        width: `${e.bounds[2]}px`, height: `${e.bounds[3]}px`,
      });
    }
    for (const [id, wrapper] of thumbnails) {
      if (shown.has(id)) continue;
      wrapper.remove();
      thumbnails.delete(id);
    }
  }

  function showNote(text) { note.textContent = text; note.hidden = !text; }

  function connect() {
    const socket = new WebSocket(wsBase + '/shell/ws');
    shell.socket = socket;
    socket.addEventListener('open', () => {
      shell.retry = 0;
      showNote('');
      if (shell.view) shell.view.dispose();
      // A new page on the server: a new view sends its size and gets the whole frame.
      shell.view = Tela.attach(canvas, { send: (m) => { if (socket.readyState === WebSocket.OPEN) socket.send(m); } });
      canvas.focus();
    });
    socket.addEventListener('message', (ev) => {
      if (!shell.view) return;
      shell.view.receive(ev.data);
      if (shell.view.frame) {
        play.sync(shell.view.frame);
        syncThumbnails(shell.view.frame);
      }
    });
    socket.addEventListener('close', () => {
      showNote('Studio との接続が切れました。つなぎ直しています…');
      play.stop();
      syncThumbnails({ elements: [] });
      shell.retry = Math.min(shell.retry + 1, 6);
      setTimeout(connect, 500 * shell.retry);
    });
  }

  // ---- remote play -----------------------------------------------------------

  // KeyboardEvent.code -> the play plugin's key names (key_names.h).
  function keyName(code) {
    if (/^Key[A-Z]$/.test(code)) return code.slice(3);
    if (/^Digit[0-9]$/.test(code)) return code.slice(5);
    if (/^F([1-9]|1[0-2])$/.test(code)) return code;
    const named = {
      Enter: 'Enter', NumpadEnter: 'Enter', Escape: 'Escape', Space: 'Space', Tab: 'Tab', Backspace: 'Backspace',
      ShiftLeft: 'Shift', ShiftRight: 'Shift', ControlLeft: 'Ctrl', ControlRight: 'Ctrl', AltLeft: 'Alt', AltRight: 'Alt',
      ArrowUp: 'Up', ArrowDown: 'Down', ArrowLeft: 'Left', ArrowRight: 'Right', Insert: 'Insert', Delete: 'Delete',
      Home: 'Home', End: 'End', PageUp: 'PageUp', PageDown: 'PageDown', CapsLock: 'CapsLock', Minus: 'Minus',
      Equal: 'Equal', BracketLeft: 'BracketLeft', BracketRight: 'BracketRight', Semicolon: 'Semicolon', Quote: 'Quote',
      Comma: 'Comma', Period: 'Period', Slash: 'Slash', Backslash: 'Backslash', Backquote: 'Backquote',
    };
    return named[code] || '';
  }

  const play = {
    stream: '', ws: null, pc: null, bounds: null, keys: [], focused: false, held: '', lastMove: 0, video: null,

    // Called after every frame: start or stop watching as the play tab declares.
    sync(frame) {
      let wanted = '', bounds = null;
      this.keys = [];
      for (const e of frame.elements) {
        if (e.kind === 'video' && typeof e.stream === 'string' && e.stream.startsWith('play:')) { wanted = e.stream; bounds = e.bounds; }
        if (e.kind === 'button') {
          const m = /\/key-([A-Za-z0-9]+)$/.exec(e.id);
          if (m) this.keys.push({ key: m[1], bounds: e.bounds });
        }
      }
      this.bounds = bounds;
      if (wanted === this.stream) return;
      this.stop();
      if (wanted) this.start(wanted);
    },

    send(msg) { if (this.ws && this.ws.readyState === WebSocket.OPEN) this.ws.send(JSON.stringify(msg)); },

    stop() {
      if (this.pc) { this.pc.close(); this.pc = null; }
      if (this.ws) { const ws = this.ws; this.ws = null; ws.close(); }
      if (this.stream && shell.view) shell.view.detachStream(this.stream);
      this.stream = '';
      this.video = null;
      this.focused = false;
    },

    async start(stream) {
      this.stream = stream;
      const target = stream.slice('play:'.length);
      try {
        const ice = await (await fetch('/play/api/ice')).json();
        if (this.stream !== stream) return;
        const ws = new WebSocket(wsBase + '/play/ws');
        this.ws = ws;
        ws.addEventListener('message', async (ev) => {
          let m;
          try { m = JSON.parse(ev.data); } catch { return; }
          if (m.type === 'answer' && this.pc) await this.pc.setRemoteDescription({ type: 'answer', sdp: m.sdp });
          else if (m.type === 'error') showNote('遠隔プレイ: ' + (m.message || m.error));
        });
        await new Promise((resolve, reject) => {
          ws.addEventListener('open', resolve, { once: true });
          ws.addEventListener('error', () => reject(new Error('the play socket did not open')), { once: true });
        });
        const pc = new RTCPeerConnection({ iceServers: ice.iceServers || [] });
        this.pc = pc;
        const tr = pc.addTransceiver('video', { direction: 'recvonly' });
        const caps = RTCRtpReceiver.getCapabilities && RTCRtpReceiver.getCapabilities('video');
        if (caps && tr.setCodecPreferences) {
          const h264 = caps.codecs.filter((c) => /h264/i.test(c.mimeType));
          if (h264.length) tr.setCodecPreferences(h264);
        }
        pc.addEventListener('track', (e) => {
          if (shell.view && this.stream === stream) shell.view.attachStream(stream, e.streams[0] || new MediaStream([e.track]));
        });
        await pc.setLocalDescription(await pc.createOffer());
        // The Studio takes one offer with its candidates (no trickle): wait for gathering.
        await new Promise((resolve) => {
          if (pc.iceGatheringState === 'complete') return resolve();
          pc.addEventListener('icegatheringstatechange', () => { if (pc.iceGatheringState === 'complete') resolve(); });
          setTimeout(resolve, 3000);
        });
        if (this.stream !== stream) return;
        this.send({ type: 'offer', target, sdp: pc.localDescription.sdp });
      } catch (e) {
        showNote('映像をつなげません: ' + e.message);
      }
    },

    // Pointer position as 0..1 of the picture (the video keeps its aspect ratio).
    point(ev, clamp) {
      if (!this.bounds) return null;
      const r = canvas.getBoundingClientRect();
      const [bx, by, bw, bh] = this.bounds;
      const v = shell.view && shell.view.videos ? shell.view.videos.get(this.stream) : null;
      const vw = (v && v.videoWidth) || bw, vh = (v && v.videoHeight) || bh;
      const scale = Math.min(bw / vw, bh / vh);
      const w = vw * scale, h = vh * scale;
      const x = (ev.clientX - r.left - bx - (bw - w) / 2) / w;
      const y = (ev.clientY - r.top - by - (bh - h) / 2) / h;
      if (clamp) return { x: Math.min(1, Math.max(0, x)), y: Math.min(1, Math.max(0, y)) };
      if (x < 0 || x > 1 || y < 0 || y > 1) return null;
      return { x, y };
    },

    keyAt(ev) {
      const r = canvas.getBoundingClientRect();
      const x = ev.clientX - r.left, y = ev.clientY - r.top;
      const hit = this.keys.find((k) => x >= k.bounds[0] && y >= k.bounds[1] && x < k.bounds[0] + k.bounds[2] && y < k.bounds[1] + k.bounds[3]);
      return hit ? hit.key : '';
    },
  };

  // Capture phase: the game sees these before tela.js (which still gets them for its own state).
  canvas.addEventListener('pointerdown', (ev) => {
    if (!play.pc) return;
    const key = play.keyAt(ev);
    if (key) { play.held = key; play.send({ type: 'key', key, down: true }); return; }
    const p = play.point(ev);
    play.focused = !!p;
    if (p) play.send({ type: 'pointer', x: p.x, y: p.y, button: ev.button, phase: 'down' });
  }, true);
  canvas.addEventListener('pointerup', (ev) => {
    if (!play.pc) return;
    if (play.held) { play.send({ type: 'key', key: play.held, down: false }); play.held = ''; return; }
    if (!play.focused) return;
    const p = play.point(ev, true);   // a release outside the picture still lets go
    play.send({ type: 'pointer', x: p.x, y: p.y, button: ev.button, phase: 'up' });
  }, true);
  canvas.addEventListener('pointermove', (ev) => {
    const now = performance.now();
    if (!play.pc || now - play.lastMove < 16) return;
    const p = play.point(ev);
    if (!p) return;
    play.lastMove = now;
    play.send({ type: 'pointer', x: p.x, y: p.y, button: 0, phase: 'move' });
  }, true);
  canvas.addEventListener('wheel', (ev) => {
    const p = play.pc ? play.point(ev) : null;
    if (!p || !ev.deltaY) return;
    ev.preventDefault();
    ev.stopPropagation();
    const notches = -Math.sign(ev.deltaY) * Math.max(1, Math.min(10, Math.round(Math.abs(ev.deltaY) / 100)));
    play.send({ type: 'wheel', x: p.x, y: p.y, delta: notches });
  }, { capture: true, passive: false });
  const onKey = (down) => (ev) => {
    if (!play.pc || !play.focused) return;
    const key = keyName(ev.code);
    if (!key) return;
    ev.preventDefault();
    ev.stopPropagation();
    if (down && ev.repeat) return;   // the game sees the key held, not repeated
    play.send({ type: 'key', key, down });
  };
  window.addEventListener('keydown', onKey(true), true);
  window.addEventListener('keyup', onKey(false), true);

  connect();
})();
