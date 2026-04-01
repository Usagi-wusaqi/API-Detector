const state = {
  providers: [],
  jobId: null,
  eventSource: null,
  jobs: [],
  results: [],
  filteredResults: [],
  currentPage: 1,
  lastFormat: "text",
  summary: { total: 0, checked: 0, valid: 0, invalid: 0, error: 0, canceled: 0, keys_per_second: 0 },
};

const storageKey = "apidetect.gui.preferences.v1";
const timeoutUnits = { ms: 1, s: 1000, m: 60 * 1000, h: 60 * 60 * 1000 };
let panelResizeObserver = null;
const collapsedVisibleRows = 13;
const expandedVisibleRows = 20;
const fallbackTableRowHeight = 46;
const expandedHistoryGrowRatio = 1;

const elements = {
  configPanel: document.querySelector(".config-panel"),
  resultsPanel: document.querySelector(".results-panel"),
  historyPanel: document.querySelector(".history-panel"),
  versionBadge: document.getElementById("version-badge"),
  provider: document.getElementById("provider"),
  endpointUrl: document.getElementById("endpoint-url"),
  proxyMode: document.getElementById("proxy-mode"),
  proxyUrl: document.getElementById("proxy-url"),
  proxyUrlWrap: document.getElementById("proxy-url-wrap"),
  timeout: document.getElementById("timeout"),
  concurrency: document.getElementById("concurrency"),
  format: document.getElementById("format"),
  customMethod: document.getElementById("custom-method"),
  customAuthMode: document.getElementById("custom-auth-mode"),
  customHeaders: document.getElementById("custom-headers"),
  customBody: document.getElementById("custom-body"),
  formatAdvancedDrawer: document.getElementById("format-advanced-drawer"),
  proxyAdvancedDrawer: document.getElementById("proxy-advanced-drawer"),
  providerAdvancedDrawer: document.getElementById("provider-advanced-drawer"),
  formatAdvancedTrigger: document.getElementById("format-advanced-trigger"),
  proxyAdvancedTrigger: document.getElementById("proxy-advanced-trigger"),
  providerAdvancedTrigger: document.getElementById("provider-advanced-trigger"),
  formatAdvancedClose: document.getElementById("format-advanced-close"),
  proxyAdvancedClose: document.getElementById("proxy-advanced-close"),
  providerAdvancedClose: document.getElementById("provider-advanced-close"),
  formatAdvancedNote: document.getElementById("format-advanced-note"),
  formatSuffix: document.getElementById("format-suffix"),
  proxyModeNote: document.getElementById("proxy-mode-note"),
  providerAdvancedNote: document.getElementById("provider-advanced-note"),
  spinboxButtons: Array.from(document.querySelectorAll(".spinbox-button")),
  customMethodWrap: document.getElementById("custom-method-wrap"),
  customAuthWrap: document.getElementById("custom-auth-wrap"),
  customHeadersWrap: document.getElementById("custom-headers-wrap"),
  customBodyWrap: document.getElementById("custom-body-wrap"),
  keys: document.getElementById("keys"),
  start: document.getElementById("start"),
  cancel: document.getElementById("cancel"),
  exportReport: document.getElementById("export-report"),
  exportValid: document.getElementById("export-valid"),
  exportInvalid: document.getElementById("export-invalid"),
  exportError: document.getElementById("export-error"),
  importFile: document.getElementById("import-file"),
  resetSettings: document.getElementById("reset-settings"),
  fileInput: document.getElementById("file-input"),
  resultsBody: document.getElementById("results-body"),
  statusPill: document.getElementById("status-pill"),
  statTotal: document.getElementById("stat-total"),
  statValid: document.getElementById("stat-valid"),
  statInvalid: document.getElementById("stat-invalid"),
  statError: document.getElementById("stat-error"),
  summaryRate: document.getElementById("summary-rate"),
  progressFill: document.getElementById("progress-fill"),
  progressText: document.getElementById("progress-text"),
  statusFilter: document.getElementById("status-filter"),
  sortOrder: document.getElementById("sort-order"),
  searchQuery: document.getElementById("search-query"),
  pageSize: document.getElementById("page-size"),
  pagePrev: document.getElementById("page-prev"),
  pageNext: document.getElementById("page-next"),
  pageInfo: document.getElementById("page-info"),
  jobHistory: document.getElementById("job-history"),
  historyCount: document.getElementById("history-count"),
  refreshHistory: document.getElementById("refresh-history"),
  clearHistory: document.getElementById("clear-history"),
  toggleHistory: document.getElementById("toggle-history"),
  banner: document.getElementById("banner"),
};

async function loadMeta() {
  const response = await fetch("/api/meta");
  if (!response.ok) return;
  const meta = await response.json();
  if (meta.version) {
    document.title = `API Detector GUI ${meta.version}`;
    elements.versionBadge.textContent = meta.version;
  }
}

async function loadProviders() {
  const response = await fetch("/api/providers");
  const providers = await response.json();
  state.providers = providers;

  for (const provider of providers) {
    const option = document.createElement("option");
    option.value = provider.name;
    option.textContent = provider.label || provider.name;
    elements.provider.appendChild(option);
  }
}

function currentProviderMeta() {
  return state.providers.find((provider) => provider.name === elements.provider.value) || null;
}

function emitInputEvent(element) {
  element.dispatchEvent(new Event("input", { bubbles: true }));
}

function adjustConcurrency(direction) {
  const current = Number.parseInt(elements.concurrency.value, 10);
  const base = Number.isFinite(current) && current > 0 ? current : 1;
  elements.concurrency.value = String(Math.max(1, base + direction));
  emitInputEvent(elements.concurrency);
}

function parseTimeoutValue(value) {
  const normalized = String(value || "").trim().toLowerCase().replace(/\s+/g, "");
  if (!normalized) {
    return null;
  }

  const tokenPattern = /(\d+(?:\.\d+)?)(ms|s|m|h)/g;
  let totalMilliseconds = 0;
  let lastIndex = 0;
  let lastUnit = "s";
  let matched = false;
  let match = tokenPattern.exec(normalized);

  while (match) {
    if (match.index !== lastIndex) {
      return null;
    }
    matched = true;
    totalMilliseconds += Number(match[1]) * timeoutUnits[match[2]];
    lastIndex = tokenPattern.lastIndex;
    lastUnit = match[2];
    match = tokenPattern.exec(normalized);
  }

  if (matched && lastIndex === normalized.length) {
    return { milliseconds: totalMilliseconds, unit: lastUnit };
  }

  const numericValue = Number(normalized);
  if (Number.isFinite(numericValue) && numericValue > 0) {
    return { milliseconds: numericValue * timeoutUnits.s, unit: "s" };
  }

  return null;
}

function formatTimeoutValue(milliseconds, unit) {
  const normalizedUnit = timeoutUnits[unit] ? unit : "s";
  const divisor = timeoutUnits[normalizedUnit];
  const rawValue = milliseconds / divisor;
  const displayValue = Number.isInteger(rawValue) ? rawValue : Number(rawValue.toFixed(3));
  return `${displayValue}${normalizedUnit}`;
}

function adjustTimeout(direction) {
  const parsed = parseTimeoutValue(elements.timeout.value);
  const unit = parsed?.unit || "s";
  const stepMilliseconds = timeoutUnits[unit] || timeoutUnits.s;
  const baseMilliseconds = parsed?.milliseconds || (10 * timeoutUnits.s);
  const nextMilliseconds = Math.max(stepMilliseconds, baseMilliseconds + (direction * stepMilliseconds));
  elements.timeout.value = formatTimeoutValue(nextMilliseconds, unit);
  emitInputEvent(elements.timeout);
}

function stepField(target, direction) {
  if (target === "concurrency") {
    adjustConcurrency(direction);
    return;
  }
  if (target === "timeout") {
    adjustTimeout(direction);
  }
}

function syncResultsPanelHeight() {
  const configPanel = elements.configPanel;
  const resultsPanel = elements.resultsPanel;
  const historyPanel = elements.historyPanel;
  const historyList = elements.jobHistory;
  if (!configPanel || !resultsPanel) return;

  if (window.matchMedia("(max-width: 1100px)").matches) {
    configPanel.style.minHeight = "";
    resultsPanel.style.height = "";
    resultsPanel.style.minHeight = "";
    if (historyPanel) {
      historyPanel.style.minHeight = "";
    }
    if (historyList) {
      historyList.style.maxHeight = "";
    }
    return;
  }

  const tableWrap = resultsPanel.querySelector(".results-table-wrap");
  const headerRow = resultsPanel.querySelector(".results-table thead tr");
  const bodyRow = resultsPanel.querySelector(".results-table tbody tr");
  const historyExpanded = elements.historyPanel && !elements.historyPanel.classList.contains("collapsed");
  const targetRows = historyExpanded ? expandedVisibleRows : collapsedVisibleRows;
  const rowHeight = bodyRow ? bodyRow.getBoundingClientRect().height : fallbackTableRowHeight;
  const headerHeight = headerRow ? headerRow.getBoundingClientRect().height : rowHeight;

  if (historyPanel) {
    historyPanel.style.minHeight = "";
  }
  if (historyList) {
    historyList.style.maxHeight = "";
  }

  configPanel.style.minHeight = "";
  let naturalLeftHeight = Math.ceil(configPanel.getBoundingClientRect().height);

  if (tableWrap) {
    const panelHeight = resultsPanel.getBoundingClientRect().height || resultsPanel.offsetHeight;
    const tableHeight = tableWrap.getBoundingClientRect().height || tableWrap.offsetHeight;
    const nonTableHeight = Math.max(0, Math.ceil(panelHeight - tableHeight));
    const requiredTableHeight = Math.ceil(headerHeight + (rowHeight * targetRows));
    const requiredPanelHeight = nonTableHeight + requiredTableHeight;

    const extraSpace = Math.max(0, requiredPanelHeight - naturalLeftHeight);
    if (historyExpanded && extraSpace > 0 && historyPanel && historyList) {
      const allocatedSpace = Math.round(extraSpace * expandedHistoryGrowRatio);
      const baseHistoryHeight = Math.ceil(historyPanel.getBoundingClientRect().height);
      historyPanel.style.minHeight = `${baseHistoryHeight + allocatedSpace}px`;
      historyList.style.maxHeight = `${220 + allocatedSpace}px`;
      naturalLeftHeight = Math.ceil(configPanel.getBoundingClientRect().height);
    }

    const finalRequiredHeight = Math.max(requiredPanelHeight, naturalLeftHeight);
    configPanel.style.minHeight = `${finalRequiredHeight}px`;
  }

  const leftHeight = Math.ceil(configPanel.getBoundingClientRect().height);
  if (leftHeight > 0) {
    resultsPanel.style.height = `${leftHeight}px`;
    resultsPanel.style.minHeight = `${leftHeight}px`;
  }
}

function setupPanelHeightSync() {
  if (!elements.configPanel || !elements.resultsPanel) return;
  syncResultsPanelHeight();

  if (typeof ResizeObserver !== "undefined") {
    panelResizeObserver = new ResizeObserver(() => {
      syncResultsPanelHeight();
    });
    panelResizeObserver.observe(elements.configPanel);
  }

  window.addEventListener("resize", syncResultsPanelHeight);
}

function setRunning(running) {
  elements.start.disabled = running;
  elements.cancel.disabled = !running;
  elements.importFile.disabled = running;
  elements.provider.disabled = running;
  elements.keys.disabled = running;
  elements.statusPill.textContent = running ? "检测中" : "就绪";
  elements.statusPill.className = `status-pill ${running ? "running" : "idle"}`;
}

function updateSummary(summary) {
  state.summary = summary;
  elements.statTotal.textContent = summary.total ?? 0;
  elements.statValid.textContent = summary.valid ?? 0;
  elements.statInvalid.textContent = summary.invalid ?? 0;
  elements.statError.textContent = summary.error ?? 0;
  elements.summaryRate.textContent = `${(summary.keys_per_second ?? 0).toFixed(2)} keys/s`;
  elements.progressText.textContent = `${summary.checked ?? 0} / ${summary.total ?? 0}`;
  const progress = summary.total ? ((summary.checked / summary.total) * 100).toFixed(2) : 0;
  elements.progressFill.style.width = `${progress}%`;
}

function renderResults() {
  elements.resultsBody.innerHTML = "";
  const query = elements.searchQuery.value.trim().toLowerCase();
  const status = elements.statusFilter.value;
  const sort = elements.sortOrder.value;

  state.filteredResults = state.results.filter((result) => {
    if (status !== "all" && result.status !== status) {
      return false;
    }
    if (!query) {
      return true;
    }
    return [
      result.index,
      result.masked_key,
      result.key,
      result.status,
      result.message,
      result.http_status,
      result.latency_ms,
    ].some((value) => String(value || "").toLowerCase().includes(query));
  });

  state.filteredResults.sort((left, right) => {
    if (sort === "latency_desc") return (right.latency_ms || 0) - (left.latency_ms || 0);
    if (sort === "latency_asc") return (left.latency_ms || 0) - (right.latency_ms || 0);
    return (left.index || 0) - (right.index || 0);
  });

  const pageSize = Number(elements.pageSize.value);
  const totalPages = Math.max(1, Math.ceil(state.filteredResults.length / pageSize));
  state.currentPage = Math.max(1, Math.min(state.currentPage, totalPages));
  const start = (state.currentPage - 1) * pageSize;
  const end = start + pageSize;

  const pageResults = state.filteredResults.slice(start, end);
  for (const [offset, result] of pageResults.entries()) {
    const serial = start + offset + 1;
    const row = document.createElement("tr");
    row.innerHTML = `
      <td>${serial}</td>
      <td class="status-${result.status}">${result.status}</td>
      <td><code>${result.masked_key}</code></td>
      <td>${result.message}</td>
      <td>${result.http_status ?? 0}</td>
      <td>${result.latency_ms ?? 0}ms</td>
    `;
    elements.resultsBody.appendChild(row);
  }

  elements.exportReport.disabled = !state.jobId;
  elements.exportValid.disabled = state.results.every((item) => item.status !== "valid");
  elements.exportInvalid.disabled = state.results.every((item) => item.status !== "invalid");
  elements.exportError.disabled = state.results.every((item) => item.status !== "error");
  elements.pagePrev.disabled = state.currentPage <= 1;
  elements.pageNext.disabled = state.currentPage >= totalPages;
  elements.pageInfo.textContent = `第 ${state.currentPage} / ${totalPages} 页`;
  syncResultsPanelHeight();
}

function renderHistory() {
  elements.historyCount.textContent = state.jobs.length;
  elements.jobHistory.innerHTML = "";
  if (!state.jobs.length) {
    elements.jobHistory.textContent = "暂无任务";
    elements.jobHistory.classList.add("empty");
    return;
  }

  elements.jobHistory.classList.remove("empty");
  for (const job of state.jobs) {
    const button = document.createElement("button");
    button.className = `job-card${job.id === state.jobId ? " active" : ""}`;
    button.innerHTML = `
      <div class="job-card-row">
        <span>${job.status}</span>
        <span>${new Date(job.started_at).toLocaleString()}</span>
      </div>
      <strong>${job.summary.checked}/${job.summary.total} checked</strong>
      <div class="job-card-row">
        <span>valid ${job.summary.valid}</span>
        <span>invalid ${job.summary.invalid}</span>
        <span>error ${job.summary.error}</span>
      </div>
    `;
    button.addEventListener("click", () => loadJobSnapshot(job.id));
    elements.jobHistory.appendChild(button);
  }
}

function toggleHistory() {
  const panel = document.querySelector(".history-panel");
  panel.classList.toggle("collapsed");
  const collapsed = panel.classList.contains("collapsed");
  elements.toggleHistory.textContent = collapsed ? "展开" : "收起";
  syncResultsPanelHeight();
}

function defaultSuffixForFormat(format) {
  if (format === "custom") {
    return "log";
  }
  return format === "json" ? "json" : "txt";
}

function effectiveExportFormat() {
  if (elements.format.value !== "custom") {
    return elements.format.value;
  }
  const suffix = normalizeSuffix(elements.formatSuffix.value);
  return suffix === "json" ? "json" : "text";
}

function normalizeSuffix(value) {
  const trimmed = String(value || "").trim().replace(/^\.+/, "");
  const safe = trimmed.replace(/[^A-Za-z0-9_-]/g, "");
  return safe.toLowerCase();
}

function activeSuffix() {
  if (elements.format.value === "custom") {
    const custom = normalizeSuffix(elements.formatSuffix.value);
    if (custom) {
      return custom;
    }
  }
  return defaultSuffixForFormat(elements.format.value);
}

function buildFilename(baseName) {
  return `${baseName}.${activeSuffix()}`;
}

function buildStatusExportPayload(status) {
  const targetResults = state.results.filter((result) => result.status === status);
  if (effectiveExportFormat() === "json") {
    return {
      content: JSON.stringify(targetResults.map((item) => item.key), null, 2),
      contentType: "application/json;charset=utf-8",
    };
  }
  return {
    content: `${targetResults.map((item) => item.key).join("\n")}${targetResults.length ? "\n" : ""}`,
    contentType: "text/plain;charset=utf-8",
  };
}

function downloadContent(content, filename, contentType) {
  const blob = new Blob([content], { type: contentType });
  const url = URL.createObjectURL(blob);
  const anchor = document.createElement("a");
  anchor.href = url;
  anchor.download = filename;
  anchor.click();
  URL.revokeObjectURL(url);
}

function downloadStatus(status, baseName) {
  if (!state.jobId) return;
  const payload = buildStatusExportPayload(status);
  downloadContent(payload.content, buildFilename(baseName), payload.contentType);
}

function downloadReport() {
  if (!state.jobId) return;
  if (effectiveExportFormat() === "json") {
    const payload = {
      id: state.jobId,
      summary: state.summary,
      results: state.results,
      exported_at: new Date().toISOString(),
    };
    downloadContent(JSON.stringify(payload, null, 2), buildFilename(`job_${state.jobId}_report`), "application/json;charset=utf-8");
    return;
  }

  const lines = [
    `job_id: ${state.jobId}`,
    `exported_at: ${new Date().toISOString()}`,
    `summary: total=${state.summary.total} checked=${state.summary.checked} valid=${state.summary.valid} invalid=${state.summary.invalid} error=${state.summary.error}`,
    "",
    "results:",
  ];
  for (const item of state.results) {
    lines.push(`${item.index}. [${item.status}] ${item.key} http=${item.http_status ?? 0} latency=${item.latency_ms ?? 0}ms ${item.message || ""}`);
  }
  downloadContent(`${lines.join("\n")}\n`, buildFilename(`job_${state.jobId}_report`), "text/plain;charset=utf-8");
}

function downloadUrl(url, filename) {
  const anchor = document.createElement("a");
  anchor.href = url;
  anchor.download = filename;
  anchor.click();
}

function appendResult(result) {
  state.results.push(result);
  state.results.sort((a, b) => a.index - b.index);
  renderResults();
}

function showBanner(message, type = "info") {
  if (!message) {
    elements.banner.textContent = "";
    elements.banner.className = "banner hidden";
    return;
  }
  elements.banner.textContent = message;
  elements.banner.className = `banner ${type}`;
}

function closeEvents() {
  if (state.eventSource) {
    state.eventSource.close();
    state.eventSource = null;
  }
}

function connectEvents(jobId) {
  closeEvents();
  const source = new EventSource(`/api/jobs/${jobId}/events`);
  state.eventSource = source;

  source.addEventListener("snapshot", (event) => {
    const payload = JSON.parse(event.data);
    state.results = payload.results || [];
    updateSummary(payload.summary || state.summary);
    renderResults();
  });

  source.addEventListener("result", (event) => {
    const payload = JSON.parse(event.data);
    appendResult(payload.result);
    updateSummary(payload.summary);
  });

  source.addEventListener("complete", (event) => {
    const payload = JSON.parse(event.data);
    updateSummary(payload.summary);
    elements.statusPill.textContent = "已完成";
    elements.statusPill.className = "status-pill done";
    setRunning(false);
    closeEvents();
    showBanner("检测完成。", "success");
  });

  source.onerror = () => {
    closeEvents();
    setRunning(false);
    showBanner("与本地检测服务的事件连接已断开。", "error");
  };
}

async function startJob() {
  const validationError = validateForm();
  if (validationError) {
    showBanner(validationError, "error");
    return;
  }

  state.results = [];
  state.currentPage = 1;
  renderResults();
  updateSummary({ total: 0, checked: 0, valid: 0, invalid: 0, error: 0, canceled: 0, keys_per_second: 0 });
  setRunning(true);
  showBanner("检测任务已创建，正在执行。", "info");

  const payload = {
    provider: elements.provider.value,
    keys: elements.keys.value,
    concurrency: Number(elements.concurrency.value),
    timeout: elements.timeout.value,
    proxy_mode: elements.proxyMode.value,
    proxy_url: elements.proxyUrl.value,
    custom_url: elements.endpointUrl.value.trim(),
    custom_method: elements.customMethod.value,
    custom_auth_mode: elements.customAuthMode.value,
    custom_headers: elements.customHeaders.value,
    custom_body: elements.customBody.value,
  };

  const response = await fetch("/api/jobs", {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify(payload),
  });

  if (!response.ok) {
    const text = await response.text();
    setRunning(false);
    showBanner(text || "创建任务失败", "error");
    return;
  }

  const data = await response.json();
  state.jobId = data.id;
  await refreshHistory();
  connectEvents(state.jobId);
}

async function cancelJob() {
  if (!state.jobId) return;
  await fetch(`/api/jobs/${state.jobId}/cancel`, { method: "POST" });
  showBanner("已请求取消当前任务。", "info");
}

async function refreshHistory() {
  const response = await fetch("/api/jobs");
  if (!response.ok) return;
  const jobs = await response.json();
  state.jobs = Array.isArray(jobs) ? jobs : [];
  state.jobs.sort((left, right) => new Date(right.started_at).getTime() - new Date(left.started_at).getTime());
  renderHistory();
}

async function clearHistory() {
  const response = await fetch("/api/jobs", { method: "DELETE" });
  if (!response.ok) {
    showBanner("清理历史失败。", "error");
    return;
  }
  const payload = await response.json();
  state.jobs = [];
  renderHistory();
  showBanner(`已清理 ${payload.removed ?? 0} 条历史任务。`, "success");
}

async function loadJobSnapshot(jobId) {
  const snapshotResponse = await fetch(`/api/jobs/${jobId}`);
  if (!snapshotResponse.ok) return;

  const snapshot = await snapshotResponse.json();
  state.jobId = snapshot.id;
  state.results = snapshot.results || [];
  updateSummary(snapshot.summary || state.summary);
  renderResults();
  renderHistory();

  if (snapshot.status === "running") {
    setRunning(true);
    connectEvents(snapshot.id);
  } else {
    setRunning(false);
    elements.statusPill.textContent = snapshot.status === "done" ? "已完成" : snapshot.status;
    elements.statusPill.className = "status-pill done";
    showBanner("已加载历史任务。", "success");
  }
}

async function restoreLatestJob() {
  await refreshHistory();
  if (!state.jobs.length) return;
  await loadJobSnapshot(state.jobs[0].id);
}

function wireFileImport() {
  elements.importFile.addEventListener("click", () => elements.fileInput.click());
  elements.fileInput.addEventListener("change", async (event) => {
    const file = event.target.files?.[0];
    if (!file) return;
    elements.keys.value = await file.text();
  });
}

function wireSpinboxes() {
  for (const button of elements.spinboxButtons) {
    button.addEventListener("click", () => {
      const direction = button.dataset.direction === "down" ? -1 : 1;
      stepField(button.dataset.target, direction);
      document.getElementById(button.dataset.target)?.focus({ preventScroll: true });
    });
  }

  [
    { element: elements.concurrency, target: "concurrency" },
    { element: elements.timeout, target: "timeout" },
  ].forEach(({ element, target }) => {
    element.addEventListener("keydown", (event) => {
      if (event.key !== "ArrowUp" && event.key !== "ArrowDown") {
        return;
      }
      event.preventDefault();
      stepField(target, event.key === "ArrowDown" ? -1 : 1);
    });
  });
}

function resetSettings() {
  localStorage.removeItem(storageKey);
  elements.provider.value = "custom";
  elements.endpointUrl.value = "";
  elements.concurrency.value = "100";
  elements.timeout.value = "10s";
  elements.format.value = "text";
  elements.formatSuffix.value = defaultSuffixForFormat(elements.format.value);
  elements.pageSize.value = "50";
  elements.proxyMode.value = "env";
  elements.proxyUrl.value = "";
  elements.customMethod.value = "GET";
  elements.customAuthMode.value = "bearer";
  elements.customHeaders.value = "";
  elements.customBody.value = "";
  elements.statusFilter.value = "all";
  elements.sortOrder.value = "index";
  elements.searchQuery.value = "";
  state.currentPage = 1;
  syncCustomPanel();
  syncEndpointField();
  syncProxyPanel();
  applyFormatChange(elements.format.value);
  elements.formatAdvancedDrawer.classList.add("hidden");
  elements.providerAdvancedDrawer.classList.add("hidden");
  elements.proxyAdvancedDrawer.classList.add("hidden");
  syncOutputAdvancedPanel();
  syncProviderAdvancedPanel();
  syncProxyAdvancedPanel();
  renderResults();
  showBanner("已重置界面设置。", "success");
}

function syncCustomPanel() {
  const isCustom = elements.provider.value === "custom";
  elements.customMethodWrap.classList.toggle("hidden", !isCustom);
  elements.customAuthWrap.classList.toggle("hidden", !isCustom);
  elements.customHeadersWrap.classList.toggle("hidden", !isCustom);
  elements.customBodyWrap.classList.toggle("hidden", !isCustom);
  const provider = currentProviderMeta();
  if (isCustom) {
    elements.providerAdvancedNote.textContent = "以下设置均为可选，不填将使用默认请求行为。";
  } else if (provider) {
    elements.providerAdvancedNote.textContent = `当前为 ${provider.label || provider.name}，接口与请求方式按预设展示。`;
  } else {
    elements.providerAdvancedNote.textContent = "当前供应商使用预设配置。";
  }
  syncProviderAdvancedPanel();
}

function syncEndpointField() {
  const provider = currentProviderMeta();
  if (!provider) return;

  const isCustom = provider.name === "custom";
  elements.endpointUrl.readOnly = !isCustom;
  if (isCustom) {
    elements.endpointUrl.placeholder = provider.notes || "请输入自定义供应商接口地址";
  } else {
    elements.endpointUrl.value = provider.url || "";
    elements.endpointUrl.placeholder = "";
  }
}

function syncProxyPanel() {
  const isCustomProxy = elements.proxyMode.value === "custom";
  if (isCustomProxy && !elements.proxyUrl.value.trim()) {
    elements.proxyUrl.value = "http://127.0.0.1:8080";
  }
  elements.proxyUrl.disabled = !isCustomProxy;
  elements.proxyModeNote.textContent = isCustomProxy
    ? "仅在自定义模式下使用代理地址。"
    : "当前模式不会使用代理地址；如需手动代理，请切换为自定义模式。";
  syncProxyAdvancedPanel();
}

function syncProviderAdvancedPanel() {
  const expanded = !elements.providerAdvancedDrawer.classList.contains("hidden");
  elements.providerAdvancedTrigger.textContent = expanded ? "收起高级设置" : "高级设置";
  syncModalBodyLock();
}

function syncProxyAdvancedPanel() {
  const expanded = !elements.proxyAdvancedDrawer.classList.contains("hidden");
  elements.proxyAdvancedTrigger.textContent = expanded ? "收起高级设置" : "高级设置";
  syncModalBodyLock();
}

function syncOutputAdvancedPanel() {
  const isCustomFormat = elements.format.value === "custom";
  elements.formatAdvancedTrigger.classList.remove("hidden");
  elements.formatSuffix.disabled = !isCustomFormat;
  const expanded = !elements.formatAdvancedDrawer.classList.contains("hidden");
  elements.formatAdvancedTrigger.textContent = expanded ? "收起高级设置" : "高级设置";
  if (isCustomFormat) {
    elements.formatAdvancedNote.textContent = "仅在自定义输出格式下生效；可通过尾缀控制导出文件类型。";
  } else {
    const defaultSuffix = defaultSuffixForFormat(elements.format.value);
    elements.formatAdvancedNote.textContent = `当前固定使用 .${defaultSuffix}，高级设置仅在自定义格式下可用。`;
  }
  syncModalBodyLock();
}

function applyFormatChange(newFormat) {
  const previousFormat = state.lastFormat || newFormat;
  const currentSuffix = normalizeSuffix(elements.formatSuffix.value);
  if (newFormat === "custom" && (!currentSuffix || currentSuffix === defaultSuffixForFormat(previousFormat))) {
    elements.formatSuffix.value = defaultSuffixForFormat(newFormat);
  }
  state.lastFormat = newFormat;
  syncOutputAdvancedPanel();
}

function closeFormatAdvancedPanel() {
  elements.formatAdvancedDrawer.classList.add("hidden");
  syncOutputAdvancedPanel();
}

function syncModalBodyLock() {
  const hasOpenModal = !elements.formatAdvancedDrawer.classList.contains("hidden")
    || !elements.providerAdvancedDrawer.classList.contains("hidden")
    || !elements.proxyAdvancedDrawer.classList.contains("hidden");
  document.body.classList.toggle("modal-open", hasOpenModal);
}

function closeProviderAdvancedPanel() {
  elements.providerAdvancedDrawer.classList.add("hidden");
  syncProviderAdvancedPanel();
}

function closeProxyAdvancedPanel() {
  elements.proxyAdvancedDrawer.classList.add("hidden");
  syncProxyAdvancedPanel();
}

function closeAllAdvancedPanels() {
  elements.formatAdvancedDrawer.classList.add("hidden");
  elements.providerAdvancedDrawer.classList.add("hidden");
  elements.proxyAdvancedDrawer.classList.add("hidden");
  syncOutputAdvancedPanel();
  syncProviderAdvancedPanel();
  syncProxyAdvancedPanel();
}

function savePreferences() {
  localStorage.setItem(storageKey, JSON.stringify({
    provider: elements.provider.value,
    endpointUrl: elements.endpointUrl.value,
    concurrency: elements.concurrency.value,
    timeout: elements.timeout.value,
    format: elements.format.value,
    formatSuffix: normalizeSuffix(elements.formatSuffix.value),
    pageSize: elements.pageSize.value,
    proxyMode: elements.proxyMode.value,
    proxyUrl: elements.proxyUrl.value,
    customMethod: elements.customMethod.value,
    customAuthMode: elements.customAuthMode.value,
    customHeaders: elements.customHeaders.value,
    customBody: elements.customBody.value,
    statusFilter: elements.statusFilter.value,
    sortOrder: elements.sortOrder.value,
    searchQuery: elements.searchQuery.value,
  }));
}

function restorePreferences() {
  const raw = localStorage.getItem(storageKey);
  if (!raw) return;

  try {
    const payload = JSON.parse(raw);
    elements.provider.value = payload.provider || elements.provider.value;
    elements.endpointUrl.value = payload.endpointUrl || "";
    elements.concurrency.value = payload.concurrency || elements.concurrency.value;
    elements.timeout.value = payload.timeout || elements.timeout.value;
    elements.format.value = payload.format || elements.format.value;
    elements.formatSuffix.value = payload.formatSuffix || "";
    elements.pageSize.value = payload.pageSize || elements.pageSize.value;
    elements.proxyMode.value = payload.proxyMode || elements.proxyMode.value;
    elements.proxyUrl.value = payload.proxyUrl || "";
    elements.customMethod.value = payload.customMethod || elements.customMethod.value;
    elements.customAuthMode.value = payload.customAuthMode || elements.customAuthMode.value;
    elements.customHeaders.value = payload.customHeaders || "";
    elements.customBody.value = payload.customBody || "";
    elements.statusFilter.value = payload.statusFilter || elements.statusFilter.value;
    elements.sortOrder.value = payload.sortOrder || elements.sortOrder.value;
    elements.searchQuery.value = payload.searchQuery || "";
  } catch (error) {
    console.warn("Failed to restore preferences", error);
  }
}

function wireActions() {
  elements.start.addEventListener("click", startJob);
  elements.cancel.addEventListener("click", cancelJob);
  elements.exportReport.addEventListener("click", downloadReport);
  elements.exportValid.addEventListener("click", () => downloadStatus("valid", "valid_keys"));
  elements.exportInvalid.addEventListener("click", () => downloadStatus("invalid", "invalid_keys"));
  elements.exportError.addEventListener("click", () => downloadStatus("error", "error_keys"));
  elements.refreshHistory.addEventListener("click", refreshHistory);
  elements.clearHistory.addEventListener("click", clearHistory);
  elements.toggleHistory.addEventListener("click", toggleHistory);
  elements.formatAdvancedTrigger.addEventListener("click", () => {
    elements.formatAdvancedDrawer.classList.toggle("hidden");
    syncOutputAdvancedPanel();
  });
  elements.providerAdvancedTrigger.addEventListener("click", () => {
    elements.providerAdvancedDrawer.classList.toggle("hidden");
    syncProviderAdvancedPanel();
  });
  elements.proxyAdvancedTrigger.addEventListener("click", () => {
    elements.proxyAdvancedDrawer.classList.toggle("hidden");
    syncProxyAdvancedPanel();
  });
  elements.formatAdvancedClose?.addEventListener("click", closeFormatAdvancedPanel);
  elements.providerAdvancedClose?.addEventListener("click", closeProviderAdvancedPanel);
  elements.proxyAdvancedClose?.addEventListener("click", closeProxyAdvancedPanel);
  elements.formatAdvancedDrawer.addEventListener("click", (event) => {
    if (event.target === elements.formatAdvancedDrawer) {
      closeFormatAdvancedPanel();
    }
  });
  elements.providerAdvancedDrawer.addEventListener("click", (event) => {
    if (event.target === elements.providerAdvancedDrawer) {
      closeProviderAdvancedPanel();
    }
  });
  elements.proxyAdvancedDrawer.addEventListener("click", (event) => {
    if (event.target === elements.proxyAdvancedDrawer) {
      closeProxyAdvancedPanel();
    }
  });
  document.addEventListener("keydown", (event) => {
    if (event.key === "Escape") {
      closeAllAdvancedPanels();
    }
  });
  elements.resetSettings.addEventListener("click", resetSettings);
  elements.statusFilter.addEventListener("change", () => { state.currentPage = 1; savePreferences(); renderResults(); });
  elements.sortOrder.addEventListener("change", () => { state.currentPage = 1; savePreferences(); renderResults(); });
  elements.searchQuery.addEventListener("input", () => { state.currentPage = 1; savePreferences(); renderResults(); });
  elements.pageSize.addEventListener("change", () => { state.currentPage = 1; savePreferences(); renderResults(); });
  elements.pagePrev.addEventListener("click", () => {
    if (state.currentPage > 1) {
      state.currentPage -= 1;
      renderResults();
    }
  });
  elements.pageNext.addEventListener("click", () => {
    state.currentPage += 1;
    renderResults();
  });

  [
    elements.provider,
    elements.endpointUrl,
    elements.concurrency,
    elements.timeout,
    elements.format,
    elements.proxyMode,
    elements.proxyUrl,
    elements.customMethod,
    elements.customAuthMode,
    elements.customHeaders,
    elements.customBody,
  ].forEach((element) => {
    element.addEventListener("change", () => {
      if (element === elements.provider) {
        syncCustomPanel();
        syncEndpointField();
      }
      if (element === elements.proxyMode) {
        syncProxyPanel();
      }
      if (element === elements.format) {
        applyFormatChange(elements.format.value);
      }
      savePreferences();
    });
    element.addEventListener("input", savePreferences);
  });

  elements.formatSuffix.addEventListener("input", () => {
    elements.formatSuffix.value = normalizeSuffix(elements.formatSuffix.value);
    savePreferences();
    syncOutputAdvancedPanel();
  });
}

function validateForm() {
  if (!elements.keys.value.trim()) {
    return "请至少输入一个 key。";
  }
  if (!Number.isInteger(Number(elements.concurrency.value)) || Number(elements.concurrency.value) < 1) {
    return "并发数必须是大于 0 的整数。";
  }
  if (!elements.timeout.value.trim()) {
    return "请填写超时。";
  }
  if (elements.provider.value === "custom" && !elements.endpointUrl.value.trim()) {
    return "自定义供应商必须填写接口地址。";
  }
  if (elements.proxyMode.value === "custom" && !elements.proxyUrl.value.trim()) {
    return "自定义代理模式必须填写代理地址。";
  }
  return "";
}

async function init() {
  await loadMeta();
  await loadProviders();
  restorePreferences();
  if (!elements.formatSuffix.value) {
    elements.formatSuffix.value = defaultSuffixForFormat(elements.format.value);
  }
  state.lastFormat = elements.format.value;
  if (!elements.provider.value) {
    elements.provider.value = "custom";
  }
  syncCustomPanel();
  syncEndpointField();
  syncProxyPanel();
  applyFormatChange(elements.format.value);
  elements.formatAdvancedDrawer.classList.add("hidden");
  elements.providerAdvancedDrawer.classList.add("hidden");
  elements.proxyAdvancedDrawer.classList.add("hidden");
  syncOutputAdvancedPanel();
  syncProviderAdvancedPanel();
  syncProxyAdvancedPanel();
  document.querySelector(".history-panel")?.classList.add("collapsed");
  elements.toggleHistory.textContent = "展开";
  wireFileImport();
  wireSpinboxes();
  wireActions();
  setupPanelHeightSync();
  updateSummary(state.summary);
  renderResults();
  showBanner("", "info");
  await restoreLatestJob();
}

init().catch((error) => {
  console.error(error);
  showBanner(`初始化失败: ${error.message}`, "error");
});
