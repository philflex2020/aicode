/* static/js/alarms.js */

(() => {
  let alarmData = [];
  let alarmLimits = {};
  let useBlue = true;
  let sortEnabled = false;

  const errorMessageEl = document.getElementById('error-message');

  function getColorClass(state) {
    return useBlue ? 'color-blue' : 'color-original';
  }
  function getColorText(state) {
    if (useBlue) {
      switch(state) {
        case 0: return 'Dark Blue (Normal)';
        case 1: return 'Light Blue (Warning)';
        case 2: return 'Sky Blue (Alert)';
        case 3: return 'Pale Blue (Critical)';
        default: return 'Unknown';
      }
    } else {
      switch(state) {
        case 0: return 'Green (Normal)';
        case 1: return 'Yellow (Warning)';
        case 2: return 'Orange (Alert)';
        case 3: return 'Red (Critical)';
        default: return 'Unknown';
      }
    }
  }
  function getColorHex(state) {
    if (useBlue) {
      switch(state) {
        case 0: return '#004080';
        case 1: return '#3399ff';
        case 2: return '#66ccff';
        case 3: return '#99ddff';
        default: return '#666';
      }
    } else {
      switch(state) {
        case 0: return '#4CAF50';
        case 1: return '#FFEB3B';
        case 2: return '#FF9800';
        case 3: return '#F44336';
        default: return '#666';
      }
    }
  }

  function renderAlarmChart(containerId, alarmData) {
    const container = document.getElementById(containerId);
    container.innerHTML = '';

    if (alarmData.length === 0) {
      container.textContent = 'No alarm data loaded.';
      document.getElementById('color-switch-btn').disabled = true;
      document.getElementById('sort-toggle-btn').disabled = true;
      return;
    }
    document.getElementById('color-switch-btn').disabled = false;
    document.getElementById('sort-toggle-btn').disabled = false;

    const allAlarms = Object.keys(alarmData[0].alarms);
    const alarmHistories = {};
    allAlarms.forEach(name => alarmHistories[name] = []);

    let lastValues = {};
    alarmData.forEach(entry => {
      const changes = entry.alarms;
      allAlarms.forEach(alarm => {
        if (alarm in changes) {
          lastValues[alarm] = changes[alarm];
        }
        alarmHistories[alarm].push(lastValues[alarm] ?? 0);
      });
    });

    const alarmFrequencies = {};
    allAlarms.forEach(alarm => {
      alarmFrequencies[alarm] = alarmHistories[alarm].reduce((acc, state) => acc + (state > 0 ? 1 : 0), 0);
    });

    const sortedAlarms = sortEnabled ? allAlarms.sort((a, b) => alarmFrequencies[b] - alarmFrequencies[a]) : allAlarms;

    const tooltip = document.getElementById('tooltip');

    sortedAlarms.forEach(alarm => {
      const lineDiv = document.createElement('div');
      lineDiv.className = 'alarm-line';

      const lastState = alarmHistories[alarm][alarmHistories[alarm].length - 1];

      const nameDiv = document.createElement('div');
      nameDiv.className = 'alarm-name';
      nameDiv.textContent = alarm;
      nameDiv.title = 'Click to see details';
      nameDiv.style.color = getColorHex(lastState);
      nameDiv.addEventListener('click', () => showPopup(alarm, alarmHistories[alarm]));
      lineDiv.appendChild(nameDiv);

      const timelineDiv = document.createElement('div');
      timelineDiv.className = 'alarm-timeline';

      alarmHistories[alarm].forEach((state, idx) => {
        const block = document.createElement('div');
        block.className = `alarm-block ${getColorClass(state)} alarm-${state}`;
        block.dataset.alarm = alarm;
        block.dataset.state = state;
        block.dataset.time = alarmData[idx].time;

        block.addEventListener('mousemove', e => {
          tooltip.style.opacity = 1;
          tooltip.style.left = e.pageX + 10 + 'px';
          tooltip.style.top = e.pageY + 10 + 'px';
          tooltip.textContent = `${alarm} @ ${alarmData[idx].time}: ${getColorText(state)}`;
        });
        block.addEventListener('mouseleave', () => {
          tooltip.style.opacity = 0;
        });

        timelineDiv.appendChild(block);
      });

      lineDiv.appendChild(timelineDiv);
      container.appendChild(lineDiv);
    });
  }

  function showPopup(alarm, history) {
    const overlay = document.getElementById('overlay');
    const popup = document.getElementById('history-popup');
    const popupTitle = document.getElementById('popup-title');

    popupTitle.textContent = `Alarm Details: ${alarm}`;
    showTab('history');

    popup.currentAlarm = alarm;
    popup.currentHistory = history;

    populateHistoryList(history);
    populateLimits(alarm);

    overlay.style.display = 'block';
    popup.style.display = 'block';
  }

  function populateHistoryList(history) {
    const list = document.getElementById('history-list');
    list.innerHTML = '';
    history.forEach((state, idx) => {
      const li = document.createElement('li');
      li.textContent = `${alarmData[idx].time}: ${getColorText(state)}`;
      li.style.color = getColorHex(state);
      list.appendChild(li);
    });
  }

  function populateLimits(alarm) {
    const limits = alarmLimits[alarm];
    if (!limits) {
      alarmLimits[alarm] = {
        level1: { time: 10, offtime: 5, action: "Notify", value: 100, hysteresis: 5 },
        level2: { time: 20, offtime: 10, action: "Log", value: 200, hysteresis: 10 },
        level3: { time: 30, offtime: 15, action: "Shutdown", value: 300, hysteresis: 15 }
      };
    }
    const l = alarmLimits[alarm];

    document.getElementById('level1-time').value = l.level1.time;
    document.getElementById('level1-offtime').value = l.level1.offtime;
    document.getElementById('level1-action').value = l.level1.action;
    document.getElementById('level1-value').value = l.level1.value;
    document.getElementById('level1-hysteresis').value = l.level1.hysteresis;

    document.getElementById('level2-time').value = l.level2.time;
    document.getElementById('level2-offtime').value = l.level2.offtime;
    document.getElementById('level2-action').value = l.level2.action;
    document.getElementById('level2-value').value = l.level2.value;
    document.getElementById('level2-hysteresis').value = l.level2.hysteresis;

    document.getElementById('level3-time').value = l.level3.time;
    document.getElementById('level3-offtime').value = l.level3.offtime;
    document.getElementById('level3-action').value = l.level3.action;
    document.getElementById('level3-value').value = l.level3.value;
    document.getElementById('level3-hysteresis').value = l.level3.hysteresis;

    setLimitsEditable(false);
  }

  function setLimitsEditable(editable) {
    const inputs = document.querySelectorAll('#limits .limit-input');
    inputs.forEach(input => {
      input.readOnly = !editable;
      if (editable) {
        input.style.background = '#222';
        input.style.borderColor = '#66ccff';
        input.style.color = '#66ccff';
      } else {
        input.style.background = 'transparent';
        input.style.borderColor = 'transparent';
        input.style.color = '#999';
      }
    });
    document.getElementById('edit-limits').style.display = editable ? 'none' : 'inline-block';
    document.getElementById('save-limits').style.display = editable ? 'inline-block' : 'none';
  }

  function showTab(tabName) {
    const tabs = document.querySelectorAll('#popup-tabs .tab');
    const contents = document.querySelectorAll('.tab-content');
    tabs.forEach(tab => {
      tab.classList.toggle('active', tab.dataset.tab === tabName);
    });
    contents.forEach(content => {
      content.classList.toggle('active', content.id === tabName);
    });
  }

  document.getElementById('close-history').addEventListener('click', () => {
    document.getElementById('overlay').style.display = 'none';
    document.getElementById('history-popup').style.display = 'none';
  });

  document.querySelectorAll('#popup-tabs .tab').forEach(tab => {
    tab.addEventListener('click', () => {
      showTab(tab.dataset.tab);
    });
  });

  document.getElementById('edit-limits').addEventListener('click', () => {
    setLimitsEditable(true);
  });

  async function saveLimits(baseUrl) {
    try {
      const response = await fetch(baseUrl.replace(/\/api\/alarms$/, '/api/limits'), {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(alarmLimits),
      });
      if (!response.ok) throw new Error(`HTTP error! status: ${response.status}`);
      const result = await response.json();
      alert('Limits saved successfully for: ' + result.updated.join(', '));
    } catch (err) {
      alert('Error saving limits: ' + err.message);
    }
  }

  document.getElementById('save-limits').addEventListener('click', () => {
    const alarm = document.getElementById('history-popup').currentAlarm;
    if (!alarm) return;

    alarmLimits[alarm].level1.time = Number(document.getElementById('level1-time').value);
    alarmLimits[alarm].level1.offtime = Number(document.getElementById('level1-offtime').value);
    alarmLimits[alarm].level1.action = document.getElementById('level1-action').value;
    alarmLimits[alarm].level1.value = Number(document.getElementById('level1-value').value);
    alarmLimits[alarm].level1.hysteresis = Number(document.getElementById('level1-hysteresis').value);

    alarmLimits[alarm].level2.time = Number(document.getElementById('level2-time').value);
    alarmLimits[alarm].level2.offtime = Number(document.getElementById('level2-offtime').value);
    alarmLimits[alarm].level2.action = document.getElementById('level2-action').value;
    alarmLimits[alarm].level2.value = Number(document.getElementById('level2-value').value);
    alarmLimits[alarm].level2.hysteresis = Number(document.getElementById('level2-hysteresis').value);

    alarmLimits[alarm].level3.time = Number(document.getElementById('level3-time').value);
    alarmLimits[alarm].level3.offtime = Number(document.getElementById('level3-offtime').value);
    alarmLimits[alarm].level3.action = document.getElementById('level3-action').value;
    alarmLimits[alarm].level3.value = Number(document.getElementById('level3-value').value);
    alarmLimits[alarm].level3.hysteresis = Number(document.getElementById('level3-hysteresis').value);

    setLimitsEditable(false);

    const baseUrl = document.getElementById('api-url-input').value.trim();
    if (baseUrl) {
      saveLimits(baseUrl);
    } else {
      alert('Please enter a valid API base URL to save limits.');
    }
  });

  async function fetchLimits(baseUrl) {
    try {
      const response = await fetch(baseUrl.replace(/\/api\/alarms$/, '/api/limits'));
      if (!response.ok) throw new Error(`HTTP error! status: ${response.status}`);
      const limits = await response.json();
      alarmLimits = limits || {};
      const popup = document.getElementById('history-popup');
      if (popup.style.display === 'block' && popup.currentAlarm) {
        populateLimits(popup.currentAlarm);
      }
    } catch (err) {
      console.error('Error fetching limits:', err);
    }
  }

  async function fetchAlarmData(url) {
    errorMessageEl.textContent = '';
    try {
      const response = await fetch(url);
      if (!response.ok) throw new Error(`HTTP error! status: ${response.status}`);
      const json = await response.json();
      if (!json.data || !Array.isArray(json.data)) throw new Error('Invalid data format: expected data array');
      alarmData = alarmData.concat(json.data);
      alarmLimits = alarmLimits || {};
      renderAlarmChart('alarm-container', alarmData);

      const baseUrl = document.getElementById('api-url-input').value.trim();
      if (baseUrl) {
        await fetchLimits(baseUrl);
      }
    } catch (err) {
      errorMessageEl.textContent = `Error fetching data: ${err.message}`;
    }
  }

  function generateRandomSimData() {
    const alarms = ['alarm1', 'alarm2', 'alarm3', 'alarm4', 'alarm5'];
    const data = [];
    const now = Date.now();
    for (let i = 0; i < 10; i++) {
      const time = new Date(now + i * 60000).toISOString();
      const alarmStates = {};
      alarms.forEach(alarm => {
        alarmStates[alarm] = Math.floor(Math.random() * 4);
      });
      data.push({ time, alarms: alarmStates });
    }
    return data;
  }

  function loadSimData() {
    const simData = generateRandomSimData();
    alarmData = alarmData.concat(simData);
    alarmLimits = alarmLimits || {};
    renderAlarmChart('alarm-container', alarmData);
    errorMessageEl.textContent = '';
  }

  document.getElementById('fetch-api-btn').addEventListener('click', () => {
    const url = document.getElementById('api-url-input').value.trim();
    if (url) {
      fetchAlarmData(url);
    } else {
      errorMessageEl.textContent = 'Please enter a valid API URL.';
    }
  });

  document.getElementById('load-sim-btn').addEventListener('click', () => {
    loadSimData();
  });

  document.getElementById('color-switch-btn').addEventListener('click', () => {
    useBlue = !useBlue;
    renderAlarmChart('alarm-container', alarmData);
  });

  document.getElementById('sort-toggle-btn').addEventListener('click', () => {
    sortEnabled = !sortEnabled;
    document.getElementById('sort-toggle-btn').textContent = sortEnabled ? 'Disable Sorting' : 'Enable Sorting';
    renderAlarmChart('alarm-container', alarmData);
  });

})();