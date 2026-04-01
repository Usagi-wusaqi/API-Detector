const state = {
  providers: [],
  jobId: null,
  eventSource: null,
  jobs: [],
  results: [],
  filteredResults: [],
  summary: { total: 0, checked: 0, valid: 0, invalid: 0, error: 0, canceled: 0, keys_per_second: 0 },
};

const elements = {
  provider: document.getElementById("provider"),
  concurrency: document.getElementById("concurrency"),
  timeout: document.getElementById("timeout"),
  format: document.getElementById("format"),
  customUrl: document.getElementById("custom-url"),
  customMethod: document.getElementById("custom-method"),
  customAuthMode: document.getElementById("custom-auth-mode"),
  customHeaders: document.getElementById("custom-headers"),
  customBody: document.getElementById("custom-body"),
  keys: document.getElementById("keys"),
  start: document.getElementById("start"),
  cancel: document.getElementById("cancel"),
  exportValid: document.getElementById("export-valid"),
  exportInvalid: document.getElementById("export-invalid"),
  exportError: document.getElementById("export-error"),
  importFile: document.getElementById("import-file"),
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
  jobHistory: document.getElementById("job-history"),
  refreshHistory: document.getElementById("refresh-history"),
};

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

  for (const result of state.filteredResults) {
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

  elements.exportValid.disabled = state.results.every((item) => item.status !== "valid");
  elements.exportInvalid.disabled = state.results.every((item) => item.status !== "invalid");
  elements.exportError.disabled = state.results.every((item) => item.status !== "error");
}

function renderHistory() {
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

function downloadStatus(status, filename) {
  if (!state.jobId) return;
  const url = `/api/jobs/${state.jobId}/results?status=${encodeURIComponent(status)}`;
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
  });

  source.onerror = () => {
    closeEvents();
    setRunning(false);
  };
}

async function startJob() {
  state.results = [];
  renderResults();
  updateSummary({ total: 0, checked: 0, valid: 0, invalid: 0, error: 0, canceled: 0, keys_per_second: 0 });
  setRunning(true);

  const payload = {
    provider: elements.provider.value,
    keys: elements.keys.value,
    concurrency: Number(elements.concurrency.value),
    timeout: elements.timeout.value,
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
    alert(text || "创建任务失败");
    return;
  }

  const data = await response.json();
  state.jobId = data.id;
  connectEvents(state.jobId);
}

async function cancelJob() {
  if (!state.jobId) return;
  await fetch(`/api/jobs/${state.jobId}/cancel`, { method: "POST" });
}

async function refreshHistory() {
  const response = await fetch("/api/jobs");
  if (!response.ok) return;
  const jobs = await response.json();
  state.jobs = Array.isArray(jobs) ? jobs : [];
  state.jobs.sort((left, right) => new Date(right.started_at).getTime() - new Date(left.started_at).getTime());
  renderHistory();
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

function wireActions() {
  elements.start.addEventListener("click", startJob);
  elements.cancel.addEventListener("click", cancelJob);
  elements.exportValid.addEventListener("click", () => downloadStatus("valid", "valid_keys.txt"));
  elements.exportInvalid.addEventListener("click", () => downloadStatus("invalid", "invalid_keys.txt"));
  elements.exportError.addEventListener("click", () => downloadStatus("error", "error_keys.txt"));
  elements.refreshHistory.addEventListener("click", refreshHistory);
  elements.statusFilter.addEventListener("change", renderResults);
  elements.sortOrder.addEventListener("change", renderResults);
  elements.searchQuery.addEventListener("input", renderResults);
}

async function init() {
  await loadProviders();
  wireFileImport();
  wireActions();
  updateSummary(state.summary);
  renderResults();
  await restoreLatestJob();
}

init().catch((error) => {
  console.error(error);
  alert(`初始化失败: ${error.message}`);
});
