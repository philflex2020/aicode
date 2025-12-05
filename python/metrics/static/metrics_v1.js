// static/metrics_v1.js
(() => {
  console.log('metrics_v1.js loaded');

  // Example: initialize metrics UI and fetch metrics data
  async function initMetrics(apiUrl) {
    try {
      const response = await fetch(apiUrl);
      if (!response.ok) throw new Error(`HTTP error! status: ${response.status}`);
      const json = await response.json();
      // For demo, just log the data
      console.log('Metrics data:', json);
      // TODO: Render metrics UI here
      const container = document.getElementById('alarm-container');
      container.textContent = 'Metrics data loaded. Implement rendering here.';
    } catch (err) {
      const errorEl = document.getElementById('error-message');
      errorEl.textContent = `Error loading metrics: ${err.message}`;
    }
  }

  // Expose a function called by the loader
  window.onBehaviorLoaded = () => {
    const apiUrl = document.getElementById('api-url-input').value.trim();
    if (apiUrl) {
      initMetrics(apiUrl);
    }
  };
})();