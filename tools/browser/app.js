const folder = document.querySelector("#folder");
const choose = document.querySelector("#choose");
const cancel = document.querySelector("#cancel");
const status = document.querySelector("#status");
const download = document.querySelector("#download");
const logPanel = document.querySelector("#details");
const log = document.querySelector("#log");
let worker = null;
let downloadUrl = null;

choose.addEventListener("click", () => folder.click());
folder.addEventListener("change", startConversion);
cancel.addEventListener("click", () => {
  if (!worker) return;
  cancel.disabled = true;
  status.textContent = "Cancelling conversion…";
  worker.postMessage({ type: "cancel" });
});

async function startConversion() {
  if (!folder.files?.length) return;
  resetOutput();
  setBusy(true);
  status.textContent = "Loading the homebrew binary from this Pages site…";
  logPanel.hidden = false;
  log.textContent = "";
  try {
    worker = new Worker(new URL("./worker.js", import.meta.url), { type: "module" });
    worker.onmessage = event => {
      const message = event.data;
      if (message.type === "progress") status.textContent = message.message;
      else if (message.type === "log") appendLog(message.message);
      else if (message.type === "done") {
        const blob = new Blob([message.zip], { type: "application/zip" });
        downloadUrl = URL.createObjectURL(blob);
        download.href = downloadUrl;
        download.download = "Jazz-Jackrabbit-SD-card.zip";
        download.hidden = false;
        status.textContent = `Ready: ${message.fileCount} data files, ${formatBytes(message.packSize)} Jazz.jj pack.${message.warnings ? ` ${message.warnings}` : ""}`;
        worker.terminate();
        worker = null;
        setBusy(false);
      } else if (message.type === "cancelled") {
        status.textContent = "Conversion cancelled. Your files were not uploaded.";
        worker.terminate();
        worker = null;
        setBusy(false);
      } else if (message.type === "error") {
        status.textContent = message.message;
        appendLog(message.message);
        worker.terminate();
        worker = null;
        setBusy(false);
      }
    };
    worker.onerror = event => {
      status.textContent = `Converter failed to start: ${event.message || "unknown error"}`;
      appendLog(event.message || "unknown error");
      worker?.terminate();
      worker = null;
      setBusy(false);
    };
    worker.postMessage({ type: "convert", files: [...folder.files] });
    status.textContent = "Preparing your game files locally…";
  } catch (error) {
    status.textContent = error?.message || String(error);
    appendLog(status.textContent);
    setBusy(false);
  }
}

function setBusy(busy) {
  choose.disabled = busy;
  cancel.hidden = !busy;
  cancel.disabled = false;
}

function resetOutput() {
  download.hidden = true;
  if (downloadUrl) URL.revokeObjectURL(downloadUrl);
  downloadUrl = null;
}

function appendLog(line) {
  log.textContent += `${line}\n`;
  log.scrollTop = log.scrollHeight;
}

function formatBytes(bytes) {
  return `${(bytes / (1024 * 1024)).toFixed(1)} MiB`;
}
