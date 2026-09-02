/* ============================================================================
   dashboard.js
   Admin dashboard: top stats, live delivery table (filter + search + auto
   dispatch + manual assign/reassign), delivery-agent roster with
   availability toggles, and a restaurant roster.
   ============================================================================ */

const AdminDashboard = (() => {
  let allOrders = [];
  let allAgents = [];
  let allRestaurants = [];
  let currentFilter = 'ALL';
  let searchTerm = '';
  let manualAssignOrderId = null;

  const FILTERS = ['ALL', 'PREPARING', 'READY', 'ASSIGNED', 'OUT_FOR_DELIVERY', 'DELIVERED', 'CANCELLED'];
  const FILTER_LABELS = {
    ALL: 'All', PREPARING: 'Preparing', READY: 'Ready', ASSIGNED: 'Assigned',
    OUT_FOR_DELIVERY: 'Out for Delivery', DELIVERED: 'Delivered', CANCELLED: 'Cancelled',
  };

  function pageTemplate() {
    return `
      <div class="stat-grid mb-16" id="statGrid"></div>

      <div class="section-head">
        <div>
          <h2>Live Delivery Table</h2>
          <div class="desc">Every order in the system, updated live from the C++ backend.</div>
        </div>
        <div class="row gap-8">
          <button class="btn btn-outline btn-sm" id="refreshBtn">
            <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><path d="M4 4v6h6M20 20v-6h-6"/><path d="M20 10a8 8 0 0 0-14.9-3M4 14a8 8 0 0 0 14.9 3"/></svg>
            Refresh
          </button>
          <button class="btn btn-accent btn-sm" id="autoDispatchBtn">
            <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><path d="M13 2 3 14h7l-1 8 11-14h-7l1-6Z"/></svg>
            Auto Dispatch
          </button>
        </div>
      </div>

      <div class="card mb-16">
        <div class="card-header" style="flex-wrap:wrap; gap:12px;">
          <div class="chip-tabs" id="statusFilters"></div>
          <div class="search-box">
            <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><circle cx="11" cy="11" r="7"/><path d="m21 21-4.3-4.3"/></svg>
            <input type="text" id="orderSearch" placeholder="Search order ID, customer, restaurant...">
          </div>
        </div>
        <div class="card-body tight">
          <div class="table-wrap" id="ordersTableWrap"></div>
        </div>
      </div>

      <div class="section-head">
        <div>
          <h2>Delivery Agents</h2>
          <div class="desc">Toggle availability or jump to the Dispatch Center for a full breakdown.</div>
        </div>
        <a href="dispatch.html" class="btn btn-outline btn-sm">
          Open Dispatch Center
          <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><path d="M5 12h14M13 6l6 6-6 6"/></svg>
        </a>
      </div>
      <div class="grid grid-3" id="agentGrid"></div>

      <div class="section-head">
        <div>
          <h2>Restaurants</h2>
          <div class="desc">Partner outlets currently taking orders.</div>
        </div>
      </div>
      <div class="grid grid-3" id="restaurantGrid"></div>
    `;
  }

  function modalTemplate() {
    return `
      <div class="modal-overlay" id="assignModal">
        <div class="modal-box wide">
          <div class="modal-header">
            <h3>Manual Assign — <span class="mono" id="assignOrderId"></span></h3>
            <button class="modal-close" onclick="Utils.closeModal('assignModal')">&times;</button>
          </div>
          <div class="modal-body" id="assignModalBody">
            <div class="state-block"><div class="spinner"></div></div>
          </div>
        </div>
      </div>
    `;
  }

  // ---- Stats -----------------------------------------------------------------
  function renderStats(summary) {
    const cards = [
      { label: 'Total Orders', value: summary.totalOrders, color: 'var(--blue)' },
      { label: 'Active Deliveries', value: summary.activeDeliveries, color: 'var(--accent)' },
      { label: 'Available Agents', value: summary.availableAgents, color: 'var(--teal)' },
      { label: 'Delivered Today', value: summary.deliveredToday, color: 'var(--teal)' },
      { label: 'Avg Delivery Time', value: Utils.formatMinutes(summary.averageDeliveryTimeMinutes), color: 'var(--violet)' },
      { label: 'Cancellation Rate', value: summary.cancellationRate.toFixed(1) + '%', color: 'var(--red)' },
    ];
    document.getElementById('statGrid').innerHTML = cards.map(c => `
      <div class="stat-card">
        <div class="accent-bar" style="background:${c.color}"></div>
        <div class="label">${c.label}</div>
        <div class="value">${c.value}</div>
      </div>
    `).join('');
  }

  // ---- Orders table ------------------------------------------------------------
  function orderActionsHtml(o) {
    const actions = [];
    if (o.status === 'READY') {
      actions.push(`<button class="btn btn-accent btn-sm" data-dispatch="${o.id}">Dispatch</button>`);
      actions.push(`<button class="btn btn-outline btn-sm" data-assign="${o.id}">Manual</button>`);
    } else if (o.status === 'ASSIGNED' || o.status === 'PICKED_UP' || o.status === 'OUT_FOR_DELIVERY') {
      actions.push(`<button class="btn btn-outline btn-sm" data-assign="${o.id}">Reassign</button>`);
    }
    if (['PLACED', 'CONFIRMED', 'PREPARING', 'READY'].includes(o.status)) {
      actions.push(`<button class="btn btn-danger btn-sm" data-cancel="${o.id}">Cancel</button>`);
    }
    return `<div class="row gap-8" style="flex-wrap:wrap;">${actions.join('')}</div>`;
  }

  function renderOrdersTable() {
    let filtered = currentFilter === 'ALL' ? allOrders : allOrders.filter(o => o.status === currentFilter);
    if (searchTerm) {
      const t = searchTerm.toLowerCase();
      filtered = filtered.filter(o =>
        o.id.toLowerCase().includes(t) ||
        (o.customerName || '').toLowerCase().includes(t) ||
        (o.restaurantName || '').toLowerCase().includes(t));
    }

    const wrap = document.getElementById('ordersTableWrap');
    if (filtered.length === 0) {
      wrap.innerHTML = `
        <div class="state-block">
          <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.6"><circle cx="11" cy="11" r="7"/><path d="m21 21-4.3-4.3"/></svg>
          <h4>No matching orders</h4>
          <p>Try a different filter or search term.</p>
        </div>`;
      return;
    }

    wrap.innerHTML = `
      <table class="data-table responsive-cards">
        <thead><tr>
          <th>Order ID</th><th>Restaurant</th><th>Customer</th><th>Agent</th>
          <th>Priority</th><th>Status</th><th>ETA</th><th>Actions</th>
        </tr></thead>
        <tbody>
          ${filtered.map(o => `
            <tr>
              <td data-label="Order ID" class="cell-id">${o.id}</td>
              <td data-label="Restaurant">${Utils.escapeHtml(o.restaurantName || '—')}</td>
              <td data-label="Customer">${Utils.escapeHtml(o.customerName || '—')}</td>
              <td data-label="Agent">${o.agentName ? Utils.escapeHtml(o.agentName) : '<span class="text-faint">Unassigned</span>'}</td>
              <td data-label="Priority">${Utils.priorityBadge(o.priority)}</td>
              <td data-label="Status">${Utils.statusBadge(o.status)}</td>
              <td data-label="ETA" class="mono">${o.status === 'DELIVERED' || o.status === 'CANCELLED' ? '—' : Utils.formatMinutes(o.estimatedEtaMinutes)}</td>
              <td data-label="Actions">${orderActionsHtml(o)}</td>
            </tr>`).join('')}
        </tbody>
      </table>`;

    wrap.querySelectorAll('[data-dispatch]').forEach(btn => btn.addEventListener('click', () => dispatchOrder(btn.dataset.dispatch)));
    wrap.querySelectorAll('[data-assign]').forEach(btn => btn.addEventListener('click', () => openAssignModal(btn.dataset.assign)));
    wrap.querySelectorAll('[data-cancel]').forEach(btn => btn.addEventListener('click', () => cancelOrder(btn.dataset.cancel)));
  }

  function renderFilters() {
    const el = document.getElementById('statusFilters');
    el.innerHTML = FILTERS.map(f => `<button class="chip-tab ${f === currentFilter ? 'active' : ''}" data-filter="${f}">${FILTER_LABELS[f]}</button>`).join('');
    el.querySelectorAll('[data-filter]').forEach(btn => btn.addEventListener('click', () => {
      currentFilter = btn.dataset.filter;
      renderFilters();
      renderOrdersTable();
    }));
  }

  // ---- Agents grid ---------------------------------------------------------------
  function renderAgents() {
    const grid = document.getElementById('agentGrid');
    grid.innerHTML = allAgents.map(a => `
      <div class="card">
        <div class="card-body">
          <div class="row-between mb-16">
            <div class="row">
              <div class="avatar-sm" style="width:34px;height:34px;font-size:12px;">${Utils.initials(a.name)}</div>
              <div>
                <div class="cell-primary">${Utils.escapeHtml(a.name)}</div>
                <div class="cell-sub">${a.vehicle.type} · ${a.vehicle.registrationNumber}</div>
              </div>
            </div>
            ${Utils.agentStatusBadge(a.status)}
          </div>
          <div class="row-between" style="font-size:12px;color:var(--ink-soft);">
            <span>★ ${a.rating.toFixed(1)}</span>
            <span>${a.activeOrders}/${a.maxCapacity} active</span>
            <span>${a.totalDeliveries} total</span>
          </div>
          <div class="divider"></div>
          <div class="row gap-8">
            <button class="btn btn-sm ${a.status === 'AVAILABLE' ? 'btn-primary' : 'btn-outline'}" data-avail="${a.id}" data-status="AVAILABLE" style="flex:1;">Available</button>
            <button class="btn btn-sm ${a.status === 'OFFLINE' ? 'btn-primary' : 'btn-outline'}" data-avail="${a.id}" data-status="OFFLINE" style="flex:1;">Offline</button>
          </div>
        </div>
      </div>
    `).join('');

    grid.querySelectorAll('[data-avail]').forEach(btn => btn.addEventListener('click', async () => {
      try {
        await Api.setAgentAvailability(btn.dataset.avail, btn.dataset.status);
        Utils.toast(`Agent status updated to ${btn.dataset.status}`, 'success');
        await loadAll();
      } catch (e) { Utils.toast(e.message, 'error'); }
    }));
  }

  function renderRestaurants() {
    const grid = document.getElementById('restaurantGrid');
    grid.innerHTML = allRestaurants.map(r => `
      <div class="card">
        <div class="card-body">
          <div class="cell-primary">${Utils.escapeHtml(r.name)}</div>
          <div class="cell-sub">${Utils.escapeHtml(r.cuisine)} · Zone ${r.location.zone.replace('Zone-', '')}</div>
          <div class="row-between mt-16" style="font-size:12px;color:var(--ink-soft);">
            <span>★ ${r.rating.toFixed(1)}</span>
            <span>~${Math.round(r.avgPrepTimeMinutes)} min prep</span>
          </div>
        </div>
      </div>
    `).join('');
  }

  // ---- Actions ---------------------------------------------------------------------
  async function dispatchOrder(orderId) {
    try {
      const res = await Api.dispatchOrder(orderId);
      Utils.toast(res.dispatch.message, 'success');
      await loadAll();
    } catch (e) { Utils.toast(e.message, 'error'); }
  }

  async function cancelOrder(orderId) {
    if (!confirm(`Cancel order ${orderId}? This cannot be undone.`)) return;
    try {
      await Api.cancelOrder(orderId);
      Utils.toast(`Order ${orderId} cancelled`, 'warning');
      await loadAll();
    } catch (e) { Utils.toast(e.message, 'error'); }
  }

  async function openAssignModal(orderId) {
    manualAssignOrderId = orderId;
    document.getElementById('assignOrderId').textContent = orderId;
    Utils.openModal('assignModal');
    const body = document.getElementById('assignModalBody');
    body.innerHTML = `<div class="state-block"><div class="spinner"></div></div>`;
    try {
      const res = await Api.getDispatchCandidates(orderId);
      if (res.candidates.length === 0) {
        body.innerHTML = `<div class="state-block">
          <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.6"><circle cx="12" cy="12" r="9"/><path d="M12 8v5M12 16h.01"/></svg>
          <h4>No suitable delivery agent available</h4>
          <p>Every agent is offline or already at maximum capacity.</p>
        </div>`;
        return;
      }
      body.innerHTML = `<div class="stack">${res.candidates.map(c => scorePanelHtml(c)).join('')}</div>`;
      body.querySelectorAll('[data-pick-agent]').forEach(btn => btn.addEventListener('click', () => confirmManualAssign(btn.dataset.pickAgent)));
    } catch (e) {
      body.innerHTML = `<div class="state-block"><h4>Could not load candidates</h4><p>${Utils.escapeHtml(e.message)}</p></div>`;
    }
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
        <button class="btn btn-primary btn-block mt-16" data-pick-agent="${c.agentId}">Assign to ${Utils.escapeHtml(c.agentName)}</button>
      </div>
    `;
  }

  async function confirmManualAssign(agentId) {
    try {
      const res = await Api.reassignOrder(manualAssignOrderId, agentId);
      Utils.toast(res.message, 'success');
      Utils.closeModal('assignModal');
      await loadAll();
    } catch (e) { Utils.toast(e.message, 'error'); }
  }

  async function autoDispatch() {
    try {
      const res = await Api.autoDispatchAll();
      Utils.toast(`Auto dispatch complete — ${res.assignedCount} order(s) assigned`, res.assignedCount > 0 ? 'success' : 'info');
      await loadAll();
    } catch (e) { Utils.toast(e.message, 'error'); }
  }

  // ---- Data loading -----------------------------------------------------------------
  async function loadAll() {
    try {
      const [dashRes, ordersRes, agentsRes, restaurantsRes] = await Promise.all([
        Api.getDashboard(), Api.getOrders(), Api.getAgents(), Api.getRestaurants(),
      ]);
      renderStats(dashRes.dashboard);
      allOrders = ordersRes.orders;
      allAgents = agentsRes.agents;
      allRestaurants = restaurantsRes.restaurants;
      renderOrdersTable();
      renderAgents();
      renderRestaurants();
    } catch (e) {
      Utils.toast(e.message, 'error');
    }
  }

  function init() {
    const session = Shell.render({ activeHref: 'admin.html', title: 'Overview', subtitle: 'Live operations across the whole platform' });
    if (!session) return;
    document.getElementById('pageContent').innerHTML = pageTemplate();
    document.body.insertAdjacentHTML('beforeend', modalTemplate());
    renderFilters();

    document.getElementById('refreshBtn').addEventListener('click', loadAll);
    document.getElementById('autoDispatchBtn').addEventListener('click', autoDispatch);
    document.getElementById('orderSearch').addEventListener('input', (e) => {
      searchTerm = e.target.value;
      renderOrdersTable();
    });

    loadAll();
    setInterval(loadAll, 15000); // dashboard auto-refresh
  }

  return { init };
})();

document.addEventListener('DOMContentLoaded', AdminDashboard.init);
