/* ============================================================================
   analytics.js
   All charts are small hand-rolled SVG (no external chart library) fed
   directly from GET /api/analytics — nothing here is randomly generated.
   ============================================================================ */

const Analytics = (() => {

  const STATUS_COLORS = {
    PLACED: '#8A93A3', CONFIRMED: '#2F6FED', PREPARING: '#C77700', READY: '#FF5A1F',
    ASSIGNED: '#7C5CFC', PICKED_UP: '#2F6FED', OUT_FOR_DELIVERY: '#0F9D7C',
    DELIVERED: '#0F9D7C', CANCELLED: '#D6304A',
  };
  const PRIORITY_COLORS = { NORMAL: '#8A93A3', HIGH: '#C77700', URGENT: '#D6304A', PERISHABLE: '#7C5CFC' };

  function pageTemplate() {
    return `
      <div class="stat-grid mb-16" id="analyticsStats"></div>

      <div class="grid grid-2">
        <div class="card">
          <div class="card-header"><h3>Orders Per Hour</h3></div>
          <div class="card-body" id="ordersPerHourChart"></div>
        </div>
        <div class="card">
          <div class="card-header"><h3>Order Status Distribution</h3></div>
          <div class="card-body" id="statusChart"></div>
        </div>
      </div>

      <div class="grid grid-2 mt-24">
        <div class="card">
          <div class="card-header"><h3>Orders by Priority</h3></div>
          <div class="card-body" id="priorityChart"></div>
        </div>
        <div class="card">
          <div class="card-header"><h3>Agent Utilization</h3></div>
          <div class="card-body" id="utilizationChart" style="max-height:340px;overflow-y:auto;"></div>
        </div>
      </div>
    `;
  }

  function emptyState(label) {
    return `<div class="state-block">
      <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.6"><path d="M4 20V10M12 20V4M20 20v-7"/></svg>
      <h4>No data yet</h4><p>${label}</p></div>`;
  }

  function barChartSvg(items, opts = {}) {
    if (!items.length) return emptyState('Chart data will appear once orders exist.');
    const width = 560, height = 200, padding = 28;
    const max = Math.max(1, ...items.map(i => i.value));
    const barW = (width - padding * 2) / items.length - 8;

    const bars = items.map((item, i) => {
      const x = padding + i * ((width - padding * 2) / items.length);
      const h = (item.value / max) * (height - padding - 20);
      const y = height - padding - h;
      return `
        <rect x="${x}" y="${y}" width="${barW}" height="${h}" rx="4" fill="${item.color || 'var(--accent, #FF5A1F)'}"></rect>
        <text x="${x + barW / 2}" y="${height - padding + 14}" text-anchor="middle" font-size="9.5" fill="#8A93A3" font-family="JetBrains Mono, monospace">${item.label}</text>
        <text x="${x + barW / 2}" y="${y - 6}" text-anchor="middle" font-size="10.5" fill="#12161F" font-weight="700" font-family="JetBrains Mono, monospace">${item.value}</text>
      `;
    }).join('');

    return `<svg viewBox="0 0 ${width} ${height}" style="width:100%;height:auto;">${bars}</svg>`;
  }

  function horizontalBarList(items) {
    if (!items.length) return emptyState('Chart data will appear once orders exist.');
    const total = items.reduce((s, i) => s + i.value, 0) || 1;
    return `<div class="stack" style="gap:14px;">` + items.map(i => `
      <div>
        <div class="row-between" style="font-size:12.5px;margin-bottom:6px;">
          <span style="font-weight:600;">${i.label}</span>
          <span class="mono text-soft">${i.value} (${Math.round(100 * i.value / total)}%)</span>
        </div>
        <div class="progress-track"><div class="progress-fill" style="width:${Math.round(100 * i.value / total)}%;background:${i.color};"></div></div>
      </div>
    `).join('') + `</div>`;
  }

  function renderStats(summary) {
    const cards = [
      { label: 'Total Orders', value: summary.totalOrders },
      { label: 'Active Deliveries', value: summary.activeDeliveries },
      { label: 'Delivered Today', value: summary.deliveredToday },
      { label: 'Avg Delivery Time', value: Utils.formatMinutes(summary.averageDeliveryTimeMinutes) },
      { label: 'Cancellation Rate', value: summary.cancellationRate.toFixed(1) + '%' },
      { label: 'Total Agents', value: summary.totalAgents },
    ];
    document.getElementById('analyticsStats').innerHTML = cards.map(c => `
      <div class="stat-card"><div class="accent-bar" style="background:var(--accent)"></div><div class="label">${c.label}</div><div class="value">${c.value}</div></div>
    `).join('');
  }

  async function load() {
    try {
      const res = await Api.getAnalytics();
      const a = res.analytics;
      renderStats(a.summary);

      const hourItems = a.ordersPerHour.map(h => ({ label: String(h.hour).padStart(2, '0'), value: h.count }));
      document.getElementById('ordersPerHourChart').innerHTML = barChartSvg(hourItems);

      const statusItems = a.statusDistribution.map(s => ({ label: Utils.STATUS_META[s.status]?.label || s.status, value: s.count, color: STATUS_COLORS[s.status] || '#8A93A3' }));
      document.getElementById('statusChart').innerHTML = horizontalBarList(statusItems);

      const priorityItems = a.priorityDistribution.map(p => ({ label: Utils.PRIORITY_META[p.priority]?.label || p.priority, value: p.count, color: PRIORITY_COLORS[p.priority] || '#8A93A3' }));
      document.getElementById('priorityChart').innerHTML = horizontalBarList(priorityItems);

      const utilItems = a.agentUtilization.map(u => ({ label: u.agentName, value: Math.round(u.utilizationPercent), color: u.utilizationPercent >= 90 ? '#D6304A' : u.utilizationPercent > 0 ? '#FF5A1F' : '#E4E7EC' }));
      document.getElementById('utilizationChart').innerHTML = horizontalBarList(utilItems);
    } catch (e) {
      Utils.toast(e.message, 'error');
    }
  }

  function init() {
    const session = Shell.render({ activeHref: 'analytics.html', title: 'Analytics', subtitle: 'Live metrics computed from backend state — nothing here is faked' });
    if (!session) return;
    document.getElementById('pageContent').innerHTML = pageTemplate();
    load();
    setInterval(load, 15000);
  }

  return { init };
})();

document.addEventListener('DOMContentLoaded', Analytics.init);
