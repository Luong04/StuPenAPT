/**
 * Init script — injected vào MỌI page qua add_init_script().
 *
 * Patch fetch() và XMLHttpRequest để:
 *   1. Log mọi dynamic request ra console (Playwright bắt được qua CDP)
 *   2. Đánh dấu request origin cho traffic analysis
 *   3. Expose helper functions cho link extraction
 */

(function () {
  "use strict";

  // ─── Patch fetch() ────────────────────────────────────
  const _fetch = window.fetch;
  window.fetch = function (...args) {
    const url = typeof args[0] === "string" ? args[0] : args[0]?.url || "";
    const method =
      (typeof args[1] === "object" ? args[1]?.method : null) || "GET";
    console.log("[DFS_CRAWL] fetch:", method, url);
    return _fetch.apply(this, args);
  };

  // ─── Patch XMLHttpRequest ─────────────────────────────
  const _xhrOpen = XMLHttpRequest.prototype.open;
  XMLHttpRequest.prototype.open = function (method, url, ...rest) {
    console.log("[DFS_CRAWL] XHR:", method, url);
    this.__dfs_url = url;
    this.__dfs_method = method;
    return _xhrOpen.apply(this, [method, url, ...rest]);
  };

  // ─── Patch sendBeacon ─────────────────────────────────
  if (navigator.sendBeacon) {
    const _beacon = navigator.sendBeacon.bind(navigator);
    navigator.sendBeacon = function (url, data) {
      console.log("[DFS_CRAWL] beacon:", url);
      return _beacon(url, data);
    };
  }

  // ─── Helper: extract tất cả URL động từ page ─────────
  window.__dfs_extractURLs = function () {
    const urls = new Set();

    // <a> tags
    document.querySelectorAll("a[href]").forEach((a) => {
      if (
        a.href &&
        !a.href.startsWith("javascript:") &&
        !a.href.startsWith("mailto:")
      ) {
        urls.add(a.href);
      }
    });

    // routerLink (Angular SPA)
    document.querySelectorAll("[routerLink], [routerlink]").forEach((el) => {
      const rl =
        el.getAttribute("routerLink") || el.getAttribute("routerlink") || "";
      if (rl && rl !== "/") {
        if (location.href.includes("#/")) {
          urls.add(location.origin + "/#" + (rl.startsWith("/") ? rl : "/" + rl));
        } else {
          try {
            urls.add(new URL(rl, location.href).href);
          } catch (e) {}
        }
      }
    });

    // Form actions
    document.querySelectorAll("form[action]").forEach((f) => {
      const action = f.getAttribute("action");
      if (action && action !== "" && action !== "#") {
        try {
          urls.add(new URL(action, location.href).href);
        } catch (e) {}
      }
    });

    return [...urls];
  };

  // ─── Marker ───────────────────────────────────────────
  window.__DFS_CRAWL__ = true;
})();
