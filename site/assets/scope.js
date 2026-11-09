// The hero "signal scope": a toy model of what the board sees. Moving the pointer
// over it plays the part of a person in the room and disturbs the trace; when it
// rises above the dashed threshold, that's "motion". Nothing here is real data.
(() => {
  const panel = document.querySelector(".scope")
  const cv = document.getElementById("scope")
  if (!panel || !cv) return
  const ctx = cv.getContext("2d")
  const stateEl = panel.querySelector(".state")
  const btn = panel.querySelector("button")
  const reduce = matchMedia("(prefers-reduced-motion: reduce)").matches

  const N = 220                       // samples on screen
  const THR = 3.2                     // motion threshold, as in a calibrated room
  const vals = new Array(N).fill(0.7)
  let energy = 0                      // how much "someone" is moving
  let walkUntil = 0
  let last = null
  let w = 0, h = 0, dpr = 1, raf = 0, lastFrame = 0, motionHold = 0

  const css = (name) => getComputedStyle(document.documentElement).getPropertyValue(name).trim()

  function resize() {
    dpr = Math.min(devicePixelRatio || 1, 2)
    w = cv.clientWidth
    h = cv.clientHeight
    cv.width = Math.round(w * dpr)
    cv.height = Math.round(h * dpr)
    ctx.setTransform(dpr, 0, 0, dpr, 0, 0)
    draw()
  }

  function disturb(x, y, t) {
    if (last) {
      const dt = Math.max(1, t - last.t)
      const speed = Math.hypot(x - last.x, y - last.y) / dt     // px per ms
      energy = Math.min(energy + speed * 0.9, 7)
    }
    last = { x, y, t }
  }
  panel.addEventListener("pointermove", (e) => disturb(e.clientX, e.clientY, e.timeStamp))
  panel.addEventListener("pointerleave", () => { last = null })

  btn?.addEventListener("click", () => {
    walkUntil = performance.now() + 4500
    start()
  })

  function step(now) {
    if (now < walkUntil) {                         // a scripted walk-through
      const p = 1 - (walkUntil - now) / 4500
      energy = Math.max(energy, 7 * Math.sin(Math.PI * p) ** 0.8)
    }
    energy *= 0.955
    const noise = 0.65 + Math.random() * 0.55 + Math.sin(now / 900) * 0.12
    const burst = energy * (0.5 + Math.random() * 0.9)
    vals.push(Math.max(0.05, noise + burst))
    vals.shift()
  }

  function draw() {
    ctx.clearRect(0, 0, w, h)
    const max = 9
    const Y = (v) => h - 8 - (Math.min(v, max) / max) * (h - 16)
    const line = css("--line"), accent = css("--accent"), motion = css("--motion"), muted = css("--muted")

    ctx.strokeStyle = line
    ctx.lineWidth = 1
    ctx.setLineDash([])
    for (let g = 0; g <= 4; g++) {
      const y = 8 + ((h - 16) * g) / 4
      ctx.beginPath(); ctx.moveTo(0, y); ctx.lineTo(w, y); ctx.stroke()
    }

    // motion shading where the trace is above the threshold
    ctx.fillStyle = motion
    ctx.globalAlpha = 0.16
    vals.forEach((v, i) => { if (v > THR) ctx.fillRect((i / (N - 1)) * w, 0, w / (N - 1) + 1, h) })
    ctx.globalAlpha = 1

    // area under the trace
    ctx.beginPath()
    vals.forEach((v, i) => { const x = (i / (N - 1)) * w; i ? ctx.lineTo(x, Y(v)) : ctx.moveTo(x, Y(v)) })
    ctx.lineTo(w, h); ctx.lineTo(0, h); ctx.closePath()
    ctx.fillStyle = accent          // the colour tokens are oklch(): fade with alpha, not a hex suffix
    ctx.globalAlpha = 0.14
    ctx.fill()
    ctx.globalAlpha = 1

    ctx.beginPath()
    vals.forEach((v, i) => { const x = (i / (N - 1)) * w; i ? ctx.lineTo(x, Y(v)) : ctx.moveTo(x, Y(v)) })
    ctx.strokeStyle = accent
    ctx.lineWidth = 2
    ctx.lineJoin = "round"
    ctx.stroke()

    ctx.strokeStyle = muted
    ctx.lineWidth = 1.5
    ctx.setLineDash([6, 5])
    ctx.beginPath(); ctx.moveTo(0, Y(THR)); ctx.lineTo(w, Y(THR)); ctx.stroke()
    ctx.setLineDash([])
    ctx.fillStyle = muted
    ctx.font = "11px ui-monospace, monospace"
    ctx.fillText("threshold", 8, Y(THR) - 6)
  }

  function frame(now) {
    raf = requestAnimationFrame(frame)
    if (now - lastFrame < 33) return                // ~30 fps is plenty
    lastFrame = now
    step(now)
    draw()
    const hot = vals.slice(-8).some((v) => v > THR)
    if (hot) motionHold = now + 600
    const motion = now < motionHold
    if ((panel.dataset.state === "motion") !== motion) {
      panel.dataset.state = motion ? "motion" : "empty"
      stateEl.textContent = motion ? "MOTION" : "ROOM EMPTY"
    }
  }

  function start() { if (!raf) raf = requestAnimationFrame(frame) }
  function stop() { cancelAnimationFrame(raf); raf = 0 }

  new ResizeObserver(resize).observe(cv)
  matchMedia("(prefers-color-scheme: dark)").addEventListener("change", draw)
  new IntersectionObserver(([e]) => (e.isIntersecting && !reduce ? start() : stop())).observe(panel)
  resize()
  if (reduce) { for (let i = 0; i < N; i++) step(i * 33); draw() }   // a still frame, plus the button
})()
