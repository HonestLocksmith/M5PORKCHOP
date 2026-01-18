// WiFi File Server implementation

#include "fileserver.h"
#include <esp_heap_caps.h>
#include <SD.h>
#include <ESPmDNS.h>
#include <pgmspace.h>
#include "../core/wifi_utils.h"

// Static members
WebServer* FileServer::server = nullptr;
FileServerState FileServer::state = FileServerState::IDLE;
char FileServer::statusMessage[64] = "Ready";
char FileServer::targetSSID[64] = "";
char FileServer::targetPassword[64] = "";
uint32_t FileServer::connectStartTime = 0;
uint32_t FileServer::lastReconnectCheck = 0;
uint64_t FileServer::sessionRxBytes = 0;
uint64_t FileServer::sessionTxBytes = 0;
uint32_t FileServer::sessionUploadCount = 0;
uint32_t FileServer::sessionDownloadCount = 0;

// File upload state (needs to be declared early for stop() to access it)
static File uploadFile;
static String uploadDir;
static bool uploadActive = false;
static uint32_t uploadLastProgress = 0;
static bool uploadRejected = false;
static bool listActive = false;
static String uploadPath;
static size_t uploadBytesThisFile = 0;

static void logWiFiStatus(const char* label) {
    // Print WiFi status and mode to aid debugging of server connection issues
    (void)label;
    wifi_mode_t mode = WiFi.getMode();
    wl_status_t status = WiFi.status();
    Serial.printf("[FILESERVER] %s WiFi mode=%d status=%d\n", label, (int)mode, (int)status);
}

static void logRequest(WebServer* srv, const char* label) {
    // Log incoming HTTP request method and URI for debugging
    (void)label;
    if (!srv) return;
    String method;
    switch (srv->method()) {
        case HTTP_GET: method = "GET"; break;
        case HTTP_POST: method = "POST"; break;
        case HTTP_PUT: method = "PUT"; break;
        case HTTP_DELETE: method = "DELETE"; break;
        default: method = String((int)srv->method()); break;
    }
    Serial.printf("[FILESERVER] %s %s\n", method.c_str(), srv->uri().c_str());
}

static void logHeapStatus(const char* label) {
    // Log current free heap and largest free block for debugging memory usage
    size_t freeHeap = ESP.getFreeHeap();
    size_t largest = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
    Serial.printf("[FILESERVER] %s heap free=%u largest=%u\n", label ? label : "heap", (unsigned int)freeHeap, (unsigned int)largest);
}

static void logHeapStatusIfLow(const char* label) {
    // If free heap drops below 60k, log details to help trace memory issues
    size_t freeHeap = ESP.getFreeHeap();
    if (freeHeap < 60000) {
        size_t largest = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
        Serial.printf("[FILESERVER] %s low heap free=%u largest=%u\n", label ? label : "low heap", (unsigned int)freeHeap, (unsigned int)largest);
    }
}

static bool isTransferBusy() {
    return uploadActive;
}

static void sendBusyResponse(WebServer* srv) {
    srv->sendHeader("Connection", "close");
    srv->send(503, "text/plain", "Busy");
}

static void appendJsonEscaped(String& out, const char* in) {
    while (*in) {
        char c = *in++;
        if (c == '\"') {
            out += "\\\"";
        } else if (c == '\\') {
            out += "\\\\";
        } else if (static_cast<uint8_t>(c) < 0x20) {
            out += ' ';
        } else {
            out += c;
        }
    }
}

// Black & white HTML interface - Midnight Commander style dual-pane
static const char HTML_TEMPLATE[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>PORKCHOP COMMANDER</title>
    <style>
        :root { --pink: #C8B2FF; --bg: #2E1A47; --sel: #3A2058; --active: #4B2A6E; }
        * { box-sizing: border-box; margin: 0; padding: 0; }
        body { 
            background: var(--bg); 
            color: var(--pink); 
            font-family: 'Courier New', monospace;
            font-size: 0.9em;
            height: 100vh;
            display: flex;
            flex-direction: column;
            overflow: hidden;
        }
        .header {
            padding: 5px 10px;
            border-bottom: 1px solid var(--pink);
            display: flex;
            justify-content: space-between;
            align-items: center;
            flex-shrink: 0;
        }
        .header h1 { font-size: 1em; font-weight: normal; }
        .sd-info { opacity: 0.6; font-size: 0.85em; }
        .panes {
            display: flex;
            flex: 1;
            overflow: hidden;
        }
        .pane {
            flex: 1;
            display: flex;
            flex-direction: column;
            border-right: 1px solid #3A2058;
            overflow: hidden;
        }
        .pane:last-child { border-right: none; }
        .pane.active .pane-header { background: var(--active); }
        .pane-header {
            padding: 6px 10px;
            background: #231238;
            border-bottom: 1px solid #3A2058;
            display: flex;
            justify-content: space-between;
            font-size: 0.85em;
            flex-shrink: 0;
        }
        .pane-path { 
            flex: 1; 
            overflow: hidden; 
            text-overflow: ellipsis; 
            white-space: nowrap;
        }
        .pane-select-info { opacity: 0.7; margin-left: 10px; }
        .file-list {
            flex: 1;
            overflow-y: auto;
            overflow-x: hidden;
        }
        .file-item { 
            display: flex; 
            align-items: center;
            padding: 4px 8px;
            cursor: pointer;
            border-bottom: 1px solid #231238;
        }
        .file-item:hover { background: #2B1741; }
        .file-item.focused { background: var(--sel); outline: 1px solid var(--pink); }
        .file-item.selected { background: var(--active); }
        .file-item.selected.focused { background: #5A3280; }
        .file-check { 
            display: none;
        }
        .file-icon { 
            width: 20px; 
            text-align: center; 
            opacity: 0.5;
            flex-shrink: 0;
        }
        .file-icon.dir { opacity: 1; color: var(--pink); }
        .file-name { 
            flex: 1; 
            overflow: hidden; 
            text-overflow: ellipsis; 
            white-space: nowrap;
            padding: 0 8px;
        }
        .file-size { 
            opacity: 0.4; 
            min-width: 60px; 
            text-align: right; 
            font-size: 0.85em;
            flex-shrink: 0;
        }
        .toolbar {
            display: flex;
            gap: 5px;
            padding: 8px;
            background: #1D102F;
            border-top: 1px solid #3A2058;
            flex-shrink: 0;
            flex-wrap: wrap;
        }
        .btn {
            background: var(--pink);
            color: var(--bg);
            border: none;
            padding: 5px 12px;
            cursor: pointer;
            font-family: inherit;
            font-size: 0.8em;
        }
        .btn:hover { opacity: 0.8; }
        .btn:disabled { opacity: 0.3; cursor: not-allowed; }
        .btn-outline { 
            background: transparent; 
            color: var(--pink); 
            border: 1px solid var(--pink); 
            opacity: 0.7; 
        }
        .btn-outline:hover { opacity: 1; background: #2E1A47; }
        .btn-danger { background: #4B2A6E; color: var(--pink); }
        .fkey-bar {
            display: flex;
            background: #241238;
            border-top: 1px solid #3A2058;
            flex-shrink: 0;
        }
        .fkey {
            flex: 1;
            padding: 6px 4px;
            text-align: center;
            font-size: 0.75em;
            border-right: 1px solid #2C1744;
            cursor: pointer;
        }
        .fkey:hover { background: #2E1A47; }
        .fkey:last-child { border-right: none; }
        .fkey span { opacity: 0.5; }
        .status {
            padding: 4px 10px;
            font-size: 0.8em;
            background: #1D102F;
            border-top: 1px solid #2E1A47;
            min-height: 22px;
            flex-shrink: 0;
        }
        .modal {
            display: none;
            position: fixed;
            top: 0; left: 0;
            width: 100%; height: 100%;
            background: rgba(0,0,0,0.9);
            justify-content: center;
            align-items: center;
            z-index: 100;
        }
        .modal-content {
            background: var(--bg);
            border: 1px solid var(--pink);
            padding: 20px;
            max-width: 400px;
            width: 90%;
        }
        .modal-content h3 { margin-bottom: 15px; font-weight: normal; }
        .modal-actions { display: flex; gap: 10px; margin-top: 15px; }
        input[type="text"] {
            background: #231238;
            color: var(--pink);
            border: 1px solid #3A2058;
            padding: 8px;
            font-family: inherit;
            width: 100%;
        }
        input[type="text"]:focus { outline: none; border-color: var(--pink); }
        .upload-btn { position: relative; overflow: hidden; }
        .upload-btn input[type="file"] {
            position: absolute;
            left: 0; top: 0;
            width: 100%; height: 100%;
            opacity: 0;
            cursor: pointer;
        }
        .progress-bar {
            height: 4px;
            background: #2E1A47;
            margin-top: 5px;
            display: none;
        }
        .progress-bar.active { display: block; }
        .progress-fill {
            height: 100%;
            background: var(--pink);
            width: 0%;
            transition: width 0.1s;
        }
        @media (max-width: 600px) {
            .panes { flex-direction: column; }
            .pane { border-right: none; border-bottom: 1px solid #3A2058; }
            .file-size { display: none; }
        }
    </style>
</head>
<body>
    <div class="header">
        <h1>PORKCHOP COMMANDOR 64</h1>
        <div class="sd-info" id="sdInfo">...</div>
    </div>
    
    <div class="panes">
        <div class="pane active" id="paneL" onclick="setActivePane('L')">
            <div class="pane-header">
                <div class="pane-path" id="pathL">/</div>
                <div class="pane-select-info" id="selInfoL"></div>
            </div>
            <div class="file-list" id="listL"></div>
        </div>
        <div class="pane" id="paneR" onclick="setActivePane('R')">
            <div class="pane-header">
                <div class="pane-path" id="pathR">/</div>
                <div class="pane-select-info" id="selInfoR"></div>
            </div>
            <div class="file-list" id="listR"></div>
        </div>
    </div>
    
    <div class="toolbar">
        <button class="btn btn-outline" onclick="refresh()">Refresh</button>
        <button class="btn btn-outline" onclick="showNewFolderModal()">+Folder</button>
        <label class="btn upload-btn">Upload<input type="file" multiple onchange="uploadFiles(this.files)"></label>
        <button class="btn" onclick="selectAll()">Sel All</button>
        <button class="btn btn-outline" onclick="selectNone()">Sel None</button>
        <button class="btn" onclick="showRenameModal()" id="btnRename">Rename</button>
        <button class="btn" onclick="copySelected()" id="btnCopy">Copy→</button>
        <button class="btn" onclick="moveSelected()" id="btnMove">Move→</button>
        <button class="btn" onclick="downloadSelected()" id="btnDownload">Download</button>
        <button class="btn btn-danger" onclick="deleteSelected()" id="btnDelete">Delete</button>
    </div>
    
    <div class="progress-bar" id="progressBar"><div class="progress-fill" id="progressFill"></div></div>
    
    <div class="fkey-bar">
        <div class="fkey" onclick="showHelp()"><span>F1</span> Help</div>
        <div class="fkey" onclick="showRenameModal()"><span>F2</span> Ren</div>
        <div class="fkey" onclick="copySelected()"><span>F5</span> Copy</div>
        <div class="fkey" onclick="moveSelected()"><span>F6</span> Move</div>
        <div class="fkey" onclick="showNewFolderModal()"><span>F7</span> MkDir</div>
        <div class="fkey" onclick="deleteSelected()"><span>F8</span> Del</div>
    </div>
    
    <div class="status" id="status">awaiting orders | ↑↓ nav | space sel | enter exec | tab flip</div>
    
    <!-- New Folder Modal -->
    <div class="modal" id="newFolderModal" onclick="if(event.target===this)hideModal()">
        <div class="modal-content">
            <h3>New Folder</h3>
            <input type="text" id="newFolderName" placeholder="Folder name" 
                   onkeydown="if(event.key==='Enter')createFolder();if(event.key==='Escape')hideModal()">
            <div class="modal-actions">
                <button class="btn" onclick="createFolder()">Create</button>
                <button class="btn btn-outline" onclick="hideModal()">Cancel</button>
            </div>
        </div>
    </div>
    
    <!-- Help Modal -->
    <div class="modal" id="helpModal" onclick="if(event.target===this)hideModal()">
        <div class="modal-content">
            <h3>Keyboard Shortcuts</h3>
            <pre style="font-size:0.85em;line-height:1.6;opacity:0.8">
Arrow Up/Down  Navigate files
Enter          Open folder / Download
Space          Toggle selection
Tab            Switch pane
Ctrl+A         Select all
F2             Rename focused item
F5             Copy sel → other pane
F6             Move sel → other pane
F7             New folder
F8/Delete      Delete selected
Backspace      Parent folder
            </pre>
            <div class="modal-actions">
                <button class="btn" onclick="hideModal()">Close</button>
            </div>
        </div>
    </div>

    <!-- Rename Modal -->
    <div class="modal" id="renameModal" onclick="if(event.target===this)hideModal()">
        <div class="modal-content">
            <h3>Rename</h3>
            <input type="text" id="renameNewName" placeholder="New name"
                   onkeydown="if(event.key==='Enter')doRename();if(event.key==='Escape')hideModal()">
            <input type="hidden" id="renameOldPath">
            <div class="modal-actions">
                <button class="btn" onclick="doRename()">Rename</button>
                <button class="btn btn-outline" onclick="hideModal()">Cancel</button>
            </div>
        </div>
    </div>

<script>
// Pane state
const panes = {
    L: { path: '/', items: [], selected: new Set(), focusIdx: 0, loading: false },
    R: { path: '/', items: [], selected: new Set(), focusIdx: 0, loading: false }
};
let activePane = 'L';
let sdInfoLoading = false;
let refreshInProgress = false;
let refreshPending = false;
let lastRefreshAt = 0;
let fetchQueue = Promise.resolve();
const LIST_LIMIT = 200;

function queuedFetch(url, options) {
    const run = () => fetch(url, options);
    const p = fetchQueue.then(run, run);
    fetchQueue = p.catch(() => {});
    return p;
}

// Initialize
document.addEventListener('DOMContentLoaded', () => {
    document.addEventListener('keydown', handleKeydown);
    bootstrap();
});

async function bootstrap() {
    await loadPane('L', '/');
    await loadPane('R', '/');
    await loadSDInfo();
}

function setActivePane(id) {
    activePane = id;
    document.getElementById('paneL').classList.toggle('active', id === 'L');
    document.getElementById('paneR').classList.toggle('active', id === 'R');
}

async function loadSDInfo() {
    if (sdInfoLoading) return;
    sdInfoLoading = true;
    try {
        const r = await queuedFetch('/api/sdinfo');
        const d = await r.json();
        const pct = ((d.used / d.total) * 100).toFixed(0);
        document.getElementById('sdInfo').textContent = 
            formatSize(d.used * 1024) + ' / ' + formatSize(d.total * 1024) + ' (' + pct + '%)';
    } catch(e) {
        document.getElementById('sdInfo').textContent = 'no sd. no loot.';
    } finally {
        sdInfoLoading = false;
    }
}

async function loadPane(id, path) {
    const pane = panes[id];
    if (pane.loading) return;
    pane.loading = true;
    pane.path = path;
    pane.selected.clear();
    pane.focusIdx = 0;
    
    document.getElementById('path' + id).textContent = path || '/';
    const list = document.getElementById('list' + id);
    list.innerHTML = '<div style="padding:20px;opacity:0.5">jacking in...</div>';
    
    try {
        const r = await queuedFetch('/api/ls?dir=' + encodeURIComponent(path) + '&full=1&limit=' + LIST_LIMIT);
        const items = await r.json();
        
        // Sort: directories first, then alphabetically
        pane.items = [];
        
        // Parent directory entry
        if (path !== '/') {
            pane.items.push({ name: '..', isDir: true, isParent: true, size: 0 });
        }
        
        // Directories
        items.filter(i => i.isDir).sort((a,b) => a.name.localeCompare(b.name))
            .forEach(i => pane.items.push(i));
        
        // Files
        items.filter(i => !i.isDir).sort((a,b) => a.name.localeCompare(b.name))
            .forEach(i => pane.items.push(i));
        
        renderPane(id);
    } catch(e) {
        list.innerHTML = '<div style="padding:20px;opacity:0.5">load failed</div>';
    } finally {
        pane.loading = false;
        updateSelectionInfo(id);
    }
}

function renderPane(id) {
    const pane = panes[id];
    const list = document.getElementById('list' + id);
    
    if (pane.items.length === 0) {
        list.innerHTML = '<div style="padding:20px;opacity:0.4;text-align:center">void</div>';
        return;
    }
    
    let html = '';
    pane.items.forEach((item, idx) => {
        const isSel = pane.selected.has(idx);
        const isFocus = (idx === pane.focusIdx && activePane === id);
        const cls = 'file-item' + (isSel ? ' selected' : '') + (isFocus ? ' focused' : '');
        const icon = item.isDir ? '/' : '*';
        const iconCls = item.isDir ? 'file-icon dir' : 'file-icon';
        const check = isSel ? '[x]' : '[ ]';
        const size = item.isDir ? '' : formatSize(item.size);
        
        html += '<div class="' + cls + '" data-idx="' + idx + '" data-pane="' + id + '"';
        html += ' onclick="onItemClick(event,' + idx + ',\'' + id + '\')"';
        html += ' ondblclick="onItemDblClick(' + idx + ',\'' + id + '\')">';
        html += '<div class="file-check">' + check + '</div>';
        html += '<div class="' + iconCls + '">' + icon + '</div>';
        html += '<div class="file-name">' + escapeHtml(item.name) + '</div>';
        html += '<div class="file-size">' + size + '</div>';
        html += '</div>';
    });
    list.innerHTML = html;
    
    // Scroll focused item into view
    const focused = list.querySelector('.focused');
    if (focused) focused.scrollIntoView({ block: 'nearest' });
}

function onItemClick(event, idx, paneId) {
    setActivePane(paneId);
    panes[paneId].focusIdx = idx;
    
    if (event.ctrlKey || event.metaKey) {
        toggleSelect(paneId, idx);
    } else if (event.shiftKey) {
        // Range select not implemented for simplicity
        toggleSelect(paneId, idx);
    } else {
        renderPane(paneId);
    }
}

function onItemDblClick(idx, paneId) {
    const pane = panes[paneId];
    const item = pane.items[idx];
    
    if (item.isParent) {
        const parent = pane.path.substring(0, pane.path.lastIndexOf('/')) || '/';
        loadPane(paneId, parent);
    } else if (item.isDir) {
        const newPath = (pane.path === '/' ? '' : pane.path) + '/' + item.name;
        loadPane(paneId, newPath);
    } else {
        downloadFile(paneId, idx);
    }
}

function toggleSelect(paneId, idx) {
    const pane = panes[paneId];
    const item = pane.items[idx];
    if (item.isParent) return; // Can't select parent dir
    
    if (pane.selected.has(idx)) {
        pane.selected.delete(idx);
    } else {
        pane.selected.add(idx);
    }
    renderPane(paneId);
    updateSelectionInfo(paneId);
}

function updateSelectionInfo(id) {
    const pane = panes[id];
    const count = pane.selected.size;
    const el = document.getElementById('selInfo' + id);
    el.textContent = count > 0 ? '[' + count + ' sel]' : '';
    
    // Update toolbar buttons
    const totalSel = panes.L.selected.size + panes.R.selected.size;
    document.getElementById('btnDelete').textContent = totalSel > 0 ? 'Delete (' + totalSel + ')' : 'Delete';
    document.getElementById('btnDownload').textContent = totalSel > 0 ? 'Download (' + totalSel + ')' : 'Download';
}

function selectAll() {
    const pane = panes[activePane];
    pane.items.forEach((item, idx) => {
        if (!item.isParent) pane.selected.add(idx);
    });
    renderPane(activePane);
    updateSelectionInfo(activePane);
}

function selectNone() {
    const pane = panes[activePane];
    pane.selected.clear();
    renderPane(activePane);
    updateSelectionInfo(activePane);
}

function handleKeydown(e) {
    // Don't handle if in modal input
    if (document.activeElement.tagName === 'INPUT') return;
    
    const pane = panes[activePane];
    
    switch(e.key) {
        case 'ArrowUp':
            e.preventDefault();
            if (pane.focusIdx > 0) {
                pane.focusIdx--;
                renderPane(activePane);
            }
            break;
        case 'ArrowDown':
            e.preventDefault();
            if (pane.focusIdx < pane.items.length - 1) {
                pane.focusIdx++;
                renderPane(activePane);
            }
            break;
        case 'Enter':
            e.preventDefault();
            onItemDblClick(pane.focusIdx, activePane);
            break;
        case ' ':
            e.preventDefault();
            toggleSelect(activePane, pane.focusIdx);
            break;
        case 'Tab':
            e.preventDefault();
            setActivePane(activePane === 'L' ? 'R' : 'L');
            renderPane('L');
            renderPane('R');
            break;
        case 'Backspace':
            e.preventDefault();
            if (pane.path !== '/') {
                const parent = pane.path.substring(0, pane.path.lastIndexOf('/')) || '/';
                loadPane(activePane, parent);
            }
            break;
        case 'Delete':
            e.preventDefault();
            deleteSelected();
            break;
        case 'a':
            if (e.ctrlKey || e.metaKey) {
                e.preventDefault();
                selectAll();
            }
            break;
        case 'F1':
            e.preventDefault();
            showHelp();
            break;
        case 'F2':
            e.preventDefault();
            showRenameModal();
            break;
        case 'F5':
            e.preventDefault();
            copySelected();
            break;
        case 'F6':
            e.preventDefault();
            moveSelected();
            break;
        case 'F7':
            e.preventDefault();
            showNewFolderModal();
            break;
        case 'F8':
            e.preventDefault();
            deleteSelected();
            break;
        case 'F9':
            e.preventDefault();
            downloadSelected();
            break;
    }
}

function getSelectedPaths() {
    const paths = [];
    ['L', 'R'].forEach(id => {
        const pane = panes[id];
        pane.selected.forEach(idx => {
            const item = pane.items[idx];
            if (!item.isParent) {
                const path = (pane.path === '/' ? '' : pane.path) + '/' + item.name;
                paths.push({ path, isDir: item.isDir });
            }
        });
    });
    return paths;
}

async function deleteSelected() {
    const items = getSelectedPaths();
    if (items.length === 0) {
        setStatus('select targets first');
        return;
    }
    
    const msg = 'nuke ' + items.length + ' item(s)? no undo. no regrets.';
    if (!confirm(msg)) return;
    
    setStatus('nuking ' + items.length + ' targets...');
    
    try {
        const resp = await queuedFetch('/api/bulkdelete', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ paths: items.map(i => i.path) })
        });
        const result = await resp.json();
        setStatus('nuked ' + result.deleted + '/' + items.length);
        refresh();
    } catch(e) {
        setStatus('nuke failed: ' + e.message);
    }
}

async function downloadSelected() {
    const items = getSelectedPaths().filter(i => !i.isDir);
    if (items.length === 0) {
        setStatus('no files marked. dirs need zip. we aint got zip.');
        return;
    }
    
    setStatus('exfiltrating ' + items.length + ' file(s)...');
    
    // Download files sequentially (browser limitation)
    for (let i = 0; i < items.length; i++) {
        await new Promise(resolve => {
            const a = document.createElement('a');
            a.href = '/download?f=' + encodeURIComponent(items[i].path);
            a.download = items[i].path.split('/').pop();
            a.click();
            setTimeout(resolve, 300); // Small delay between downloads
        });
    }
    
    setStatus('exfil complete: ' + items.length);
}

function downloadFile(paneId, idx) {
    const pane = panes[paneId];
    const item = pane.items[idx];
    if (item.isDir) return;
    
    const path = (pane.path === '/' ? '' : pane.path) + '/' + item.name;
    window.location.href = '/download?f=' + encodeURIComponent(path);
}

async function refresh() {
    const now = Date.now();
    if (refreshInProgress) {
        refreshPending = true;
        return;
    }
    if (now - lastRefreshAt < 500) {
        return;
    }
    refreshInProgress = true;
    lastRefreshAt = now;
    await loadPane('L', panes.L.path);
    await loadPane('R', panes.R.path);
    await loadSDInfo();
    refreshInProgress = false;
    if (refreshPending) {
        refreshPending = false;
        refresh();
    }
}

function showNewFolderModal() {
    document.getElementById('newFolderModal').style.display = 'flex';
    document.getElementById('newFolderName').value = '';
    setTimeout(() => document.getElementById('newFolderName').focus(), 50);
}

function showHelp() {
    document.getElementById('helpModal').style.display = 'flex';
}

function hideModal() {
    document.getElementById('newFolderModal').style.display = 'none';
    document.getElementById('helpModal').style.display = 'none';
    document.getElementById('renameModal').style.display = 'none';
}

function showRenameModal() {
    const pane = panes[activePane];
    const item = pane.items[pane.focusIdx];
    if (!item || item.isParent) { setStatus('select item to rename'); return; }
    const path = (pane.path === '/' ? '' : pane.path) + '/' + item.name;
    document.getElementById('renameOldPath').value = path;
    document.getElementById('renameNewName').value = item.name;
    document.getElementById('renameModal').style.display = 'flex';
    setTimeout(() => document.getElementById('renameNewName').select(), 50);
}

async function doRename() {
    const oldPath = document.getElementById('renameOldPath').value;
    const newName = document.getElementById('renameNewName').value.trim();
    if (!newName) { alert('provide new name'); return; }
    if (newName.includes('/') || newName.includes('..')) { alert('illegal characters'); return; }
    
    const pane = panes[activePane];
    const newPath = (pane.path === '/' ? '' : pane.path) + '/' + newName;
    
    try {
        const resp = await queuedFetch('/api/rename?old=' + encodeURIComponent(oldPath) + '&new=' + encodeURIComponent(newPath));
        const result = await resp.json();
        if (result.success) {
            setStatus('renamed: ' + newName);
            hideModal();
            loadPane(activePane, pane.path);
        } else {
            setStatus('rename failed: ' + (result.error || 'unknown'));
        }
    } catch(e) {
        setStatus('fault: ' + e.message);
    }
}

async function copySelected() {
    const src = panes[activePane];
    const dst = panes[activePane === 'L' ? 'R' : 'L'];
    
    // Prevent copying to same directory
    if (src.path === dst.path) {
        setStatus('source and dest are same directory');
        return;
    }
    
    // Get selected or focused items
    let items = [];
    src.selected.forEach(idx => {
        const item = src.items[idx];
        if (item && !item.isParent) items.push(item);
    });
    if (!items.length && src.focusIdx >= 0) {
        const item = src.items[src.focusIdx];
        if (item && !item.isParent) items = [item];
    }
    if (!items.length) { setStatus('select files to copy'); return; }
    
    const paths = items.map(i => (src.path === '/' ? '' : src.path) + '/' + i.name);
    setStatus('copying ' + items.length + ' item(s)...');
    
    try {
        const resp = await queuedFetch('/api/copy', {
            method: 'POST',
            headers: {'Content-Type': 'application/json'},
            body: JSON.stringify({files: paths, dest: dst.path})
        });
        const result = await resp.json();
        if (result.success) {
            setStatus('copied: ' + result.copied + ' item(s)');
            loadPane(activePane === 'L' ? 'R' : 'L', dst.path);
        } else {
            setStatus('copy failed: ' + (result.error || 'unknown'));
        }
    } catch(e) {
        setStatus('fault: ' + e.message);
    }
}

async function moveSelected() {
    const src = panes[activePane];
    const dst = panes[activePane === 'L' ? 'R' : 'L'];
    
    // Prevent moving to same directory
    if (src.path === dst.path) {
        setStatus('source and dest are same directory');
        return;
    }
    
    // Get selected or focused items
    let items = [];
    src.selected.forEach(idx => {
        const item = src.items[idx];
        if (item && !item.isParent) items.push(item);
    });
    if (!items.length && src.focusIdx >= 0) {
        const item = src.items[src.focusIdx];
        if (item && !item.isParent) items = [item];
    }
    if (!items.length) { setStatus('select files to move'); return; }
    
    const paths = items.map(i => (src.path === '/' ? '' : src.path) + '/' + i.name);
    setStatus('moving ' + items.length + ' item(s)...');
    
    try {
        const resp = await queuedFetch('/api/move', {
            method: 'POST',
            headers: {'Content-Type': 'application/json'},
            body: JSON.stringify({files: paths, dest: dst.path})
        });
        const result = await resp.json();
        if (result.success) {
            setStatus('moved: ' + result.moved + ' item(s)');
            loadPane('L', panes.L.path);
            loadPane('R', panes.R.path);
        } else {
            setStatus('move failed: ' + (result.error || 'unknown'));
        }
    } catch(e) {
        setStatus('fault: ' + e.message);
    }
}

async function createFolder() {
    const name = document.getElementById('newFolderName').value.trim();
    if (!name) { alert('name the directory'); return; }
    if (name.includes('/') || name.includes('..')) { alert('illegal characters'); return; }
    
    const pane = panes[activePane];
    const path = (pane.path === '/' ? '' : pane.path) + '/' + name;
    
    try {
        const resp = await queuedFetch('/mkdir?f=' + encodeURIComponent(path));
        if (resp.ok) {
            setStatus('spawned: ' + name);
            hideModal();
            loadPane(activePane, pane.path);
        } else {
            setStatus('spawn failed');
        }
    } catch(e) {
        setStatus('fault: ' + e.message);
    }
}

async function uploadFiles(files) {
    if (!files || !files.length) return;
    
    const pane = panes[activePane];
    const bar = document.getElementById('progressBar');
    const fill = document.getElementById('progressFill');
    bar.classList.add('active');
    
    let uploaded = 0;
    for (let i = 0; i < files.length; i++) {
        setStatus('injecting ' + (i+1) + '/' + files.length + ': ' + files[i].name);
        fill.style.width = '0%';
        
        const formData = new FormData();
        formData.append('file', files[i]);
        
        try {
            await new Promise((resolve, reject) => {
                const xhr = new XMLHttpRequest();
                xhr.upload.onprogress = (e) => {
                    if (e.lengthComputable) fill.style.width = (e.loaded/e.total*100) + '%';
                };
                xhr.onload = () => xhr.status === 200 ? resolve() : reject();
                xhr.onerror = () => reject();
                xhr.open('POST', '/upload?dir=' + encodeURIComponent(pane.path));
                xhr.send(formData);
            });
            uploaded++;
        } catch(e) {
            setStatus('inject failed: ' + files[i].name);
        }
    }
    
    bar.classList.remove('active');
    setStatus('injected ' + uploaded + '/' + files.length + ' payloads');
    loadPane(activePane, pane.path);
}

function setStatus(msg) {
    document.getElementById('status').textContent = msg;
}

function formatSize(bytes) {
    if (bytes < 1024) return bytes + 'B';
    if (bytes < 1024*1024) return (bytes/1024).toFixed(1) + 'K';
    if (bytes < 1024*1024*1024) return (bytes/1024/1024).toFixed(1) + 'M';
    return (bytes/1024/1024/1024).toFixed(2) + 'G';
}

function escapeHtml(s) {
    return s.replace(/&/g,'&amp;').replace(/</g,'&lt;').replace(/>/g,'&gt;').replace(/"/g,'&quot;');
}
</script>
</body>
</html>
)rawliteral";

void FileServer::init() {
    state = FileServerState::IDLE;
    strcpy(statusMessage, "Ready");
    targetSSID[0] = '\0';
    targetPassword[0] = '\0';
    sessionRxBytes = 0;
    sessionTxBytes = 0;
    sessionUploadCount = 0;
    sessionDownloadCount = 0;
}

bool FileServer::start(const char* ssid, const char* password) {
    if (state != FileServerState::IDLE) {
        return true;
    }
    
    // Store credentials for reconnection
    strncpy(targetSSID, ssid ? ssid : "", sizeof(targetSSID) - 1);
    targetSSID[sizeof(targetSSID) - 1] = '\0';
    strncpy(targetPassword, password ? password : "", sizeof(targetPassword) - 1);
    targetPassword[sizeof(targetPassword) - 1] = '\0';
    
    // Check credentials
    if (strlen(targetSSID) == 0) {
        strcpy(statusMessage, "No WiFi SSID set");
        return false;
    }
    
    strcpy(statusMessage, "jacking in.");
    logWiFiStatus("before connect");

    sessionRxBytes = 0;
    sessionTxBytes = 0;
    sessionUploadCount = 0;
    sessionDownloadCount = 0;
    
    // Start non-blocking connection (force restart to recover from desync)
    WiFiUtils::hardReset();
    WiFi.begin(targetSSID, targetPassword);
    
    state = FileServerState::CONNECTING;
    connectStartTime = millis();
    
    return true;
}

void FileServer::startServer() {
    snprintf(statusMessage, sizeof(statusMessage), "%s", WiFi.localIP().toString().c_str());
    logWiFiStatus("startServer");
    
    // Start mDNS
    MDNS.begin("porkchop");
    
    // Create and configure web server
    server = new WebServer(80);
    server->on("/", HTTP_GET, handleRoot);
    server->on("/api/ls", HTTP_GET, handleFileList);
    server->on("/api/sdinfo", HTTP_GET, handleSDInfo);
    server->on("/api/bulkdelete", HTTP_POST, handleBulkDelete);
    server->on("/api/rename", HTTP_GET, handleRename);
    server->on("/api/copy", HTTP_POST, handleCopy);
    server->on("/api/move", HTTP_POST, handleMove);
    server->on("/download", HTTP_GET, handleDownload);
    server->on("/upload", HTTP_POST, handleUpload, handleUploadProcess);
    server->on("/delete", HTTP_GET, handleDelete);
    server->on("/rmdir", HTTP_GET, handleDelete);  // Same handler, will detect folder
    server->on("/mkdir", HTTP_GET, handleMkdir);
    server->onNotFound(handleNotFound);
    
    server->begin();
    state = FileServerState::RUNNING;
    lastReconnectCheck = millis();
}

void FileServer::stop() {
    if (state == FileServerState::IDLE) {
        return;
    }
    // Close any pending upload file
    if (uploadFile) {
        uploadFile.close();
    }
    uploadActive = false;
    uploadRejected = false;
    
    if (server) {
        server->stop();
        delete server;
        server = nullptr;
    }
    
    MDNS.end();
    WiFiUtils::shutdown();
    
    state = FileServerState::IDLE;
    strcpy(statusMessage, "Stopped");
    sessionRxBytes = 0;
    sessionTxBytes = 0;
    sessionUploadCount = 0;
    sessionDownloadCount = 0;
}

void FileServer::update() {
    switch (state) {
        case FileServerState::CONNECTING:
        case FileServerState::RECONNECTING:
            updateConnecting();
            break;
        case FileServerState::RUNNING:
            updateRunning();
            break;
        default:
            break;
    }
}

void FileServer::updateConnecting() {
    uint32_t elapsed = millis() - connectStartTime;
    
    if (WiFi.status() == WL_CONNECTED) {
        startServer();
        return;
    }
    
    // Update status with dots animation
    int dots = (elapsed / 500) % 4;
    snprintf(statusMessage, sizeof(statusMessage), "jacking in%.*s", dots, "...");
    
    // Timeout after 15 seconds
    if (elapsed > 15000) {
        strcpy(statusMessage, "Connection failed");
        logWiFiStatus("connect timeout");
        WiFiUtils::shutdown();
        state = FileServerState::IDLE;
    }
}

void FileServer::updateRunning() {
    if (server) {
        server->handleClient();
    }

    if (uploadActive && (millis() - uploadLastProgress > 10000)) {
        if (uploadFile) {
            uploadFile.close();
        }
        uploadActive = false;
        uploadRejected = false;
    }
    
    // Check WiFi connection every 5 seconds
    uint32_t now = millis();
    if (now - lastReconnectCheck > 5000) {
        lastReconnectCheck = now;
        
        if (WiFi.status() != WL_CONNECTED) {
            strcpy(statusMessage, "retry hack.");
            logWiFiStatus("wifi lost");
            
            // Stop server but keep credentials
            if (server) {
                server->stop();
                delete server;
                server = nullptr;
            }
            
            // Stop mDNS before reconnect
            MDNS.end();
            
            // Restart connection
            WiFiUtils::hardReset();
            WiFi.begin(targetSSID, targetPassword);
            state = FileServerState::RECONNECTING;
            connectStartTime = millis();
        }
    }
}

uint64_t FileServer::getSDFreeSpace() {
    return SD.totalBytes() - SD.usedBytes();
}

uint64_t FileServer::getSDTotalSpace() {
    return SD.totalBytes();
}

void FileServer::handleRoot() {
    logRequest(server, "REQ");
    if (isTransferBusy()) {
        sendBusyResponse(server);
        return;
    }
    logHeapStatus("before /");
    const size_t totalLen = strlen_P(HTML_TEMPLATE);
    server->sendHeader("Connection", "close");
    server->sendHeader("Cache-Control", "no-store");
    server->setContentLength(totalLen);
    server->send(200, "text/html; charset=utf-8", "");

    WiFiClient client = server->client();
    client.setNoDelay(true);

    const size_t chunkSize = 512;
    size_t offset = 0;
    uint32_t lastProgress = millis();

    while (offset < totalLen && client.connected()) {
        size_t len = totalLen - offset;
        if (len > chunkSize) {
            len = chunkSize;
        }
        size_t sent = client.write_P(HTML_TEMPLATE + offset, len);
        if (sent == 0) {
            if (millis() - lastProgress > 2000) {
                break;
            }
            delay(1);
            yield();
            continue;
        }
        offset += sent;
        lastProgress = millis();
        yield();
    }
    client.flush();
    sessionTxBytes += offset;
    logHeapStatus("after /");
    }

void FileServer::handleSDInfo() {
    logRequest(server, "REQ");
    if (isTransferBusy()) {
        sendBusyResponse(server);
        return;
    }
    logHeapStatusIfLow("before /api/sdinfo");
    String json = "{\"total\":";
    json += String((unsigned long)(SD.totalBytes() / 1024));  // KB
    json += ",\"used\":";
    json += String((unsigned long)(SD.usedBytes() / 1024));
    json += ",\"free\":";
    json += String((unsigned long)((SD.totalBytes() - SD.usedBytes()) / 1024));
    json += "}";
    server->sendHeader("Connection", "close");
    server->send(200, "application/json", json);
    sessionTxBytes += json.length();
    logHeapStatusIfLow("after /api/sdinfo");
}

void FileServer::handleFileList() {
    String dir = server->arg("dir");
    bool full = server->arg("full") == "1";
    uint16_t limit = server->arg("limit").toInt();
    logRequest(server, "REQ");
    if (listActive || isTransferBusy()) {
        sendBusyResponse(server);
        return;
    }
    listActive = true;
    if (limit == 0 || limit > 1000) {
        limit = 200;
    }
    if (dir.isEmpty()) dir = "/";
    logHeapStatusIfLow("before /api/ls");
    
    // Security: prevent directory traversal
    if (dir.indexOf("..") >= 0) {
        server->sendHeader("Connection", "close");
        server->send(400, "application/json", "[]");
        listActive = false;
        return;
    }
    
    File root = SD.open(dir);
    if (!root || !root.isDirectory()) {
        server->sendHeader("Connection", "close");
        server->send(200, "application/json", "[]");
        listActive = false;
        return;
    }

    WiFiClient client = server->client();
    client.setNoDelay(true);

    server->sendHeader("Connection", "close");
    server->setContentLength(CONTENT_LENGTH_UNKNOWN);
    server->send(200, "application/json", "[");

    bool first = true;
    uint16_t sentCount = 0;
    String buffer;
    buffer.reserve(1024);

    File file = root.openNextFile();
    while (file && sentCount < limit) {
        yield();

        if (!first) {
            buffer += ",";
        }
        buffer += "{\"name\":\"";
        appendJsonEscaped(buffer, file.name());
        buffer += "\",\"size\":";
        buffer += String((unsigned)file.size());
        if (full) {
            buffer += ",\"isDir\":";
            buffer += file.isDirectory() ? "true" : "false";
        }
        buffer += "}";

        if (buffer.length() >= 1024) {
            server->sendContent(buffer);
            sessionTxBytes += buffer.length();
            buffer = "";
            if (!client.connected()) {
                file.close();
                root.close();
                listActive = false;
                return;
            }
        }

        first = false;
        sentCount++;
        file.close();
        file = root.openNextFile();
    }

    if (file) {
        file.close();
    }
    root.close();

    buffer += "]";
    server->sendContent(buffer);
    sessionTxBytes += buffer.length();
    server->sendContent("");
    client.flush();
    client.stop();
    logHeapStatusIfLow("after /api/ls");
    listActive = false;
}

void FileServer::handleDownload() {
    String path = server->arg("f");
    String dir = server->arg("dir");  // For ZIP download
    logRequest(server, "REQ");
    logHeapStatusIfLow("before /download");
    
    // ZIP download of folder
    if (!dir.isEmpty()) {
        // Simple implementation: send files one by one is not possible
        // Instead, we'll create a simple text manifest for now
        // Full ZIP requires external library
        server->send(501, "text/plain", "ZIP download not yet implemented - download files individually");
        return;
    }
    
    if (uploadActive) {
        server->sendHeader("Connection", "close");
        server->send(409, "text/plain", "Upload in progress");
        return;
    }

    if (path.isEmpty()) {
        server->sendHeader("Connection", "close");
        server->send(400, "text/plain", "Missing file path");
        return;
    }
    
    // Security: prevent directory traversal
    if (path.indexOf("..") >= 0) {
        server->sendHeader("Connection", "close");
        server->send(400, "text/plain", "Invalid path");
        return;
    }
    
    File file = SD.open(path);
    if (!file || file.isDirectory()) {
        server->sendHeader("Connection", "close");
        server->send(404, "text/plain", "File not found");
        return;
    }
    
    // Get filename for Content-Disposition
    String filename = path;
    int lastSlash = path.lastIndexOf('/');
    if (lastSlash >= 0) {
        filename = path.substring(lastSlash + 1);
    }
    
    // Determine content type
    String contentType = "application/octet-stream";
    if (path.endsWith(".txt")) contentType = "text/plain";
    else if (path.endsWith(".csv")) contentType = "text/csv";
    else if (path.endsWith(".json")) contentType = "application/json";
    else if (path.endsWith(".pcap")) contentType = "application/vnd.tcpdump.pcap";
    
    const size_t totalSize = file.size();
    server->sendHeader("Connection", "close");
    server->sendHeader("Content-Disposition", "attachment; filename=\"" + filename + "\"");
    server->setContentLength(totalSize);
    server->send(200, contentType, "");

    WiFiClient client = server->client();
    client.setNoDelay(true);

    static uint8_t buffer[1024];
    size_t sentTotal = 0;
    uint32_t lastProgress = millis();
    bool stalled = false;

    while (sentTotal < totalSize && client.connected()) {
        yield();

        size_t toRead = totalSize - sentTotal;
        if (toRead > sizeof(buffer)) {
            toRead = sizeof(buffer);
        }

        size_t readBytes = file.read(buffer, toRead);
        if (readBytes == 0) {
            break;
        }

        size_t offset = 0;
        while (offset < readBytes && client.connected()) {
            size_t chunk = readBytes - offset;
            int avail = client.availableForWrite();
            if (avail > 0 && chunk > static_cast<size_t>(avail)) {
                chunk = static_cast<size_t>(avail);
            }

            size_t written = client.write(buffer + offset, chunk);
            if (written == 0) {
                if (millis() - lastProgress > 8000) {
                    stalled = true;
                    break;
                }
                delay(1);
                yield();
                continue;
            }

            offset += written;
            sentTotal += written;
            lastProgress = millis();
        }

        if (stalled) {
            break;
        }
    }

    client.flush();
    client.stop();
    file.close();

    if (sentTotal > 0) {
        sessionTxBytes += sentTotal;
        sessionDownloadCount++;
    }

    logHeapStatusIfLow("after /download");
}

void FileServer::handleUpload() {
    logRequest(server, "REQ");
    if (uploadRejected) {
        server->sendHeader("Connection", "close");
        server->send(409, "text/plain", "Transfer in progress");
        uploadRejected = false;
        return;
    }
    server->sendHeader("Connection", "close");
    server->send(200, "text/plain", "OK");
}

void FileServer::handleUploadProcess() {
    HTTPUpload& upload = server->upload();
    
    if (upload.status == UPLOAD_FILE_START) {
        if (uploadActive) {
            uploadRejected = true;
            return;
        }
        uploadRejected = false;
        uploadActive = true;
        uploadLastProgress = millis();
        uploadBytesThisFile = 0;
        uploadDir = server->arg("dir");
        if (uploadDir.isEmpty()) uploadDir = "/";
        if (uploadDir != "/" && !uploadDir.endsWith("/")) uploadDir += "/";
        if (uploadDir == "/") uploadDir = "";  // Root doesn't need slash prefix
        
        // Security: prevent directory traversal
        String filename = upload.filename;
        if (filename.indexOf("..") >= 0 || uploadDir.indexOf("..") >= 0) {
            uploadRejected = true;
            uploadActive = false;
            return;
        }
        
        uploadPath = uploadDir + "/" + filename;
        if (uploadPath.startsWith("//")) uploadPath = uploadPath.substring(1);
        logHeapStatusIfLow("before upload");
        
        uploadFile = SD.open(uploadPath, FILE_WRITE);
        if (!uploadFile) {
            uploadRejected = true;
            uploadActive = false;
        }
    } else if (upload.status == UPLOAD_FILE_WRITE) {
        if (uploadFile) {
            uploadFile.write(upload.buf, upload.currentSize);
            sessionRxBytes += upload.currentSize;
            uploadLastProgress = millis();
            uploadBytesThisFile += upload.currentSize;
            yield();
        }
    } else if (upload.status == UPLOAD_FILE_END) {
        if (uploadFile) {
            uploadFile.close();
            sessionUploadCount++;
        }
        uploadActive = false;
        uploadRejected = false;
    } else if (upload.status == UPLOAD_FILE_ABORTED) {
        // Client disconnected or error - close file to prevent leak
        if (uploadFile) {
            uploadFile.close();
        }
        uploadActive = false;
        uploadRejected = false;
    }
}

// Recursive delete helper - properly handles nested directories
bool FileServer::deletePathRecursive(const String& path) {
    File f = SD.open(path);
    if (!f) {
        return false;
    }
    
    bool isDir = f.isDirectory();
    f.close();
    
    if (!isDir) {
        bool ok = SD.remove(path);
        return ok;
    }
    
    // It's a directory - delete all contents first (depth-first)
    File dir = SD.open(path);
    if (!dir) {
        return false;
    }
    
    File entry = dir.openNextFile();
    while (entry) {
        String entryPath = path + "/" + String(entry.name());
        bool entryIsDir = entry.isDirectory();
        entry.close();
        
        if (entryIsDir) {
            // Recurse into subdirectory
            if (!deletePathRecursive(entryPath)) {
                dir.close();
                return false;
            }
        } else {
            if (!SD.remove(entryPath)) {
                dir.close();
                return false;
            }
        }
        entry = dir.openNextFile();
    }
    dir.close();
    
    // Now remove the empty directory
    bool ok = SD.rmdir(path);
    return ok;
}

void FileServer::handleDelete() {
    String path = server->arg("f");
    logRequest(server, "REQ");
    if (isTransferBusy()) {
        sendBusyResponse(server);
        return;
    }
    if (path.isEmpty()) {
        server->sendHeader("Connection", "close");
        server->send(400, "text/plain", "Missing path");
        return;
    }
    
    // Security: prevent directory traversal
    if (path.indexOf("..") >= 0) {
        server->sendHeader("Connection", "close");
        server->send(400, "text/plain", "Invalid path");
        return;
    }
    
    bool success = deletePathRecursive(path);
    
    if (success) {
        server->sendHeader("Connection", "close");
        server->send(200, "text/plain", "Deleted");
        } else {
        server->sendHeader("Connection", "close");
        server->send(500, "text/plain", "Delete failed");
        }
}

void FileServer::handleBulkDelete() {
    logRequest(server, "REQ");
    if (isTransferBusy()) {
        sendBusyResponse(server);
        return;
    }
    // Read JSON body
    if (!server->hasArg("plain")) {
        server->sendHeader("Connection", "close");
        server->send(400, "application/json", "{\"error\":\"Missing body\"}");
        return;
    }
    
    String body = server->arg("plain");
    
    // Simple JSON parsing for {"paths":["path1","path2",...]}
    // Using manual parsing to avoid ArduinoJson dependency in this module
    int deleted = 0;
    int failed = 0;
    
    int idx = body.indexOf("\"paths\"");
    if (idx < 0) {
        server->sendHeader("Connection", "close");
        server->send(400, "application/json", "{\"error\":\"Missing paths array\"}");
        return;
    }
    
    int arrStart = body.indexOf('[', idx);
    int arrEnd = body.indexOf(']', arrStart);
    if (arrStart < 0 || arrEnd < 0) {
        server->sendHeader("Connection", "close");
        server->send(400, "application/json", "{\"error\":\"Invalid paths array\"}");
        return;
    }
    
    String arrContent = body.substring(arrStart + 1, arrEnd);
    
    // Parse each path from the array
    int pos = 0;
    while (pos < (int)arrContent.length()) {
        int quoteStart = arrContent.indexOf('"', pos);
        if (quoteStart < 0) break;
        
        int quoteEnd = arrContent.indexOf('"', quoteStart + 1);
        if (quoteEnd < 0) break;
        
        String path = arrContent.substring(quoteStart + 1, quoteEnd);
        pos = quoteEnd + 1;
        
        // Security check
        if (path.indexOf("..") >= 0) {
            failed++;
            continue;
        }
        
        if (deletePathRecursive(path)) {
            deleted++;
            } else {
            failed++;
        }
        
        yield();  // Feed watchdog during bulk operations
    }
    
    String response = "{\"deleted\":" + String(deleted) + ",\"failed\":" + String(failed) + "}";
    server->sendHeader("Connection", "close");
    server->send(200, "application/json", response);
    }

void FileServer::handleMkdir() {
    String path = server->arg("f");
    logRequest(server, "REQ");
    if (isTransferBusy()) {
        sendBusyResponse(server);
        return;
    }
    if (path.isEmpty()) {
        server->sendHeader("Connection", "close");
        server->send(400, "text/plain", "Missing path");
        return;
    }
    
    // Security: prevent directory traversal
    if (path.indexOf("..") >= 0) {
        server->sendHeader("Connection", "close");
        server->send(400, "text/plain", "Invalid path");
        return;
    }
    
    if (SD.mkdir(path)) {
        server->sendHeader("Connection", "close");
        server->send(200, "text/plain", "Created");
        } else {
        server->sendHeader("Connection", "close");
        server->send(500, "text/plain", "Create folder failed");
        }
}

void FileServer::handleRename() {
    String oldPath = server->arg("old");
    String newPath = server->arg("new");
    logRequest(server, "REQ");
    if (isTransferBusy()) {
        sendBusyResponse(server);
        return;
    }
    
    if (oldPath.isEmpty() || newPath.isEmpty()) {
        server->sendHeader("Connection", "close");
        server->send(400, "application/json", "{\"success\":false,\"error\":\"Missing path\"}");
        return;
    }
    
    // Security: prevent directory traversal
    if (oldPath.indexOf("..") >= 0 || newPath.indexOf("..") >= 0) {
        server->sendHeader("Connection", "close");
        server->send(400, "application/json", "{\"success\":false,\"error\":\"Invalid path\"}");
        return;
    }
    
    if (SD.rename(oldPath, newPath)) {
        server->sendHeader("Connection", "close");
        server->send(200, "application/json", "{\"success\":true}");
    } else {
        server->sendHeader("Connection", "close");
        server->send(500, "application/json", "{\"success\":false,\"error\":\"Rename failed\"}");
        }
}

bool FileServer::copyFileChunked(const String& srcPath, const String& dstPath) {
    // Copy a file in chunks to limit heap usage and avoid large temporary allocations.
    // Use a smaller chunk size than the default 4 KiB to reduce the chance of fragmenting
    // the heap. Allocate the buffer with heap_caps_malloc() so that allocation can
    // pull from the largest available block (8‑bit internal heap) and fail gracefully.

    File src = SD.open(srcPath, FILE_READ);
    if (!src) {
        return false;
    }

    File dst = SD.open(dstPath, FILE_WRITE);
    if (!dst) {
        src.close();
        return false;
    }

    // Use a larger 4 KiB copy buffer to balance performance and memory usage. Allocate
    // with standard malloc to avoid heap fragmentation issues; the Arduino heap
    // allocator will pull from available 8‑bit internal RAM. 4 KiB is the original
    // upstream chunk size and works well for SD cards.
    const size_t COPY_CHUNK_SIZE = 4096;
    uint8_t* buf = (uint8_t*)malloc(COPY_CHUNK_SIZE);
    if (!buf) {
        src.close();
        dst.close();
        return false;
    }

    bool success = true;
    size_t total = 0;
    while (src.available()) {
        size_t bytesToRead = COPY_CHUNK_SIZE;
        if (bytesToRead > src.available()) {
            bytesToRead = src.available();
        }
        size_t bytesRead = src.read(buf, bytesToRead);
        if (bytesRead == 0) {
            break;
        }
        if (dst.write(buf, bytesRead) != bytesRead) {
            success = false;
            break;
        }
        total += bytesRead;
        yield();  // Feed watchdog during long copies
    }

    free(buf);
    src.close();
    dst.close();

    if (!success) {
        // Clean up partial copy on failure to avoid orphan files consuming space.
        SD.remove(dstPath);
    }
    return success;
}

bool FileServer::copyPathRecursive(const String& srcPath, const String& dstPath) {
    File src = SD.open(srcPath);
    if (!src) {
        return false;
    }
    
    if (src.isDirectory()) {
        src.close();
        
        if (!SD.mkdir(dstPath)) {
            return false;
        }
        
        File dir = SD.open(srcPath);
        File entry;
        while ((entry = dir.openNextFile())) {
            String name = entry.name();
            // Extract just filename from full path
            int lastSlash = name.lastIndexOf('/');
            if (lastSlash >= 0) name = name.substring(lastSlash + 1);
            
            String newSrc = srcPath + "/" + name;
            String newDst = dstPath + "/" + name;
            
            entry.close();
            if (!copyPathRecursive(newSrc, newDst)) {
                dir.close();
                return false;
            }
            yield();
        }
        dir.close();
        return true;
    } else {
        src.close();
        return copyFileChunked(srcPath, dstPath);
    }
}

void FileServer::handleCopy() {
    logRequest(server, "REQ");
    if (isTransferBusy()) {
        sendBusyResponse(server);
        return;
    }
    if (!server->hasArg("plain")) {
        server->sendHeader("Connection", "close");
        server->send(400, "application/json", "{\"success\":false,\"error\":\"No body\"}");
        return;
    }
    
    String body = server->arg("plain");
    // Parse dest folder
    int destIdx = body.indexOf("\"dest\"");
    if (destIdx < 0) {
        server->sendHeader("Connection", "close");
        server->send(400, "application/json", "{\"success\":false,\"error\":\"Missing dest\"}");
        return;
    }
    int destStart = body.indexOf('"', body.indexOf(':', destIdx)) + 1;
    int destEnd = body.indexOf('"', destStart);
    String destDir = body.substring(destStart, destEnd);
    // Security check
    if (destDir.indexOf("..") >= 0) {
        server->sendHeader("Connection", "close");
        server->send(400, "application/json", "{\"success\":false,\"error\":\"Invalid dest\"}");
        return;
    }
    
    // Parse files array
    int filesIdx = body.indexOf("\"files\"");
    if (filesIdx < 0) {
        server->sendHeader("Connection", "close");
        server->send(400, "application/json", "{\"success\":false,\"error\":\"Missing files\"}");
        return;
    }
    
    int arrStart = body.indexOf('[', filesIdx);
    int arrEnd = body.indexOf(']', arrStart);
    if (arrStart < 0 || arrEnd < 0) {
        server->sendHeader("Connection", "close");
        server->send(400, "application/json", "{\"success\":false,\"error\":\"Invalid files array\"}");
        return;
    }
    
    String arrContent = body.substring(arrStart + 1, arrEnd);
    int copied = 0;
    int failed = 0;
    
    int pos = 0;
    while (pos < (int)arrContent.length()) {
        int quoteStart = arrContent.indexOf('"', pos);
        if (quoteStart < 0) break;
        
        int quoteEnd = arrContent.indexOf('"', quoteStart + 1);
        if (quoteEnd < 0) break;
        
        String srcPath = arrContent.substring(quoteStart + 1, quoteEnd);
        pos = quoteEnd + 1;
        
        if (srcPath.indexOf("..") >= 0) {
            failed++;
            continue;
        }
        
        // Extract filename from source path
        int lastSlash = srcPath.lastIndexOf('/');
        String filename = (lastSlash >= 0) ? srcPath.substring(lastSlash + 1) : srcPath;
        String dstPath = (destDir == "/") ? "/" + filename : destDir + "/" + filename;
        
        if (copyPathRecursive(srcPath, dstPath)) {
            copied++;
        } else {
            failed++;
        }
        
        yield();
    }
    
    String response = "{\"success\":true,\"copied\":" + String(copied) + ",\"failed\":" + String(failed) + "}";
    server->sendHeader("Connection", "close");
    server->send(200, "application/json", response);
}

void FileServer::handleMove() {
    logRequest(server, "REQ");
    if (isTransferBusy()) {
        sendBusyResponse(server);
        return;
    }
    if (!server->hasArg("plain")) {
        server->sendHeader("Connection", "close");
        server->send(400, "application/json", "{\"success\":false,\"error\":\"No body\"}");
        return;
    }
    
    String body = server->arg("plain");
    // Parse dest folder
    int destIdx = body.indexOf("\"dest\"");
    if (destIdx < 0) {
        server->sendHeader("Connection", "close");
        server->send(400, "application/json", "{\"success\":false,\"error\":\"Missing dest\"}");
        return;
    }
    int destStart = body.indexOf('"', body.indexOf(':', destIdx)) + 1;
    int destEnd = body.indexOf('"', destStart);
    String destDir = body.substring(destStart, destEnd);
    if (destDir.indexOf("..") >= 0) {
        server->sendHeader("Connection", "close");
        server->send(400, "application/json", "{\"success\":false,\"error\":\"Invalid dest\"}");
        return;
    }
    
    // Parse files array
    int filesIdx = body.indexOf("\"files\"");
    if (filesIdx < 0) {
        server->sendHeader("Connection", "close");
        server->send(400, "application/json", "{\"success\":false,\"error\":\"Missing files\"}");
        return;
    }
    
    int arrStart = body.indexOf('[', filesIdx);
    int arrEnd = body.indexOf(']', arrStart);
    if (arrStart < 0 || arrEnd < 0) {
        server->sendHeader("Connection", "close");
        server->send(400, "application/json", "{\"success\":false,\"error\":\"Invalid files array\"}");
        return;
    }
    
    String arrContent = body.substring(arrStart + 1, arrEnd);
    int moved = 0;
    int failed = 0;
    
    int pos = 0;
    while (pos < (int)arrContent.length()) {
        int quoteStart = arrContent.indexOf('"', pos);
        if (quoteStart < 0) break;
        
        int quoteEnd = arrContent.indexOf('"', quoteStart + 1);
        if (quoteEnd < 0) break;
        
        String srcPath = arrContent.substring(quoteStart + 1, quoteEnd);
        pos = quoteEnd + 1;
        
        if (srcPath.indexOf("..") >= 0) {
            failed++;
            continue;
        }
        
        // Extract filename from source path
        int lastSlash = srcPath.lastIndexOf('/');
        String filename = (lastSlash >= 0) ? srcPath.substring(lastSlash + 1) : srcPath;
        String dstPath = (destDir == "/") ? "/" + filename : destDir + "/" + filename;
        
        // Try SD.rename first (fast, atomic)
        if (SD.rename(srcPath, dstPath)) {
            moved++;
        } else if (copyPathRecursive(srcPath, dstPath)) {
            // Fallback to copy+delete for cross-filesystem moves
            if (deletePathRecursive(srcPath)) {
                moved++;
            } else {
                // Rollback: delete the copy
                deletePathRecursive(dstPath);
                failed++;
            }
        } else {
            failed++;
        }
        
        yield();
    }
    
    String response = "{\"success\":true,\"moved\":" + String(moved) + ",\"failed\":" + String(failed) + "}";
    server->sendHeader("Connection", "close");
    server->send(200, "application/json", response);
}

void FileServer::handleNotFound() {
    logRequest(server, "REQ");
    server->sendHeader("Connection", "close");
    server->send(404, "text/plain", "Not found");
}

const char* FileServer::getHTML() {
    return HTML_TEMPLATE;
}

