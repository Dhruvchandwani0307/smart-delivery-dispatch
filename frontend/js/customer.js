/* ============================================================================
   customer.js
   Customer dashboard: current order with a live status timeline + assigned
   agent + ETA, full order history, and a polished "place a new order" form.
   ============================================================================ */

const CustomerDashboard = (() => {
  let session = null;
  let myOrders = [];
  let restaurants = [];
  let itemRowCount = 0;

  const TIMELINE_STEPS = ['PLACED', 'CONFIRMED', 'PREPARING', 'ASSIGNED', 'OUT_FOR_DELIVERY', 'DELIVERED'];
  const TIMELINE_LABELS = {
    PLACED: 'Order Placed', CONFIRMED: 'Confirmed', PREPARING: 'Preparing',
    ASSIGNED: 'Agent Assigned', OUT_FOR_DELIVERY: 'Out for Delivery', DELIVERED: 'Delivered',
  };
  const CHECK_ICON = '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.5"><path d="M5 13l4 4L19 7"/></svg>';

  function pageTemplate() {
    return `
      <div class="section-head">
        <div><h2>Current Order</h2><div class="desc">Your most recent active order, tracked live.</div></div>
        <button class="btn btn-accent" id="newOrderBtn">
          <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><path d="M12 5v14M5 12h14"/></svg>
          Place New Order
        </button>
      </div>
      <div id="currentOrderCard"></div>

      <div class="section-head mt-24">
        <div><h2>Order History</h2><div class="desc">Every order you've placed.</div></div>
      </div>
      <div class="card">
        <div class="card-body tight" id="historyTableWrap"></div>
      </div>
    `;
  }

  function modalTemplate() {
    return `
      <div class="modal-overlay" id="newOrderModal">
        <div class="modal-box wide">
          <div class="modal-header">
            <h3>Place a New Order</h3>
            <button class="modal-close" onclick="Utils.closeModal('newOrderModal')">&times;</button>
          </div>
          <div class="modal-body">
            <form id="newOrderForm">
              <div class="form-group">
                <label class="form-label">Restaurant</label>
                <select class="form-control" id="restaurantSelect" required></select>
              </div>

              <div class="form-group">
                <label class="form-label">Priority</label>
                <select class="form-control" id="prioritySelect">
                  <option value="NORMAL">Normal</option>
                  <option value="HIGH">High</option>
                  <option value="URGENT">Urgent</option>
                  <option value="PERISHABLE">Perishable (e.g. ice cream, frozen items)</option>
                </select>
              </div>

              <div class="form-group">
                <label class="form-label">Payment Method</label>
                <select class="form-control" id="paymentSelect">
                  <option value="UPI">UPI</option>
                  <option value="CARD">Card</option>
                  <option value="CASH_ON_DELIVERY">Cash on Delivery</option>
                  <option value="WALLET">Wallet</option>
                </select>
              </div>

              <div class="form-group">
                <div class="row-between mb-16">
                  <label class="form-label" style="margin:0;">Items</label>
                  <button type="button" class="btn btn-ghost btn-sm" id="addItemRowBtn">+ Add item</button>
                </div>
                <div id="itemRows" class="stack" style="gap:10px;"></div>
              </div>

              <div class="row-between" style="font-size:14px;font-weight:700;padding-top:8px;border-top:1px solid var(--line-soft);">
                <span>Estimated Total</span>
                <span class="mono" id="orderTotalPreview">₹0</span>
              </div>
              <div class="form-error" id="newOrderError"></div>
            </form>
          </div>
          <div class="modal-footer">
            <button class="btn btn-outline" onclick="Utils.closeModal('newOrderModal')">Cancel</button>
            <button class="btn btn-accent" id="submitOrderBtn">Place Order</button>
          </div>
        </div>
      </div>
    `;
  }

  function itemRowHtml(idx) {
    return `
      <div class="row gap-8" data-item-row="${idx}">
        <input class="form-control" style="flex:2;" placeholder="Item name" data-item-name value="Cheeseburger">
        <input class="form-control" style="flex:1;" type="number" min="1" value="1" data-item-qty>
        <input class="form-control" style="flex:1;" type="number" min="0" step="1" value="149" data-item-price>
        <button type="button" class="btn btn-ghost btn-icon" data-remove-row="${idx}">
          <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><path d="M18 6 6 18M6 6l12 12"/></svg>
        </button>
      </div>`;
  }

  function addItemRow() {
    itemRowCount++;
    document.getElementById('itemRows').insertAdjacentHTML('beforeend', itemRowHtml(itemRowCount));
    wireItemRow(itemRowCount);
    updateTotalPreview();
  }

  function wireItemRow(idx) {
    const row = document.querySelector(`[data-item-row="${idx}"]`);
    row.querySelectorAll('input').forEach(inp => inp.addEventListener('input', updateTotalPreview));
    row.querySelector('[data-remove-row]').addEventListener('click', () => { row.remove(); updateTotalPreview(); });
  }

  function updateTotalPreview() {
    let total = 0;
    document.querySelectorAll('[data-item-row]').forEach(row => {
      const qty = Number(row.querySelector('[data-item-qty]').value || 0);
      const price = Number(row.querySelector('[data-item-price]').value || 0);
      total += qty * price;
    });
    document.getElementById('orderTotalPreview').textContent = Utils.formatCurrency(total);
  }

  // ---- Current order / timeline ------------------------------------------------
  function timelineHtml(order) {
    if (order.status === 'CANCELLED') {
      return `<div class="state-block">
        <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.6"><path d="M18 6 6 18M6 6l12 12"/></svg>
        <h4>This order was cancelled</h4><p>Order ${order.id} was cancelled and is no longer active.</p></div>`;
    }
    const currentIndex = order.status === 'PICKED_UP' ? TIMELINE_STEPS.indexOf('ASSIGNED') : TIMELINE_STEPS.indexOf(order.status);

    return `<div class="timeline">` + TIMELINE_STEPS.map((step, i) => {
      const done = i < currentIndex || (order.status === 'DELIVERED');
      const current = i === currentIndex && order.status !== 'DELIVERED';
      const cls = done ? 'done' : current ? 'current' : '';
      return `
        <div class="timeline-step ${cls}">
          <div class="marker">${done ? CHECK_ICON : ''}</div>
          <div class="line"></div>
          <div>
            <div class="title">${TIMELINE_LABELS[step]}</div>
          </div>
        </div>`;
    }).join('') + `</div>`;
  }

  async function renderCurrentOrder() {
    const active = myOrders.filter(o => o.status !== 'DELIVERED' && o.status !== 'CANCELLED');
    const container = document.getElementById('currentOrderCard');

    if (active.length === 0) {
      container.innerHTML = `<div class="card"><div class="card-body">
        <div class="state-block">
          <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.6"><path d="M6 8h12l-1 12H7L6 8Z"/><path d="M9 8V6a3 3 0 0 1 6 0v2"/></svg>
          <h4>No active orders</h4>
          <p>Place a new order to see it tracked here in real time.</p>
        </div></div></div>`;
      return;
    }

    const order = active[0];
    let agentHtml = '';
    if (order.assignedAgentId) {
      try {
        const agentRes = await Api.getAgent(order.assignedAgentId);
        const a = agentRes.agent;
        agentHtml = `
          <div class="card-body" style="border-top:1px solid var(--line-soft);">
            <div class="row-between">
              <div class="row">
                <div class="avatar-sm" style="width:34px;height:34px;">${Utils.initials(a.name)}</div>
                <div>
                  <div class="cell-primary">${Utils.escapeHtml(a.name)}</div>
                  <div class="cell-sub">${a.vehicle.type} · ★ ${a.rating.toFixed(1)}</div>
                </div>
              </div>
              <div class="text-right">
                <div class="mono" style="font-weight:700;">${Utils.formatMinutes(order.estimatedEtaMinutes)}</div>
                <div class="cell-sub">estimated arrival</div>
              </div>
            </div>
          </div>`;
      } catch (e) { /* agent lookup best-effort */ }
    }

    container.innerHTML = `
      <div class="card">
        <div class="card-header">
          <div>
            <h3>${order.id} <span style="font-weight:400;color:var(--ink-soft);">· ${Utils.escapeHtml(order.restaurantName || '')}</span></h3>
            <div class="cell-sub">${order.items.length} item(s) · ${Utils.formatCurrency(order.totalAmount)}</div>
          </div>
          ${Utils.statusBadge(order.status)}
        </div>
        <div class="card-body">${timelineHtml(order)}</div>
        ${agentHtml}
        ${['PLACED', 'CONFIRMED', 'PREPARING', 'READY'].includes(order.status) ? `
        <div class="card-body" style="border-top:1px solid var(--line-soft);">
          <button class="btn btn-danger btn-block" data-cancel-current="${order.id}">Cancel this order</button>
        </div>` : ''}
      </div>
    `;

    container.querySelector('[data-cancel-current]')?.addEventListener('click', async () => {
      if (!confirm(`Cancel order ${order.id}?`)) return;
      try {
        await Api.cancelOrder(order.id);
        Utils.toast('Order cancelled', 'warning');
        await loadOrders();
      } catch (e) { Utils.toast(e.message, 'error'); }
    });
  }

  // ---- History table -------------------------------------------------------------
  function renderHistory() {
    const wrap = document.getElementById('historyTableWrap');
    if (myOrders.length === 0) {
      wrap.innerHTML = `<div class="state-block">
        <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.6"><path d="M4 20V10M12 20V4M20 20v-7"/></svg>
        <h4>No orders yet</h4><p>Your order history will show up here.</p></div>`;
      return;
    }
    wrap.innerHTML = `
      <table class="data-table responsive-cards">
        <thead><tr><th>Order ID</th><th>Restaurant</th><th>Items</th><th>Total</th><th>Priority</th><th>Status</th><th>Action</th></tr></thead>
        <tbody>
          ${myOrders.map(o => `
            <tr>
              <td data-label="Order ID" class="cell-id">${o.id}</td>
              <td data-label="Restaurant">${Utils.escapeHtml(o.restaurantName || '—')}</td>
              <td data-label="Items">${o.items.length} item(s)</td>
              <td data-label="Total" class="mono">${Utils.formatCurrency(o.totalAmount)}</td>
              <td data-label="Priority">${Utils.priorityBadge(o.priority)}</td>
              <td data-label="Status">${Utils.statusBadge(o.status)}</td>
              <td data-label="Action">${['PLACED', 'CONFIRMED', 'PREPARING', 'READY'].includes(o.status) ? `<button class="btn btn-danger btn-sm" data-cancel-hist="${o.id}">Cancel</button>` : '—'}</td>
            </tr>`).join('')}
        </tbody>
      </table>`;

    wrap.querySelectorAll('[data-cancel-hist]').forEach(btn => btn.addEventListener('click', async () => {
      if (!confirm(`Cancel order ${btn.dataset.cancelHist}?`)) return;
      try {
        await Api.cancelOrder(btn.dataset.cancelHist);
        Utils.toast('Order cancelled', 'warning');
        await loadOrders();
      } catch (e) { Utils.toast(e.message, 'error'); }
    }));
  }

  // ---- New order form ----------------------------------------------------------
  async function openNewOrderModal() {
    document.getElementById('itemRows').innerHTML = '';
    itemRowCount = 0;
    addItemRow();
    document.getElementById('newOrderError').classList.remove('show');

    const select = document.getElementById('restaurantSelect');
    select.innerHTML = restaurants.map(r => `<option value="${r.id}">${Utils.escapeHtml(r.name)} — ${Utils.escapeHtml(r.cuisine)}</option>`).join('');
    Utils.openModal('newOrderModal');
  }

  async function submitNewOrder() {
    const errorBox = document.getElementById('newOrderError');
    errorBox.classList.remove('show');

    const items = [];
    document.querySelectorAll('[data-item-row]').forEach(row => {
      const name = row.querySelector('[data-item-name]').value.trim();
      const quantity = Number(row.querySelector('[data-item-qty]').value);
      const price = Number(row.querySelector('[data-item-price]').value);
      if (name && quantity > 0) items.push({ name, quantity, price });
    });

    if (items.length === 0) {
      errorBox.textContent = 'Add at least one item to your order.';
      errorBox.classList.add('show');
      return;
    }

    try {
      await Api.createOrder({
        customerId: session.id,
        restaurantId: document.getElementById('restaurantSelect').value,
        items,
        priority: document.getElementById('prioritySelect').value,
        paymentMethod: document.getElementById('paymentSelect').value,
      });
      Utils.toast('Order placed! Track it under Current Order.', 'success');
      Utils.closeModal('newOrderModal');
      await loadOrders();
    } catch (e) {
      errorBox.textContent = e.message;
      errorBox.classList.add('show');
    }
  }

  // ---- Data loading -----------------------------------------------------------------
  async function loadOrders() {
    try {
      const res = await Api.getOrders({ customerId: session.id });
      myOrders = res.orders;
      await renderCurrentOrder();
      renderHistory();
    } catch (e) { Utils.toast(e.message, 'error'); }
  }

  async function init() {
    session = Shell.render({ activeHref: 'customer.html', title: `Welcome, ${''}`, subtitle: 'Track your orders in real time' });
    if (!session) return;
    document.querySelector('.topbar-title h1').textContent = `Welcome, ${session.name.split(' ')[0]}`;

    if (!session.id) session.id = 'CUS-0001'; // demo fallback

    document.getElementById('pageContent').innerHTML = pageTemplate();
    document.body.insertAdjacentHTML('beforeend', modalTemplate());

    document.getElementById('newOrderBtn').addEventListener('click', openNewOrderModal);
    document.getElementById('addItemRowBtn').addEventListener('click', addItemRow);
    document.getElementById('submitOrderBtn').addEventListener('click', submitNewOrder);

    try {
      const restRes = await Api.getRestaurants();
      restaurants = restRes.restaurants;
    } catch (e) { Utils.toast(e.message, 'error'); }

    await loadOrders();
    setInterval(loadOrders, 10000);
  }

  return { init };
})();

document.addEventListener('DOMContentLoaded', CustomerDashboard.init);
