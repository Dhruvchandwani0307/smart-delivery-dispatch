/* ============================================================================
   dispatch.js
   Dispatch Center: available agents on the left, orders waiting for dispatch
   on the right. Selecting an order calls GET /api/dispatch/:orderId for a
   full, transparent score breakdown per candidate agent, and highlights the
   recommended one. Auto Assign performs the real assignment through the
   C++ DispatchEngine.
   ============================================================================ */

const DispatchCenter = (() => {
  let selectedOrderId = null;
  let queueOrders = [];
  let availableAgents = [];

  function pageTemplate() {
    return `
      <div class="row-between mb-16">
        <div class="desc" style="font-size:13px;color:var(--ink-soft);max-width:640px;">
          Pick an order waiting for dispatch to see the live, weighted score breakdown for every
          eligible agent — the same calculation the C++ <span class="mono">DispatchEngine</span> uses.
        </div>
        <button class="btn btn-accent" id="autoDispatchAllBtn">
          <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><path d="M13 2 3 14h7l-1 8 11-14h-7l1-6Z"/></svg>
          Auto Dispatch All
        </button>
      </div>

      <div class="dispatch-columns">
        <div class="card">
          <div class="card-header"><h3>Available Agents</h3><span class="badge badge-teal" id="agentCountBadge"></span></div>
          <div class="card-body tight" id="agentList" style="max-height:560px;overflow-y:auto;"></div>
        </div>

        <div class="stack">
          <div class="card">
            <div class="card-header"><h3>Orders Waiting for Dispatch</h3><span class="badge badge-accent" id="queueCountBadge"></span></div>
            <div class="card-body tight" id="queueList" style="max-height:280px;overflow-y:auto;"></div>
          </div>

          <div class="card">
            <div class="card-header"><h3>Candidate Score Breakdown</h3><span id="selectedOrderBadge"></span></div>
            <div class="card-body" id="candidatePanel">
              <div class="state-block">
                <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.6"><circle cx="12" cy="12" r="9"/><path d="M12 8v5M12 16h.01"/></svg>
                <h4>No order selected</h4>
                <p>Choose an order from the queue on the left to see agent candidates and their scores.</p>
              </div>
            </div>
          </div>
        </div>
      </div>
    `;
  }

  function agentRowHtml(a) {
    return `
      <div class="row-between" style="padding:12px 20px;border-bottom:1px solid var(--line-soft);">
        <div class="row">
          <div class="avatar-sm">${Utils.initials(a.name)}</div>
          <div>
            <div class="cell-primary" style="font-size:13px;">${Utils.escapeHtml(a.name)}</div>
            <div class="cell-sub">${a.vehicle.type} · Zone ${a.location.zone.replace('Zone-', '')} · ★${a.rating.toFixed(1)}</div>
          </div>
        </div>
        <div class="text-right">
          <div class="mono" style="font-size:12px;">${a.activeOrders}/${a.maxCapacity}</div>
          <div class="cell-sub">active</div>
        </div>
      </div>
    `;
  }

  function queueItemHtml(o) {
    const active = o.id === selectedOrderId;
    return `
      <button class="w-full" data-select-order="${o.id}" style="display:block;text-align:left;background:${active ? 'var(--accent-dim)' : 'transparent'};border:none;border-bottom:1px solid var(--line-soft);padding:12px 20px;cursor:pointer;">
        <div class="row-between">
          <span class="cell-id">${o.id}</span>
          ${Utils.priorityBadge(o.priority)}
        </div>
        <div class="row-between mt-8">
          <span class="cell-sub">${Utils.escapeHtml(o.restaurantName || '')}</span>
          <span class="cell-sub mono">waiting ${o.secondsSinceReady}s</span>
        </div>
      </button>
    `;
  }

  function scorePanelHtml(c) {
    const pct = (v) => Math.max(2, Math.min(100, v));
    return `
      <div class="score-panel ${c.recommended ? 'recommended' : ''}">
        <div class="score-panel-head">
          <div class="score-panel-agent">
            <div class="score-panel-avatar">${Utils.initials(c.agentName)}</div>
            <div>
              <div class="name">${Utils.escapeHtml(c.agentName)} ${c.recommended ? '<span class="recommended-flag">Recommended</span>' : ''}</div>
              <div class="sub">${c.distanceKm.toFixed(2)} km away</div>
            </div>
          </div>
          <div class="score-panel-final">${c.finalScore.toFixed(1)}</div>
        </div>
        <div class="score-rows">
          <div class="score-row"><span class="k">Distance</span><div class="bar-track"><div class="bar-fill distance" style="width:${pct(c.distanceScore)}%"></div></div><span class="v">${c.distanceScore.toFixed(0)}</span></div>
          <div class="score-row"><span class="k">Workload</span><div class="bar-track"><div class="bar-fill workload" style="width:${pct(c.workloadScore)}%"></div></div><span class="v">${c.workloadScore.toFixed(0)}</span></div>
          <div class="score-row"><span class="k">Rating</span><div class="bar-track"><div class="bar-fill rating" style="width:${pct(c.ratingScore)}%"></div></div><span class="v">${c.ratingScore.toFixed(0)}</span></div>
          <div class="score-row"><span class="k">Priority + Zone</span><div class="bar-track"><div class="bar-fill" style="width:${pct(c.priorityAdjustment + c.zoneBonus)}%"></div></div><span class="v">+${(c.priorityAdjustment + c.zoneBonus).toFixed(0)}</span></div>
        </div>
      </div>
    `;
  }

  async function selectOrder(orderId) {
    selectedOrderId = orderId;
    renderQueue();
    document.getElementById('selectedOrderBadge').innerHTML = `<span class="badge badge-neutral mono">${orderId}</span>`;
    const panel = document.getElementById('candidatePanel');
    panel.innerHTML = `<div class="state-block"><div class="spinner"></div></div>`;

    try {
      const res = await Api.getDispatchCandidates(orderId);
      if (res.candidates.length === 0) {
        panel.innerHTML = `<div class="state-block">
          <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.6"><path d="M12 9v4M12 17h.01"/><circle cx="12" cy="12" r="9"/></svg>
          <h4>No suitable delivery agent available</h4>
          <p>Every agent is offline or already at maximum capacity for this order.</p>
        </div>`;
        return;
      }
      panel.innerHTML = `
        <div class="stack">${res.candidates.map(scorePanelHtml).join('')}</div>
        <button class="btn btn-accent btn-block mt-24" id="dispatchThisOrderBtn">
          <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><path d="M13 2 3 14h7l-1 8 11-14h-7l1-6Z"/></svg>
          Auto Assign — dispatch to ${Utils.escapeHtml(res.candidates[0].agentName)}
        </button>`;
      document.getElementById('dispatchThisOrderBtn').addEventListener('click', () => dispatchSelected());
    } catch (e) {
      panel.innerHTML = `<div class="state-block"><h4>Could not load candidates</h4><p>${Utils.escapeHtml(e.message)}</p></div>`;
    }
  }

  async function dispatchSelected() {
    if (!selectedOrderId) return;
    try {
      const res = await Api.dispatchOrder(selectedOrderId);
      Utils.toast(res.dispatch.message, 'success');
      selectedOrderId = null;
      await loadAll();
      document.getElementById('candidatePanel').innerHTML = `<div class="state-block">
        <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.6"><path d="m9 12 2 2 4-4"/><circle cx="12" cy="12" r="9"/></svg>
        <h4>Order dispatched</h4><p>Pick the next order from the queue.</p></div>`;
      document.getElementById('selectedOrderBadge').innerHTML = '';
    } catch (e) {
      Utils.toast(e.message, 'error');
    }
  }

  function renderQueue() {
    const list = document.getElementById('queueList');
    document.getElementById('queueCountBadge').textContent = queueOrders.length;
    if (queueOrders.length === 0) {
      list.innerHTML = `<div class="state-block">
        <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.6"><path d="m9 12 2 2 4-4"/><circle cx="12" cy="12" r="9"/></svg>
        <h4>Queue is empty</h4><p>No orders are currently waiting for dispatch.</p></div>`;
      return;
    }
    list.innerHTML = queueOrders.map(queueItemHtml).join('');
    list.querySelectorAll('[data-select-order]').forEach(btn =>
      btn.addEventListener('click', () => selectOrder(btn.dataset.selectOrder)));
  }

  function renderAgents() {
    const list = document.getElementById('agentList');
    document.getElementById('agentCountBadge').textContent = availableAgents.length;
    if (availableAgents.length === 0) {
      list.innerHTML = `<div class="state-block">
        <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.6"><circle cx="12" cy="12" r="9"/><path d="M12 8v5M12 16h.01"/></svg>
        <h4>No agents available</h4><p>All agents are offline or at capacity.</p></div>`;
      return;
    }
    list.innerHTML = availableAgents.map(agentRowHtml).join('');
  }

  async function loadAll() {
    try {
      const [queueRes, agentsRes] = await Promise.all([Api.getDispatchQueue(), Api.getAgents({ status: 'AVAILABLE' })]);
      queueOrders = queueRes.queue;
      availableAgents = agentsRes.agents;
      renderQueue();
      renderAgents();
    } catch (e) {
      Utils.toast(e.message, 'error');
    }
  }

  async function autoDispatchAll() {
    try {
      const res = await Api.autoDispatchAll();
      Utils.toast(`Auto dispatch complete — ${res.assignedCount} order(s) assigned`, res.assignedCount > 0 ? 'success' : 'info');
      selectedOrderId = null;
      await loadAll();
    } catch (e) { Utils.toast(e.message, 'error'); }
  }

  function init() {
    const session = Shell.render({ activeHref: 'dispatch.html', title: 'Dispatch Center', subtitle: 'Transparent, weighted agent selection' });
    if (!session) return;
    document.getElementById('pageContent').innerHTML = pageTemplate();
    document.getElementById('autoDispatchAllBtn').addEventListener('click', autoDispatchAll);
    loadAll();
    setInterval(loadAll, 8000);
  }

  return { init };
})();

document.addEventListener('DOMContentLoaded', DispatchCenter.init);
