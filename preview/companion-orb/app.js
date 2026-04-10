(function(){
  const canvas = document.getElementById('orb');
  const ctx = canvas.getContext('2d');
  const W = canvas.width, H = canvas.height, CX = W/2, CY = H/2;

  const themes = {
    chill: { bg: '#071826', accent: '#49c7ff', accentStrong: '#86e6ff', moodRing: '#3fa8ff', accentDim: '#2b6f90' },
    sport: { bg: '#20130c', accent: '#ff8a36', accentStrong: '#ffbf7a', moodRing: '#ffcf33', accentDim: '#7a3b1a' }
  };

  const ui = {
    driveMode: document.getElementById('driveMode'),
    mood: document.getElementById('mood'),
    rpm: document.getElementById('rpm'),
    sleep: document.getElementById('sleep'),
    reaction: document.getElementById('reaction'),
    progress: document.getElementById('progress')
  };

  const state = { drive: 'chill', mood: 0, rpm: 0, sleeping: false, reaction: 'none', progress: 0.3, breath: 0 };

  function hexToRgb(h) {
    const v = h.replace('#','');
    return { r: parseInt(v.substring(0,2),16), g: parseInt(v.substring(2,4),16), b: parseInt(v.substring(4,6),16) };
  }
  function rgbToHex(r,g,b){
    const to = (n)=>Math.max(0,Math.min(255,Math.round(n))).toString(16).padStart(2,'0');
    return '#'+to(r)+to(g)+to(b);
  }
  function blendHex(a,b,t){
    const A=hexToRgb(a), B=hexToRgb(b);
    return rgbToHex(A.r + (B.r-A.r)*t, A.g + (B.g-A.g)*t, A.b + (B.b-A.b)*t);
  }

  function clear() { ctx.clearRect(0,0,W,H); }

  function drawRing(progress, rpm, modeBlend){
    const base = blendHex(themes.chill.moodRing, themes.sport.moodRing, modeBlend);
    const dim = blendHex(themes.chill.bg, themes.sport.bg, modeBlend);
    const rpmNorm = Math.min(1, state.rpm/6000);
    const pulse = 0.5 + 0.5*Math.sin(performance.now()* (0.002 + rpmNorm*0.012));
    const trackColor = blendHex(dim, base, 0.08 + 0.12*pulse);

    // track
    ctx.lineWidth = 10;
    ctx.strokeStyle = trackColor;
    ctx.beginPath();
    ctx.arc(CX, CY, 116, 0, Math.PI*2);
    ctx.stroke();

    // progress arc
    if (progress > 0.005) {
      const start = -Math.PI/2;
      const end = start + Math.min(progress,1.0)*Math.PI*2;
      ctx.lineWidth = 10;
      ctx.strokeStyle = blendHex('#ffffff', base, 0.15);
      ctx.beginPath();
      ctx.arc(CX, CY, 116, start, end);
      ctx.stroke();
    }
  }

  function drawFace(){
    const modeBlend = (state.drive==='sport') ? 1 : 0;
    const bg = blendHex(themes.chill.bg, themes.sport.bg, modeBlend);
    const accent = blendHex(themes.chill.accent, themes.sport.accent, modeBlend);
    const strong = blendHex(themes.chill.accentStrong, themes.sport.accentStrong, modeBlend);

    // interior
    ctx.fillStyle = bg;
    ctx.beginPath(); ctx.arc(CX, CY, 110, 0, Math.PI*2); ctx.fill();

    const breath = 0.5 + 0.5*Math.sin(state.breath);
    const boff = Math.round(breath*2);
    const lean = 0; // simplified

    const eye_spacing = 40;
    const eye_r = 18;
    const eye_y = CY - 18 + boff;
    const lx = CX - eye_spacing + lean;
    const rx = CX + eye_spacing + lean;

    // Eyes by mood
    if (state.sleeping) {
      // closed
      ctx.fillStyle = accent;
      ctx.beginPath(); ctx.arc(lx, eye_y, eye_r, 0, Math.PI); ctx.fill();
      ctx.beginPath(); ctx.arc(rx, eye_y, eye_r, 0, Math.PI); ctx.fill();
    } else if (state.mood==3) {
      // happy - squint (filled circles then clear top)
      ctx.fillStyle = accent; ctx.beginPath(); ctx.ellipse(lx, eye_y, eye_r, eye_r*0.6, 0, 0, Math.PI*2); ctx.fill();
      ctx.fillStyle = accent; ctx.beginPath(); ctx.ellipse(rx, eye_y, eye_r, eye_r*0.6, 0, 0, Math.PI*2); ctx.fill();
      // mask top
      ctx.fillStyle = bg; ctx.fillRect(lx-eye_r-2, eye_y-eye_r-2, (eye_r+2)*2, eye_r-2);
      ctx.fillRect(rx-eye_r-2, eye_y-eye_r-2, (eye_r+2)*2, eye_r-2);

    } else if (state.mood==4) {
      // sad: top half visible
      ctx.fillStyle = accent; ctx.beginPath(); ctx.arc(lx, eye_y, eye_r, Math.PI, 2*Math.PI); ctx.fill();
      ctx.fillStyle = accent; ctx.beginPath(); ctx.arc(rx, eye_y, eye_r, Math.PI, 2*Math.PI); ctx.fill();
    } else if (state.mood==2) {
      // alert: big open eyes
      ctx.fillStyle = accent; ctx.beginPath(); ctx.arc(lx, eye_y, eye_r+3, 0, Math.PI*2); ctx.fill();
      ctx.fillStyle = bg; ctx.beginPath(); ctx.arc(lx+2, eye_y+2, eye_r-5, 0, Math.PI*2); ctx.fill();
      ctx.fillStyle = strong; ctx.beginPath(); ctx.arc(lx-5, eye_y-6, 4, 0, Math.PI*2); ctx.fill();
      ctx.fillStyle = accent; ctx.beginPath(); ctx.arc(rx, eye_y, eye_r+3, 0, Math.PI*2); ctx.fill();
      ctx.fillStyle = bg; ctx.beginPath(); ctx.arc(rx+2, eye_y+2, eye_r-5, 0, Math.PI*2); ctx.fill();
      ctx.fillStyle = strong; ctx.beginPath(); ctx.arc(rx-5, eye_y-6, 4, 0, Math.PI*2); ctx.fill();

    } else if (state.mood==5) {
      // excited: X eyes
      ctx.fillStyle = accent; ctx.beginPath(); ctx.arc(lx, eye_y, eye_r, 0, Math.PI*2); ctx.fill();
      ctx.fillStyle = bg; ctx.strokeStyle = bg; ctx.lineWidth = 3; ctx.beginPath(); ctx.moveTo(lx-eye_r+2, eye_y-eye_r+2); ctx.lineTo(lx+eye_r-2, eye_y+eye_r-2); ctx.stroke();
      ctx.beginPath(); ctx.moveTo(lx+eye_r-2, eye_y-eye_r+2); ctx.lineTo(lx-eye_r+2, eye_y+eye_r-2); ctx.stroke();
      ctx.fillStyle = accent; ctx.beginPath(); ctx.arc(rx, eye_y, eye_r, 0, Math.PI*2); ctx.fill();
      ctx.beginPath(); ctx.moveTo(rx-eye_r+2, eye_y-eye_r+2); ctx.lineTo(rx+eye_r-2, eye_y+eye_r-2); ctx.stroke();
      ctx.beginPath(); ctx.moveTo(rx+eye_r-2, eye_y-eye_r+2); ctx.lineTo(rx-eye_r+2, eye_y+eye_r-2); ctx.stroke();

    } else {
      // neutral / warm
      ctx.fillStyle = accent; ctx.beginPath(); ctx.arc(lx, eye_y, eye_r, 0, Math.PI*2); ctx.fill();
      ctx.beginPath(); ctx.arc(rx, eye_y, eye_r, 0, Math.PI*2); ctx.fill();
      if (state.mood==1) { ctx.fillStyle = strong; ctx.beginPath(); ctx.arc(lx-5, eye_y-6, 4, 0, Math.PI*2); ctx.fill(); ctx.beginPath(); ctx.arc(rx-5, eye_y-6, 4, 0, Math.PI*2); ctx.fill(); }
    }

    // mouth
    const mouthY = CY + 30 + boff;
    ctx.fillStyle = accent;
    if (state.mood==0) {
      ctx.fillRect(CX-22, mouthY, 44, 3);
    } else if (state.mood==1) {
      for (let x=-28;x<=28;x++){ const yo = Math.round(12*Math.sin((x+28)*Math.PI/56)); ctx.fillRect(CX+x, mouthY+yo, 1, 2); }
    } else if (state.mood==3 || state.mood==5) {
      for (let x=-36;x<=36;x++){ const yo = Math.round(18*Math.sin((x+36)*Math.PI/72)); ctx.fillRect(CX+x, mouthY+yo, 1, 2); }
    } else if (state.mood==4) {
      for (let x=-28;x<=28;x++){ const yo = Math.round(-12*Math.sin((x+28)*Math.PI/56)); ctx.fillRect(CX+x, mouthY+14+yo, 1, 2); }
    } else if (state.mood==2) {
      ctx.beginPath(); ctx.arc(CX, mouthY+8, 14, 0, Math.PI*2); ctx.fill(); ctx.fillStyle = bg; ctx.beginPath(); ctx.arc(CX, mouthY+8, 9, 0, Math.PI*2); ctx.fill();
    }

    // cheeks
    if (state.mood==1 || state.mood==3 || state.mood==5) {
      const blush = blendHex(bg, blendHex(accent, themes.chill.moodRing, 0.3), 0.28);
      ctx.fillStyle = blush; ctx.beginPath(); ctx.arc(CX-64, eye_y+22, 14, 0, Math.PI*2); ctx.fill(); ctx.beginPath(); ctx.arc(CX+64, eye_y+22, 14, 0, Math.PI*2); ctx.fill();
    }
  }

  // Reaction overlays (only hearts implemented in preview)
  let reactionStart = performance.now();
  function drawReaction() {
    if (state.reaction==='hearts') {
      const age = (performance.now()-reactionStart)/1000;
      for (let i=0;i<3;i++){
        const off = i*30;
        const rise = (age + i*0.33) % 1.0;
        const hy = CY + 30 - Math.round(rise*80);
        ctx.fillStyle = '#ff6b88'; ctx.beginPath(); ctx.arc(CX-30 + i*30, hy, 4, 0, Math.PI*2); ctx.fill();
      }
    } else if (state.reaction==='startled') {
      const age = (performance.now()-reactionStart)/1000;
      if (Math.floor(age*4)%2==0) { ctx.strokeStyle='#ffcc00'; ctx.lineWidth=3; ctx.beginPath(); ctx.arc(CX, CY, 100, 0, Math.PI*2); ctx.stroke(); }
    } else if (state.reaction==='stars') {
      const age = (performance.now()-reactionStart)/1000;
      for (let i=0;i<4;i++){ const angle = age*0.004 + i*Math.PI/2; const sx = CX + Math.cos(angle)*50; const sy = CY -10 + Math.sin(angle)*35; ctx.fillStyle = '#fff'; ctx.fillRect(sx-1, sy-1, 2,2); }
    }
  }

  function draw() {
    clear();
    const modeBlend = (state.drive==='sport')?1:0;
    const bgOut = blendHex(themes.chill.bg, themes.sport.bg, modeBlend);
    // subtle background
    ctx.fillStyle = '#051019'; ctx.fillRect(0,0,W,H);

    drawRing(state.progress, state.rpm, modeBlend);
    drawFace();
    drawReaction();
  }

  function update(dt){
    state.breath += dt*0.006;
    // keep reaction start time aligned when reaction selected
  }

  // UI wiring
  ui.driveMode.addEventListener('change', e=> state.drive=e.target.value );
  ui.mood.addEventListener('change', e=> state.mood=Number(e.target.value));
  ui.rpm.addEventListener('input', e=> state.rpm=Number(e.target.value));
  ui.sleep.addEventListener('change', e=> state.sleeping=e.target.checked);
  ui.reaction.addEventListener('change', e=> { state.reaction=e.target.value; reactionStart = performance.now(); });
  ui.progress.addEventListener('input', e=> state.progress=Number(e.target.value)/100);

  // animation loop (~30FPS throttle)
  let last = performance.now(); let acc = 0;
  function loop(now){
    const dt = now - last; last = now; acc += dt;
    if (acc >= 33) { update(acc); draw(); acc = 0; }
    requestAnimationFrame(loop);
  }
  requestAnimationFrame(loop);

  // initial fill
  draw();
})();
