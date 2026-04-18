(function(){
  const canvas = document.getElementById('orb');
  const ctx = canvas.getContext('2d');
  const W = canvas.width, H = canvas.height, CX = W/2, CY = H/2;

  const themes = {
    chill: { bg: '#071826', accent: '#49c7ff', accentStrong: '#86e6ff', moodRing: '#3fa8ff', accentDim: '#2b6f90' },
    sport: { bg: '#20130c', accent: '#ff8a36', accentStrong: '#ffbf7a', moodRing: '#ffcf33', accentDim: '#7a3b1a' }
  };

  // UI elements
  const ui = {
    speed: document.getElementById('speed'),
    rpm: document.getElementById('rpm'),
    fuel: document.getElementById('fuel'),
    mood: document.getElementById('mood'),
    sleep: document.getElementById('sleep'),
    progress: document.getElementById('progress'),
    driveMode: document.querySelectorAll('input[name="driveMode"]'),
    speedValue: document.getElementById('speedValue'),
    rpmValue: document.getElementById('rpmValue')
  };

  // State
  const state = {
    speed: 0,
    rpm: 0,
    fuel: 60,
    mood: 0,
    sleeping: false,
    progress: 0.3,
    drive: 'chill',
    breath: 0
  };

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

  function clear() {
    ctx.clearRect(0,0,W,H);
  }

  // Draw the outer ring (XP progress + RPM indicator)
  function drawRing(progress, rpmNorm, modeBlend){
    const base = blendHex(themes.chill.moodRing, themes.sport.moodRing, modeBlend);
    const dim = blendHex(themes.chill.bg, themes.sport.bg, modeBlend);
    const pulse = 0.5 + 0.5*Math.sin(performance.now()* (0.002 + rpmNorm*0.012));
    const trackColor = blendHex(dim, base, 0.08 + 0.12*pulse);

    // track circle
    ctx.lineWidth = 10;
    ctx.strokeStyle = trackColor;
    ctx.beginPath();
    ctx.arc(CX, CY, 116, 0, Math.PI*2);
    ctx.stroke();

    // XP progress arc
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

  // Draw the pet face
  function drawFace(){
    const modeBlend = (state.drive==='sport') ? 1 : 0;
    const bg = blendHex(themes.chill.bg, themes.sport.bg, modeBlend);
    const accent = blendHex(themes.chill.accent, themes.sport.accent, modeBlend);
    const strong = blendHex(themes.chill.accentStrong, themes.sport.accentStrong, modeBlend);

    // interior circle / face background
    ctx.fillStyle = bg;
    ctx.beginPath(); 
    ctx.arc(CX, CY, 105, 0, Math.PI*2); 
    ctx.fill();

    // Dark split-horizon top shade
    const topGradient = ctx.createLinearGradient(0, CY - 105, 0, CY - 40);
    topGradient.addColorStop(0, 'rgba(0,0,0,0.6)');
    topGradient.addColorStop(1, 'rgba(0,0,0,0)');
    ctx.fillStyle = topGradient;
    ctx.beginPath();
    ctx.arc(CX, CY, 105, Math.PI, Math.PI*2);
    ctx.fill();

    const breath = 0.5 + 0.5*Math.sin(state.breath);
    const boff = Math.round(breath*2);

    // Shift eyes and mouth slightly down to balance the heavy top UI
    const yOffset = 8;

    // Eyes — small dots
    const eye_spacing = 38;
    const dot_r = 5;
    const eye_y = CY - 20 + boff + yOffset;
    const lx = CX - eye_spacing;
    const rx = CX + eye_spacing;

    if (state.sleeping) {
      // closed: thin horizontal lines
      ctx.fillStyle = accent;
      ctx.fillRect(lx - dot_r, eye_y, dot_r * 2, 2);
      ctx.fillRect(rx - dot_r, eye_y, dot_r * 2, 2);
    } else {
      // open eyes: dots
      ctx.fillStyle = accent;
      ctx.beginPath(); ctx.arc(lx, eye_y, dot_r, 0, Math.PI * 2); ctx.fill();
      ctx.beginPath(); ctx.arc(rx, eye_y, dot_r, 0, Math.PI * 2); ctx.fill();
      
      // glint for happy/excited
      if (state.mood == 1 || state.mood == 5) {
        ctx.fillStyle = strong;
        ctx.beginPath(); ctx.arc(lx - 2, eye_y - 2, 2, 0, Math.PI * 2); ctx.fill();
        ctx.beginPath(); ctx.arc(rx - 2, eye_y - 2, 2, 0, Math.PI * 2); ctx.fill();
      }
    }

    // Mouth
    const mouthY = CY + 28 + boff + yOffset;
    ctx.fillStyle = accent;
    if (state.mood==0) {
      // neutral line
      ctx.fillRect(CX-20, mouthY, 40, 3);
    } else if (state.mood==1) {
      // smile
      for (let x=-26;x<=26;x++){ const yo = Math.round(10*Math.sin((x+26)*Math.PI/52)); ctx.fillRect(CX+x, mouthY+yo, 1, 2); }
    } else if (state.mood==3 || state.mood==5) {
      // big smile
      for (let x=-32;x<=32;x++){ const yo = Math.round(16*Math.sin((x+32)*Math.PI/64)); ctx.fillRect(CX+x, mouthY+yo, 1, 2); }
    } else if (state.mood==4) {
      // sad
      for (let x=-26;x<=26;x++){ const yo = Math.round(-10*Math.sin((x+26)*Math.PI/52)); ctx.fillRect(CX+x, mouthY+12+yo, 1, 2); }
    } else if (state.mood==2) {
      // alert: open oval
      ctx.beginPath(); ctx.arc(CX, mouthY+6, 12, 0, Math.PI*2); ctx.fill(); 
      ctx.fillStyle = bg; ctx.beginPath(); ctx.arc(CX, mouthY+6, 8, 0, Math.PI*2); ctx.fill();
    }

    // Cheeks blush
    if (state.mood==1 || state.mood==3 || state.mood==5) {
      const blush = blendHex(bg, blendHex(accent, themes.chill.moodRing, 0.3), 0.28);
      ctx.fillStyle = blush; 
      ctx.beginPath(); ctx.arc(CX-60, eye_y+20, 13, 0, Math.PI*2); ctx.fill(); 
      ctx.beginPath(); ctx.arc(CX+60, eye_y+20, 13, 0, Math.PI*2); ctx.fill();
    }
  }

  // Draw RPM speed bar at bottom
  function drawSpeedBar(){
    const modeBlend = (state.drive==='sport') ? 1 : 0;
    const bar = blendHex(themes.chill.moodRing, themes.sport.moodRing, modeBlend);
    const maxRpm = 7000;
    const rpmRatio = Math.min(1, state.rpm / maxRpm);
    
    // Bar background
    ctx.fillStyle = blendHex(themes.chill.bg, themes.sport.bg, modeBlend);
    ctx.fillRect(CX-55, CY+52, 110, 12);
    
    // RPM fill
    ctx.fillStyle = bar;
    ctx.fillRect(CX-55, CY+52, 110 * rpmRatio, 12);
  }

  function draw() {
    clear();
    const modeBlend = (state.drive==='sport')?1:0;
    
    // subtle background
    ctx.fillStyle = '#051019';
    ctx.fillRect(0,0,W,H);

    const rpmNorm = Math.min(1, state.rpm / 7000);
    drawRing(state.progress, rpmNorm, modeBlend);
    drawFace();
    drawSpeedBar();

    // Update telemetry display
    ui.speedValue.textContent = Math.round(state.speed);
    ui.rpmValue.textContent = Math.round(state.rpm);
  }

  function update(dt){
    state.breath += dt*0.006;
  }

  // UI event listeners
  ui.speed.addEventListener('input', e => state.speed = Number(e.target.value));
  ui.rpm.addEventListener('input', e => state.rpm = Number(e.target.value));
  ui.fuel.addEventListener('input', e => state.fuel = Number(e.target.value));
  ui.mood.addEventListener('change', e => state.mood = Number(e.target.value));
  ui.sleep.addEventListener('change', e => state.sleeping = e.target.checked);
  ui.progress.addEventListener('input', e => state.progress = Number(e.target.value)/100);
  
  ui.driveMode.forEach(radio => {
    radio.addEventListener('change', e => {
      if (e.target.checked) state.drive = e.target.value;
    });
  });

  // Animation loop (~30FPS throttle)
  let last = performance.now();
  let acc = 0;
  function loop(now){
    const dt = now - last;
    last = now;
    acc += dt;
    if (acc >= 33) {
      update(acc);
      draw();
      acc = 0;
    }
    requestAnimationFrame(loop);
  }
  requestAnimationFrame(loop);

  // Initial draw
  draw();
})();
