/* ============================================================================
   utils.js
   Shared helpers used across every page: currency/date formatting, the
   session ("logged in as") helper backed by sessionStorage (demo auth only -
   see README "Demo Mode"), toast notifications, and small DOM utilities.
   ============================================================================ */

const Utils = (() => {

  // ---- Formatting ----------------------------------------------------------
  function formatCurrency(amount) {
    return '₹' + Number(amount || 0).toLocaleString('en-IN', { maximumFractionDigits: 0 });
  }

  function formatMinutes(mins) {
    const m = Math.round(Number(mins || 0));
    return m + ' min';
  }

  function timeAgo(dateLike) {
    const diffSec = Math.max(0, Math.round((Date.now() - new Date(dateLike).getTime()) / 1000));
    if (diffSec < 60) return diffSec + 's ago';
    if (diffSec < 3600) return Math.floor(diffSec / 60) + 'm ago';
    return Math.floor(diffSec / 3600) + 'h ago';
  }

  function initials(name) {
    if (!name) return '?';
    const parts = name.trim().split(/\s+/);
    return ((parts[0]?.[0] || '') + (parts[1]?.[0] || '')).toUpperCase();
  }

  function escapeHtml(str) {
    const div = document.createElement('div');
    div.textContent = str ?? '';
    return div.innerHTML;
  }

  // ---- Status / priority presentation --------------------------------------
  const STATUS_META = {
    PLACED:            { label: 'Placed',           badge: 'badge-neutral' },
    CONFIRMED:         { label: 'Confirmed',         badge: 'badge-blue' },
    PREPARING:         { label: 'Preparing',         badge: 'badge-amber' },
    READY:             { label: 'Ready',             badge: 'badge-accent' },
    ASSIGNED:          { label: 'Assigned',          badge: 'badge-violet' },
    PICKED_UP:         { label: 'Picked Up',         badge: 'badge-blue' },
    OUT_FOR_DELIVERY:  { label: 'Out for Delivery',  badge: 'badge-teal' },
    DELIVERED:         { label: 'Delivered',         badge: 'badge-teal' },
    CANCELLED:         { label: 'Cancelled',         badge: 'badge-red' },
  };

  const PRIORITY_META = {
    NORMAL:     { label: 'Normal',     badge: 'badge-neutral' },
    HIGH:       { label: 'High',       badge: 'badge-amber' },
    URGENT:     { label: 'Urgent',     badge: 'badge-red' },
    PERISHABLE: { label: 'Perishable', badge: 'badge-violet' },
  };

  const AGENT_STATUS_META = {
    AVAILABLE: { label: 'Available', badge: 'badge-teal' },
    BUSY:      { label: 'Busy',      badge: 'badge-amber' },
    OFFLINE:   { label: 'Offline',   badge: 'badge-neutral' },
  };

  function statusBadge(status) {
    const m = STATUS_META[status] || { label: status, badge: 'badge-neutral' };
    return `<span class="badge ${m.badge}"><span class="dot"></span>${m.label}</span>`;
  }

  function priorityBadge(priority) {
    const m = PRIORITY_META[priority] || { label: priority, badge: 'badge-neutral' };
    return `<span class="badge ${m.badge}"><span class="dot"></span>${m.label}</span>`;
  }

  function agentStatusBadge(status) {
    const m = AGENT_STATUS_META[status] || { label: status, badge: 'badge-neutral' };
    return `<span class="badge ${m.badge}"><span class="dot"></span>${m.label}</span>`;
  }

  // ---- Session (demo auth) --------------------------------------------------
  const SESSION_KEY = 'sdd_session';

  function setSession(session) {
    sessionStorage.setItem(SESSION_KEY, JSON.stringify(session));
  }

  function getSession() {
    try {
      return JSON.parse(sessionStorage.getItem(SESSION_KEY));
    } catch (e) {
      return null;
    }
  }

  function clearSession() {
    sessionStorage.removeItem(SESSION_KEY);
  }

  function requireSession(expectedRole) {
    const session = getSession();
    if (!session || (expectedRole && session.role !== expectedRole)) {
      window.location.href = 'index.html';
      return null;
    }
    return session;
  }

  // ---- Toasts ----------------------------------------------------------------
  function ensureToastStack() {
    let stack = document.querySelector('.toast-stack');
    if (!stack) {
      stack = document.createElement('div');
      stack.className = 'toast-stack';
      document.body.appendChild(stack);
    }
    return stack;
  }

  function toast(message, type = 'info', duration = 4200) {
    const stack = ensureToastStack();
    const el = document.createElement('div');
    el.className = `toast ${type}`;
    el.innerHTML = `<span class="dot"></span><span class="msg"></span><button class="close" aria-label="Dismiss">&times;</button>`;
    el.querySelector('.msg').textContent = message;
    el.querySelector('.close').addEventListener('click', () => el.remove());
    stack.appendChild(el);
    setTimeout(() => el.remove(), duration);
  }

  // ---- Modal helpers -----------------------------------------------------------
  function openModal(id) {
    document.getElementById(id)?.classList.add('open');
  }
  function closeModal(id) {
    document.getElementById(id)?.classList.remove('open');
  }

  // ---- Mobile drawer / sidebar toggle -----------------------------------------
  function initShell() {
    const rail = document.querySelector('.rail');
    const scrim = document.querySelector('.rail-scrim');
    const hamburger = document.querySelector('.hamburger');
    const openDrawer = () => { rail?.classList.add('open'); scrim?.classList.add('open'); };
    const closeDrawer = () => { rail?.classList.remove('open'); scrim?.classList.remove('open'); };
    hamburger?.addEventListener('click', openDrawer);
    scrim?.addEventListener('click', closeDrawer);

    // Close any open modal on Escape
    document.addEventListener('keydown', (e) => {
      if (e.key === 'Escape') {
        document.querySelectorAll('.modal-overlay.open').forEach(m => m.classList.remove('open'));
        closeDrawer();
      }
    });

    // Populate the user chip + logout button if present
    const session = getSession();
    if (session) {
      const nameEl = document.querySelector('[data-user-name]');
      const roleEl = document.querySelector('[data-user-role]');
      const avatarEl = document.querySelector('[data-user-avatar]');
      if (nameEl) nameEl.textContent = session.name;
      if (roleEl) roleEl.textContent = session.role.replace('_', ' ');
      if (avatarEl) avatarEl.textContent = initials(session.name);
    }
    document.querySelectorAll('[data-logout]').forEach(btn => {
      btn.addEventListener('click', () => { clearSession(); window.location.href = 'index.html'; });
    });
  }

  return {
    formatCurrency, formatMinutes, timeAgo, initials, escapeHtml,
    statusBadge, priorityBadge, agentStatusBadge,
    STATUS_META, PRIORITY_META, AGENT_STATUS_META,
    setSession, getSession, clearSession, requireSession,
    toast, openModal, closeModal, initShell,
  };
})();
