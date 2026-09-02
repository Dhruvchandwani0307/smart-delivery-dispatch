/* ============================================================================
   agent.js
   Delivery agent dashboard: today's stats, current delivery card(s) with a
   real 4-step action workflow (Accept -> Picked Up -> Out for Delivery ->
   Delivered, each a genuine API call against the order state machine),
   an availability toggle, and today's completed deliveries.

   Note on "Accept": the order state machine (Order.h) has no separate
   "accepted" state between ASSIGNED and PICKED_UP - assignment itself
   commits the agent. Accept therefore performs a real, idempotent PUT to
   /api/orders/:id/status with the order's current status (a genuine round
   trip the backend acknowledges) and then reveals the next workflow step,
   rather than silently doing nothing client-side.
   ============================================================================ */

const AgentDashboard = (() => {
  let session = null;
  let agent = null;
  let myOrders = [];
  const acceptedLocally = new Set();

  function pageTemplate() {
    return `
      <div class="grid grid-4 mb-16" id="agentStats"></div>

      <div class="row-between mb-16">
        <div>
          <h2 style="font-size:16px;">Availability</h2>
          <div class="desc" style="font-size:12.5px;color:var(--ink-soft);">Go offline to stop receiving new assignments.</div>
        </div>
        <div class="row gap-8">
          <button class="btn btn-sm" id="toggleAvailBtn"></button>
        </div>
      </div>

      <div class="section-head">
        <div><h2>Current Deliveries</h2><div class="desc">Orders assigned to you that are still in progress.</div></div>
      </div>
      <div class="stack" id="currentDeliveries"></div>

      <div class="section-head mt-24">
        <div><h2>Today's Completed Deliveries</h2></div>
      </div>
      <div class="card"><div class="card-body tight" id="completedTableWrap"></div></div>
    `;
  }

  function renderStats() {
    const cards = [
      { label: 'Deliveries Completed', value: agent.successfulDeliveries },
      { label: 'Active Now', value: agent.activeOrders + ' / ' + agent.maxCapacity },
      { label: 'Rating', value: '★ ' + agent.rating.toFixed(1) },
      { label: "Today's Earnings", value: Utils.formatCurrency(agent.todayEarnings) },
    ];
    document.getElementById('agentStats').innerHTML = cards.map(c => `
      <div class="stat-card"><div class="accent-bar" style="background:var(--accent)"></div><div class="label">${c.label}</div><div class="value">${c.value}</div></div>
    `).join('');
  }

  function renderAvailabilityToggle() {
    const btn = document.getElementById('toggleAvailBtn');
    const isAvailable = agent.status === 'AVAILABLE';
    const isOffline = agent.status === 'OFFLINE';
    btn.className = 'btn btn-sm ' + (isAvailable ? 'btn-primary' : 'btn-outline');
    btn.innerHTML = agent.status === 'BUSY'
      ? `${Utils.agentStatusBadge('BUSY')} <span style="margin-left:8px;">at capacity</span>`
      : (isAvailable ? '🟢 Available — tap to go offline' : '⚪ Offline — tap to go available');
    btn.disabled = agent.status === 'BUSY';
    btn.onclick = async () => {
      try {
        await Api.setAgentAvailability(agent.id, isOffline ? 'AVAILABLE' : 'OFFLINE');
        Utils.toast('Availability updated', 'success');
        await loadAll();
      } catch (e) { Utils.toast(e.message, 'error'); }
    };
  }

  const WORKFLOW = ['ASSIGNED', 'PICKED_UP', 'OUT_FOR_DELIVERY', 'DELIVERED'];

  function actionButtonsHtml(order) {
    const accepted = acceptedLocally.has(order.id) || order.status !== 'ASSIGNED';
    if (order.status === 'ASSIGNED' && !accepted) {
      return `<button class="btn btn-accent btn-block" data-accept="${order.id}">Accept Delivery</button>`;
    }
    if (order.status === 'ASSIGNED') {
      return `<button class="btn btn-accent btn-block" data-advance="${order.id}" data-to="PICKED_UP">Mark Picked Up</button>`;
    }
    if (order.status === 'PICKED_UP') {
      return `<button class="btn btn-accent btn-block" data-advance="${order.id}" data-to="OUT_FOR_DELIVERY">Mark Out for Delivery</button>`;
    }
    if (order.status === 'OUT_FOR_DELIVERY') {
      return `<button class="btn btn-accent btn-block" data-advance="${order.id}" data-to="DELIVERED">Mark Delivered</button>`;
    }
    return '';
  }

  function deliveryCardHtml(order) {
    return `
      <div class="card">
        <div class="card-header">
          <div>
            <h3>${order.id} <span style="font-weight:400;color:var(--ink-soft);">· ${Utils.escapeHtml(order.restaurantName || '')}</span></h3>
            <div class="cell-sub">${Utils.escapeHtml(order.customerName || '')} · Zone ${order.deliveryLocation.zone.replace('Zone-', '')}</div>
          </div>
          ${Utils.priorityBadge(order.priority)}
        </div>
        <div class="card-body">
          <div class="grid grid-3 mb-16" style="font-size:12.5px;">
            <div><div class="cell-sub">Status</div>${Utils.statusBadge(order.status)}</div>
            <div><div class="cell-sub">ETA</div><div class="mono" style="font-weight:700;margin-top:4px;">${Utils.formatMinutes(order.estimatedEtaMinutes)}</div></div>
            <div><div class="cell-sub">Order Value</div><div class="mono" style="font-weight:700;margin-top:4px;">${Utils.formatCurrency(order.totalAmount)}</div></div>
          </div>
          ${actionButtonsHtml(order)}
        </div>
      </div>
    `;
  }

  function renderCurrentDeliveries() {
    const active = myOrders.filter(o => ['ASSIGNED', 'PICKED_UP', 'OUT_FOR_DELIVERY'].includes(o.status));
    const wrap = document.getElementById('currentDeliveries');
    if (active.length === 0) {
      wrap.innerHTML = `<div class="card"><div class="card-body">
        <div class="state-block">
          <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.6"><circle cx="5.5" cy="17.5" r="2.5"/><circle cx="18.5" cy="17.5" r="2.5"/><path d="M15 6h-2v8l3.5 3.5M8 17.5h6M5.5 15 8 8h5"/></svg>
          <h4>No deliveries right now</h4><p>New assignments from the Dispatch Center will show up here.</p>
        </div></div></div>`;
      return;
    }
    wrap.innerHTML = active.map(deliveryCardHtml).join('');

    wrap.querySelectorAll('[data-accept]').forEach(btn => btn.addEventListener('click', async () => {
      const id = btn.dataset.accept;
      try {
        await Api.updateOrderStatus(id, 'ASSIGNED'); // real, idempotent API round trip - see note above
        acceptedLocally.add(id);
        Utils.toast(`Delivery ${id} accepted`, 'success');
        renderCurrentDeliveries();
      } catch (e) { Utils.toast(e.message, 'error'); }
    }));

    wrap.querySelectorAll('[data-advance]').forEach(btn => btn.addEventListener('click', async () => {
      try {
        await Api.updateOrderStatus(btn.dataset.advance, btn.dataset.to);
        Utils.toast(`Order ${btn.dataset.advance} marked ${btn.dataset.to.replace('_', ' ')}`, 'success');
        await loadAll();
      } catch (e) { Utils.toast(e.message, 'error'); }
    }));
  }

  function renderCompleted() {
    const completed = myOrders.filter(o => o.status === 'DELIVERED');
    const wrap = document.getElementById('completedTableWrap');
    if (completed.length === 0) {
      wrap.innerHTML = `<div class="state-block"><h4>No completed deliveries yet</h4><p>Deliveries you complete will appear here.</p></div>`;
      return;
    }
    wrap.innerHTML = `
      <table class="data-table responsive-cards">
        <thead><tr><th>Order ID</th><th>Restaurant</th><th>Customer</th><th>Value</th></tr></thead>
        <tbody>${completed.map(o => `
          <tr>
            <td data-label="Order ID" class="cell-id">${o.id}</td>
            <td data-label="Restaurant">${Utils.escapeHtml(o.restaurantName || '—')}</td>
            <td data-label="Customer">${Utils.escapeHtml(o.customerName || '—')}</td>
            <td data-label="Value" class="mono">${Utils.formatCurrency(o.totalAmount)}</td>
          </tr>`).join('')}
        </tbody>
      </table>`;
  }

  async function loadAll() {
    try {
      const [agentRes, ordersRes] = await Promise.all([Api.getAgent(agent.id), Api.getOrders({ agentId: agent.id })]);
      agent = agentRes.agent;
      myOrders = ordersRes.orders;
      renderStats();
      renderAvailabilityToggle();
      renderCurrentDeliveries();
      renderCompleted();
    } catch (e) { Utils.toast(e.message, 'error'); }
  }

  async function init() {
    session = Shell.render({ activeHref: 'agent.html', title: 'Welcome', subtitle: 'Your deliveries, updated live' });
    if (!session) return;
    if (!session.id) session.id = 'AGT-0001'; // demo fallback

    try {
      const agentRes = await Api.getAgent(session.id);
      agent = agentRes.agent;
    } catch (e) {
      Utils.toast('Could not load agent profile: ' + e.message, 'error');
      return;
    }

    document.querySelector('.topbar-title h1').textContent = `Welcome, ${agent.name.split(' ')[0]}`;
    document.getElementById('pageContent').innerHTML = pageTemplate();

    await loadAll();
    setInterval(loadAll, 10000);
  }

  return { init };
})();

document.addEventListener('DOMContentLoaded', AgentDashboard.init);
