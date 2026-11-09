// Shows the newest release (including pre-releases) next to the install link.
// Purely decorative: the page works without it.
(async () => {
  const targets = document.querySelectorAll("[data-latest]")
  if (!targets.length) return
  try {
    const res = await fetch("https://api.github.com/repos/adrianoamalfi/ripple/releases?per_page=1", { headers: { Accept: "application/vnd.github+json" } })
    if (!res.ok) return
    const [rel] = await res.json()
    if (!rel) return
    const text = `${rel.tag_name}${rel.prerelease ? " · pre-release" : ""}`
    targets.forEach((el) => { el.textContent = text; el.hidden = false })
  } catch { /* offline or rate limited: leave it hidden */ }
})()
