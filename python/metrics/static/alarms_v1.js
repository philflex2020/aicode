// static/alarms_v1.js
(() => {
  console.log('alarms_v1.js loaded');

  // Example: initialize alarm UI and fetch alarm data
  async function initAlarms(apiUrl) {
    try {
      const response = await fetch(apiUrl);
      if (!response.ok) throw new Error(`HTTP error! status: ${response.status}`);
      const json = await response.json();
      // For demo, just log the data
      console.log('Alarm data:', json);
      // TODO: Render alarm chart or UI here
      const container = document.getElementById('alarm-container');
      container.textContent = 'Alarm data loaded. Implement rendering here.';
    } catch (err) {
      const errorEl = document.getElementById('error-message');
      errorEl.textContent = `Error loading alarms: ${err.message}`;
    }
  }

  // Expose a function called by the loader
  window.onBehaviorLoaded = () => {
    const apiUrl = document.getElementById('api-url-input').value.trim();
    if (apiUrl) {
      initAlarms(apiUrl);
    }
  };
})();