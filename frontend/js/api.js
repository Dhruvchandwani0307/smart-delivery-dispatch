/* ============================================================================
   api.js
   Thin fetch wrapper around the C++ backend's REST API. Every function
   returns the parsed JSON body and throws an Error with a friendly message
   on failure, so callers can just try/catch and show a toast.
   ============================================================================ */

const Api = (() => {
  // Resolution order for where the C++ backend lives:
  //   1. window.SDD_API_BASE_URL, if set (see config.js) — use this when the
  //      frontend is deployed separately from the backend, e.g. frontend on
  //      Vercel/Netlify and backend on Render/Railway/Fly.io.
  //   2. Same origin (empty prefix) — works when the C++ server itself is
  //      serving the frontend (see main.cpp set_mount_point), which is the
  //      default for local runs.
  //   3. http://localhost:8080 — fallback for opening index.html directly
  //      from disk (file://) during local development.
  const BASE_URL = window.SDD_API_BASE_URL
    ? window.SDD_API_BASE_URL.replace(/\/$/, '')
    : (window.location.protocol === 'file:' ? 'http://localhost:8080' : '');

  async function request(method, path, body) {
    let res;
    try {
      res = await fetch(BASE_URL + path, {
        method,
        headers: body ? { 'Content-Type': 'application/json' } : undefined,
        body: body ? JSON.stringify(body) : undefined,
      });
    } catch (networkErr) {
      throw new Error('Cannot reach the dispatch server. Is the C++ backend running on port 8080?');
    }

    let data = null;
    try { data = await res.json(); } catch (e) { /* empty body */ }

    if (!res.ok) {
      const message = (data && data.message) ? data.message : `Request failed (HTTP ${res.status})`;
      const err = new Error(message);
      err.status = res.status;
      err.data = data;
      throw err;
    }
    return data;
  }

  const get = (path) => request('GET', path);
  const post = (path, body) => request('POST', path, body || {});
  const put = (path, body) => request('PUT', path, body || {});

  return {
    // Dashboard / analytics
    getDashboard: () => get('/api/dashboard'),
    getAnalytics: () => get('/api/analytics'),
    getNotifications: (limit = 20) => get(`/api/notifications?limit=${limit}`),

    // Orders
    getOrders: (params = {}) => {
      const qs = new URLSearchParams(params).toString();
      return get('/api/orders' + (qs ? `?${qs}` : ''));
    },
    getOrder: (id) => get(`/api/orders/${id}`),
    createOrder: (payload) => post('/api/orders', payload),
    updateOrderStatus: (id, status) => put(`/api/orders/${id}/status`, { status }),
    cancelOrder: (id) => post(`/api/orders/${id}/cancel`),
    dispatchOrder: (id) => post(`/api/orders/${id}/dispatch`),
    reassignOrder: (id, agentId) => post(`/api/orders/${id}/reassign`, { agentId }),

    // Dispatch Center
    getDispatchCandidates: (orderId) => get(`/api/dispatch/${orderId}`),
    getDispatchQueue: () => get('/api/dispatch-queue'),
    autoDispatchAll: () => post('/api/dispatch/auto'),

    // Agents
    getAgents: (params = {}) => {
      const qs = new URLSearchParams(params).toString();
      return get('/api/agents' + (qs ? `?${qs}` : ''));
    },
    getAgent: (id) => get(`/api/agents/${id}`),
    createAgent: (payload) => post('/api/agents', payload),
    setAgentAvailability: (id, status) => put(`/api/agents/${id}/availability`, { status }),

    // Restaurants
    getRestaurants: () => get('/api/restaurants'),
    createRestaurant: (payload) => post('/api/restaurants', payload),

    // Customers
    getCustomers: () => get('/api/customers'),
    getCustomer: (id) => get(`/api/customers/${id}`),
  };
})();
