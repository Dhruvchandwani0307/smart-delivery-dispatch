/* ============================================================================
   config.js
   Set this if the frontend is deployed separately from the C++ backend
   (e.g. frontend on Vercel/Netlify, backend on Render/Railway/Fly.io).

   Leave it as null for local runs, or when the C++ server itself is
   serving the frontend (main.cpp's set_mount_point) — same-origin requests
   work automatically in that case and this file does nothing.
   ============================================================================ */

window.SDD_API_BASE_URL = null;
// Example once your backend is deployed:
// window.SDD_API_BASE_URL = "https://smart-delivery-dispatch.onrender.com";
