(() => {
  'use strict';
  const canvas = document.getElementById('scene-canvas');
  const note = document.getElementById('scene-note');
  const select = document.getElementById('scene-select');
  const light = document.getElementById('scene-light');
  const scenes = new Map();
  let pf = null, pfError = '', loadingPf = false, detailRequest = 0, chosenSpec = '', currentSpecKey = '';
  async function pfGet(path) {
    const res = await fetch('/scene_editor/api/praeforma' + path, {cache:'no-store', credentials:'same-origin'});
    const data = await res.json();
    if (!res.ok) throw new Error(data.err || 'HTTP ' + res.status);
    return data;
  }
  async function syncPf() {
    if (loadingPf || document.hidden || !document.getElementById('tab-game').classList.contains('active')) return;
    loadingPf = true;
    try {
      const catalog = await pfGet('');
      if (pf?.projectId !== catalog.projectId) chosenSpec = '';
      pf = catalog; pfError = '';
    } catch (err) { pf = null; pfError = err.message; }
    finally { loadingPf = false; }
    updateOptions();
    if (chosenSpec) showSpec(chosenSpec);
  }
  function pfScene() {
    return pf?.scenes.find(s => s.id === select.selectedOptions[0]?.dataset.layout);
  }
  async function showSpec(id) {
    const scene = pfScene();
    if (!scene?.specs.some(s => s.id === id)) return;
    chosenSpec = id;
    const request = ++detailRequest;
    const out = document.getElementById('pf-spec-detail');
    if (out.dataset.spec !== id) {
      out.dataset.spec = id;
      out.textContent = '仕様を読み込んでいます';
    }
    try {
      const {spec, acceptance} = await pfGet('/' + encodeURIComponent(pf.projectId) + '/specs/' + encodeURIComponent(id));
      if (request !== detailRequest) return;
      out.textContent = [spec.title, spec.version == null ? '' : '版 ' + spec.version, spec.description,
        ...(acceptance || []).map(item => item.description || item.text || '')].filter(Boolean).join('\n');
    } catch (err) { if (request === detailRequest) out.textContent = err.message; }
  }
  function renderSpecs() {
    const scene = pfScene();
    const key = JSON.stringify([pf?.projectId,scene?.id]);
    if (key !== currentSpecKey || !scene?.specs.some(s => s.id === chosenSpec)) {
      ++detailRequest; chosenSpec = ''; document.getElementById('pf-spec-detail').textContent = '';
    }
    currentSpecKey = key;
    document.getElementById('pf-info').textContent = pfError || (scene ? (scene.description || scene.name) :
      pf?.scenes.length === 0 ? 'Pf にシーンがありません' : 'Pf に対応するシーンがありません');
    const box = document.getElementById('pf-specs');
    box.replaceChildren();
    if (scene && !scene.specs.length) box.textContent = "このシーンに紐づく仕様はありません";
    for (const spec of scene?.specs || []) {
      const button = document.createElement('button');
      button.textContent = [spec.code, spec.title, spec.status].filter(Boolean).join(' · ');
      button.onclick = () => showSpec(spec.id);
      box.append(button);
    }
  }
  let gl, program, buffer, uniforms, count = 0;
  let sourceStatus = 'シーンを接続しています…';

  function shader(type, source) {
    const obj = gl.createShader(type);
    gl.shaderSource(obj, source);
    gl.compileShader(obj);
    if (!gl.getShaderParameter(obj, gl.COMPILE_STATUS)) {
      gl.deleteShader(obj);
      throw new Error('プレビュー用シェーダを準備できません');
    }
    return obj;
  }

  function initialize() {
    gl = canvas.getContext('webgl2', {alpha: false, antialias: true});
    if (!gl) return;
    const vs = shader(gl.VERTEX_SHADER, `#version 300 es
      layout(location=0) in vec3 position;
      uniform mat4 view, projection;
      uniform float aspect;
      out vec3 world;
      void main() {
        world = position;
        vec4 p = projection * view * vec4(position, 1.);
        gl_Position = vec4(p.x / aspect, -p.y, 2. * p.z - p.w, p.w);
      }`);
    const fs = shader(gl.FRAGMENT_SHADER, `#version 300 es
      precision highp float;
      in vec3 world;
      uniform float intensity;
      out vec4 color;
      void main() {
        vec3 normal = normalize(cross(dFdx(world), dFdy(world)));
        float shade = .16 + intensity * max(dot(normal, normalize(vec3(.4,.8,.5))), 0.);
        color = vec4(vec3(.64,.73,.77) * shade, 1.);
      }`);
    program = gl.createProgram();
    gl.attachShader(program, vs);
    gl.attachShader(program, fs);
    gl.linkProgram(program);
    gl.deleteShader(vs);
    gl.deleteShader(fs);
    if (!gl.getProgramParameter(program, gl.LINK_STATUS)) throw new Error('描画プログラムを準備できません');
    gl.useProgram(program);
    uniforms = Object.fromEntries(['view','projection','aspect','intensity'].map(k => [k, gl.getUniformLocation(program,k)]));
    buffer = gl.createBuffer();
    gl.bindBuffer(gl.ARRAY_BUFFER, buffer);
    gl.enableVertexAttribArray(0);
    gl.vertexAttribPointer(0, 3, gl.FLOAT, false, 0, 0);
    gl.enable(gl.DEPTH_TEST);
    gl.clearColor(.09,.12,.15,1);
  }

  function draw() {
    if (!program || gl.isContextLost()) return;
    const width = Math.min(2048, canvas.clientWidth), height = Math.min(2048, canvas.clientHeight);
    if (!width || !height) return;
    if (canvas.width !== width || canvas.height !== height) {
      canvas.width = width;
      canvas.height = height;
    }
    gl.viewport(0, 0, width, height);
    gl.clear(gl.COLOR_BUFFER_BIT | gl.DEPTH_BUFFER_BIT);
    if (!count) return;
    gl.uniform1f(uniforms.aspect, canvas.clientWidth / canvas.clientHeight);
    gl.uniform1f(uniforms.intensity, Number(light.value));
    gl.drawArrays(gl.TRIANGLES, 0, count);
  }

  function display() {
    count = 0;
    const scene = scenes.get(select.selectedOptions[0]?.dataset.runtimeId);
    let status = pfScene() && !scene ? 'この Pf シーンのゲーム接続を待っています' : sourceStatus;
    if (!program) status = 'WebGL 2 を利用できません';
    else if (scene) {
      status = 'このシーンは編集用形状を配信していません';
      if (scene.cameraMatrices?.length !== 32) status = 'シーンカメラの行列を待っています';
      else {
        const vertices = [];
        for (const actor of scene.actors) {
          for (const part of actor.components.filter(c => c.type === 'EditorPreviewMesh')) {
            const entry = part.props.find(([key]) => key === 'worldTriangles');
            if (!entry) continue;
            const points = JSON.parse(entry[1]);
            if (!Array.isArray(points) || points.length % 9 || !points.every(Number.isFinite))
              throw new Error('編集用形状が不正です');
            if (vertices.length + points.length > 900000) throw new Error('編集用形状の頂点数が上限を超えました');
            for (const x of points) vertices.push(x);
          }
        }
        gl.bufferData(gl.ARRAY_BUFFER, new Float32Array(vertices), gl.STATIC_DRAW);
        gl.uniformMatrix4fv(uniforms.view, false, scene.cameraMatrices.slice(0,16));
        gl.uniformMatrix4fv(uniforms.projection, false, scene.cameraMatrices.slice(16));
        count = vertices.length / 3;
        if (count) status = '';
      }
    }
    note.textContent = status;
    light.disabled = !count;
    draw();
  }

  function refresh() {
    try { display(); } catch (err) {
      count = 0;
      note.textContent = err.message;
      light.disabled = true;
      draw();
    }
  }
  function updateOptions() {
    const previous = select.value;
    const options = [], used = new Set();
    function add(label,key,runtime,layout) {
      const option = new Option(label,key);
      if (runtime) { option.dataset.app = runtime.app; option.dataset.runtimeId = runtime.id; used.add(runtime.id); }
      if (layout) option.dataset.layout = layout;
      options.push(option);
    }
    for (const layout of pf?.scenes || []) {
      const matches = Array.from(scenes.values()).filter(s => (pf.bindings?.[s.app]?.[s.id] || s.id) === layout.id);
      if (!matches.length) add(layout.name,'pf:'+layout.id,null,layout.id);
      matches.forEach((s,i) => add(matches.length > 1 ? layout.name+' / '+s.app+' / '+s.id : layout.name,
        'pf:'+layout.id+(i ? ':'+s.id : ''),s,layout.id));
    }
    for (const s of scenes.values()) if (!used.has(s.id)) add(s.app+' / '+s.id,'runtime:'+s.id,s,null);
    select.replaceChildren(...options);
    if (options.some(o => o.value === previous)) select.value = previous;
    renderSpecs();
    select.dispatchEvent(new Event('scene-list'));
    refresh();
  }
  let socket, retry;
  function connect() {
    socket = new WebSocket((location.protocol === 'https:' ? 'wss://' : 'ws://') + location.host + '/scene_editor/ws');
    socket.onopen = () => socket.send(JSON.stringify({op:'hello',role:'ui'}));
    socket.onmessage = ({data}) => {
      const msg = JSON.parse(data);
      if (msg.op === 'scenes') {
        scenes.clear();
        for (const s of msg.scenes) scenes.set(s.id,s);
      } else if (msg.op === 'scene_update') scenes.set(msg.scene.id,msg.scene);
      else if (msg.op === 'scene_removed') scenes.delete(msg.id);
      else return;
      sourceStatus = 'シーンカメラの接続を待っています';
      updateOptions();
    };
    socket.onclose = () => {
      scenes.clear();
      sourceStatus = 'シーンの接続が切れました';
      updateOptions();
      retry = setTimeout(connect, 3000);
    };
  }
  function setup() {
    try { initialize(); } catch (err) { program = null; note.textContent = err.message; }
    refresh();
  }
  canvas.addEventListener('webglcontextlost', e => { e.preventDefault(); program = null; refresh(); });
  canvas.addEventListener('webglcontextrestored', setup);
  select.addEventListener('change', () => { renderSpecs(); refresh(); });
  light.addEventListener('input', draw);
  new ResizeObserver(draw).observe(canvas);
  window.addEventListener('pagehide', () => { clearTimeout(retry); socket.onclose = null; socket.close(); });
  window.addEventListener('pageshow', e => { if (e.persisted) connect(); });
  document.querySelector('#tabs [data-tab="game"]').addEventListener('click', syncPf);
  document.getElementById('pf-sync').onclick = syncPf;
  document.addEventListener('visibilitychange', () => { if (!document.hidden) syncPf(); });
  setInterval(syncPf, 30000);
  setup();
  connect();
  syncPf();
})();
