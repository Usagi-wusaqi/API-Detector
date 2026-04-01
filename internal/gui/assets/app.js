const state = {
  providers: [],
  jobId: null,
  eventSource: null,
  jobs: [],
  results: [],
  filteredResults: [],
  currentPage: 1,
  summary: { total: 0, checked: 0, valid: 0, invalid: 0, error: 0, canceled: 0, keys_per_second: 0 },
};

const storageKey = "apidetect.gui.preferences.v1";

const elements = {
  versionBadge: document.getElementById("version-badge"),
  provider: document.getElementById("provider"),
  concurrency: document.getElementById("concurrency"),
  timeout: document.getElementById("timeout"),
  format: document.getElementById("format"),
  proxyMode: document.getElementById("proxy-mode"),
  proxyUrl: document.getElementById("proxy-url"),
  proxyUrlWrap: document.getElementById("proxy-url-wrap"),
  customUrl: document.getElementById("custom-url"),
  customMethod: document.getElementById("custom-method"),
  customAuthMode: document.getElementById("custom-auth-mode"),
  customHeaders: document.getElementById("custom-headers"),
  customBody: document.getElementById("custom-body"),
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
  customPanel: document.getElementById("custom-panel"),
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
    option.textContent = provider.aliases?.length
      ? `${provider.name} (${provider.aliases.join(", ")})`
      : provider.name;
    elements.provider.appendChild(option);
  }
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
      result.masked_key,
      result.key,
      result.status,
      result.reason,
      result.message,
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

  for (const result of state.filteredResults.slice(start, end)) {
    const row = document.createElement("tr");
    row.innerHTML = `
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
}

function downloadStatus(status, filename) {
  if (!state.jobId) return;
  const url = `/api/jobs/${state.jobId}/results?status=${encodeURIComponent(status)}`;
  downloadUrl(url, filename);
}

function downloadReport() {
  if (!state.jobId) return;
  downloadUrl(`/api/jobs/${state.jobId}/report`, `job_${state.jobId}_report.json`);
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
    custom_url: elements.customUrl.value,
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

function resetSettings() {
  localStorage.removeItem(storageKey);
  elements.provider.value = "openai";
  elements.concurrency.value = "100";
  elements.timeout.value = "10s";
  elements.format.value = "text";
  elements.pageSize.value = "50";
  elements.proxyMode.value = "env";
  elements.proxyUrl.value = "";
  elements.customUrl.value = "";
  elements.customMethod.value = "GET";
  elements.customAuthMode.value = "bearer";
  elements.customHeaders.value = "";
  elements.customBody.value = "";
  elements.statusFilter.value = "all";
  elements.sortOrder.value = "index";
  elements.searchQuery.value = "";
  state.currentPage = 1;
  syncCustomPanel();
  syncProxyPanel();
  renderResults();
  showBanner("已重置界面设置。", "success");
}

function syncCustomPanel() {
  elements.customPanel.classList.toggle("hidden", elements.provider.value !== "custom");
}

function syncProxyPanel() {
  elements.proxyUrlWrap.classList.toggle("hidden", elements.proxyMode.value !== "custom");
}

function savePreferences() {
  localStorage.setItem(storageKey, JSON.stringify({
    provider: elements.provider.value,
    concurrency: elements.concurrency.value,
    timeout: elements.timeout.value,
    format: elements.format.value,
    pageSize: elements.pageSize.value,
    proxyMode: elements.proxyMode.value,
    proxyUrl: elements.proxyUrl.value,
    customUrl: elements.customUrl.value,
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
    elements.concurrency.value = payload.concurrency || elements.concurrency.value;
    elements.timeout.value = payload.timeout || elements.timeout.value;
    elements.format.value = payload.format || elements.format.value;
    elements.pageSize.value = payload.pageSize || elements.pageSize.value;
    elements.proxyMode.value = payload.proxyMode || elements.proxyMode.value;
    elements.proxyUrl.value = payload.proxyUrl || "";
    elements.customUrl.value = payload.customUrl || "";
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
  elements.exportValid.addEventListener("click", () => downloadStatus("valid", "valid_keys.txt"));
  elements.exportInvalid.addEventListener("click", () => downloadStatus("invalid", "invalid_keys.txt"));
  elements.exportError.addEventListener("click", () => downloadStatus("error", "error_keys.txt"));
  elements.refreshHistory.addEventListener("click", refreshHistory);
  elements.clearHistory.addEventListener("click", clearHistory);
  elements.toggleHistory.addEventListener("click", toggleHistory);
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
    elements.concurrency,
    elements.timeout,
    elements.format,
    elements.proxyMode,
    elements.proxyUrl,
    elements.customUrl,
    elements.customMethod,
    elements.customAuthMode,
    elements.customHeaders,
    elements.customBody,
  ].forEach((element) => {
    element.addEventListener("change", () => {
      if (element === elements.provider) {
        syncCustomPanel();
      }
      if (element === elements.proxyMode) {
        syncProxyPanel();
      }
      savePreferences();
    });
    element.addEventListener("input", savePreferences);
  });
}

function validateForm() {
  if (!elements.keys.value.trim()) {
    return "请至少输入一个 key。";
  }
  if (!elements.timeout.value.trim()) {
    return "请填写超时。";
  }
  if (elements.provider.value === "custom" && !elements.customUrl.value.trim()) {
    return "自定义 Provider 必须填写 URL。";
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
  syncCustomPanel();
  syncProxyPanel();
  document.querySelector(".history-panel")?.classList.add("collapsed");
  elements.toggleHistory.textContent = "展开";
  wireFileImport();
  wireActions();
  updateSummary(state.summary);
  renderResults();
  showBanner("", "info");
  await restoreLatestJob();
}

init().catch((error) => {
  console.error(error);
  showBanner(`初始化失败: ${error.message}`, "error");
});
