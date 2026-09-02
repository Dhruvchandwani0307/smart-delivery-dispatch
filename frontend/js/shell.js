/* ============================================================================
   shell.js
   Renders the shared sidebar (rail) + topbar markup into #shellRoot on every
   dashboard page, based on the signed-in role. Kept separate from utils.js
   so each page's own script only has to call Shell.render({...}) once.
   ============================================================================ */

const Shell = (() => {

  const NAV_BY_ROLE = {
    ADMIN: [
      { href: 'admin.html', label: 'Overview', icon: 'grid' },
      { href: 'dispatch.html', label: 'Dispatch Center', icon: 'radar' },
      { href: 'analytics.html', label: 'Analytics', icon: 'chart' },
    ],
    CUSTOMER: [
      { href: 'customer.html', label: 'My Orders', icon: 'bag' },
    ],
    DELIVERY_AGENT: [
      { href: 'agent.html', label: 'My Deliveries', icon: 'bike' },
    ],
  };

  const ICONS = {
    grid: '<rect x="3" y="3" width="7" height="7" rx="1.5"/><rect x="14" y="3" width="7" height="7" rx="1.5"/><rect x="3" y="14" width="7" height="7" rx="1.5"/><rect x="14" y="14" width="7" height="7" rx="1.5"/>',
    radar: '<circle cx="12" cy="12" r="9"/><circle cx="12" cy="12" r="4.5"/><path d="M12 3v4M12 17v4M3 12h4M17 12h4"/>',
    chart: '<path d="M4 20V10M12 20V4M20 20v-7"/>',
    bag: '<path d="M6 8h12l-1 12H7L6 8Z"/><path d="M9 8V6a3 3 0 0 1 6 0v2"/>',
    bike: '<circle cx="5.5" cy="17.5" r="2.5"/><circle cx="18.5" cy="17.5" r="2.5"/><path d="M15 6h-2v8l3.5 3.5M8 17.5h6M5.5 15 8 8h5"/>',
  };

  function icon(name) {
    return `<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round">${ICONS[name] || ''}</svg>`;
  }

  function render({ activeHref, title, subtitle }) {
    const session = Utils.requireSession();
    if (!session) return null;

    const links = NAV_BY_ROLE[session.role] || [];
    const navHtml = links.map(l => `
      <a class="rail-link ${l.href === activeHref ? 'active' : ''}" href="${l.href}">
        ${icon(l.icon)}<span>${l.label}</span>
      </a>`).join('');

    const root = document.getElementById('shellRoot');
    root.innerHTML = `
      <div class="rail-scrim"></div>
      <aside class="rail">
        <div class="rail-brand">
          <div class="rail-brand-mark">SD</div>
          <div class="rail-brand-text">
            <div class="name">Smart Dispatch</div>
            <div class="tag">Ops Console</div>
          </div>
        </div>

        <div class="rail-section-label">Menu</div>
        <nav class="rail-nav">${navHtml}</nav>

        <div class="rail-footer">
          <div class="rail-user">
            <div class="rail-user-avatar" data-user-avatar></div>
            <div>
              <div class="rail-user-name" data-user-name></div>
              <div class="rail-user-role" data-user-role></div>
            </div>
          </div>
          <button class="rail-logout" data-logout>Sign out</button>
        </div>
      </aside>

      <div class="main-col">
        <header class="topbar">
          <div class="row" style="gap:12px;">
            <button class="hamburger" aria-label="Open menu">
              <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><path d="M4 6h16M4 12h16M4 18h16"/></svg>
            </button>
            <div class="topbar-title">
              <h1>${title || ''}</h1>
              ${subtitle ? `<div class="sub">${subtitle}</div>` : ''}
            </div>
          </div>
          <div class="topbar-actions" id="topbarActions">
            <span class="badge badge-teal"><span class="dot"></span>Live</span>
          </div>
        </header>

        <main class="page-content" id="pageContent"></main>
      </div>
    `;

    Utils.initShell();
    return session;
  }

  return { render };
})();
